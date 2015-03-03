#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"
#include "usart.h"
#include "ds1302.h"

#define MAGIC_MARKER 0x42
#define N_MEALS     5
#define N_COURSES   3
#define BREAKFAST   0
#define LUNCH       1
#define HOME        2
#define DINNER      3
#define BED         4
#define NEARLY      5
#define PAST        6
#define ITS_TIME    7
#define LED(x)      (1 << ((x) + 8))
#define NEARLY_THRESH 8
#define PAST_THRESH   8

volatile uint32_t msTicks = 0;
struct rtc_date date;

void SysTick_Handler(void) {
	msTicks++;
}

void delay_ms(uint32_t ms) {
	uint32_t now = msTicks;
	while ((msTicks-now) < ms)
		;
}

struct pwm_16b {
	LPC_CTxxBx_Type *timer;
	__IO uint32_t *port;
	uint32_t pins[3];
	uint16_t vals[3];
};

struct pwm_16b pwm0 = {
	LPC_CT16B0,
	&LPC_GPIO->PIN[0],
	{ 0x3 << 9, 0x3 << 11, 0x3 << 13 },
	{ 0, 0, 0 },
};
uint32_t *fade_in = &pwm0.pins[0];
uint32_t *fade_out = &pwm0.pins[1];
uint32_t onscreen = 0;

void TIMER_16_0_Handler(void) {
	uint32_t status = LPC_CT16B0->IR;
	bool overflow = status & (1 << 3);
	int channel;

	/* Turn all channels off if there was an overflow */
	if (overflow) {
		*pwm0.port &= ~(pwm0.pins[0] | pwm0.pins[1] | pwm0.pins[2]);
		pwm0.timer->MR[0] = pwm0.vals[0];
		pwm0.timer->MR[1] = pwm0.vals[1];
		pwm0.timer->MR[2] = pwm0.vals[2];
	}

	/* Selectively turn them on if there was a match */
	for (channel = 0; channel < 3; channel++) {
		uint32_t val = pwm0.timer->MR[channel];

		/* Latch in the new value */
		//pwm0.timer->MR[channel] = pwm0.vals[channel];

		if (status & (1 << channel)) {
			if (overflow && (val >= 0x8000)) {
				/* Don't bother turning on because we missed our chance */
				continue;
			}
			*pwm0.port |= (pwm0.pins[channel]);
		}
	}
	LPC_CT16B0->IR = status;
	return;
}

void init_timer16_0()
{
	/* Turn on the clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 7);
	LPC_CT16B0->PR = 1;
	/* Use MR3 as overflow interupt */
	LPC_CT16B0->MR3 = 0x0;
	LPC_CT16B0->MCR = (1 << 9);
	LPC_CT16B0->TCR = 0x1;
	NVIC_SetPriority(TIMER_16_0_IRQn, 3);
	NVIC_EnableIRQ(TIMER_16_0_IRQn);
}

uint8_t do_write_cmd(void)
{
	uint8_t ret;
	char n;
	ret = usart_recv(&n, 1);
	if (ret != 1 || n > 8)
		return 1;
	ret = 0;
	while (ret < n)
		ret += usart_recv((char *)&date + ret, n - ret);
	rtc_write_date(&date);
	rtc_poke(0x0, MAGIC_MARKER);
	return 0;
}

uint8_t do_read_cmd(void)
{
	uint8_t ret;
	char n;
	ret = usart_recv(&n, 1);
	if (ret != 1 || n > 8)
		return 1;
	rtc_read_date(&date);
	usart_send("<", 1);
	usart_send((char *)&date, n);
	return 0;
}

void program_mode(void)
{
	uint8_t ret;
	char buf;
	for (;;) {
		usart_send(">", 1);
		ret = usart_recv(&buf, 1);
		switch (buf) {
		case '?':
			usart_send("fuzzyclk.v1.0\r\n", 15);
			ret = 0;
			break;
		case 'w':
			ret = do_write_cmd();
			break;
		case 'r':
			ret = do_read_cmd();
			break;
		case 'x':
			NVIC_SystemReset();
			/* WIll never get here */
		default:
			ret = 1;
			break;
		}
		if (ret)
			usart_send("ERR\r\n", 5);
		else if (!ret && buf == 'w')
			usart_send("OK\r\n", 4);
	}
}

static void pwm_start(void)
{
	LPC_CT16B0->TCR = 0x1;
}

static void pwm_stop(void)
{
	LPC_CT16B0->TCR = 0x0;
}

void pwm_set(struct pwm_16b *pwm, uint32_t channel, uint16_t value)
{
	if (value) {
		value = ~(value | 0xFF);
		pwm->vals[channel] = value;
		pwm->timer->MCR |= (1 << (channel * 3));
	} else {
		pwm->timer->MCR &= ~(1 << (channel * 3));
		*pwm->port &= ~pwm->pins[channel];
		pwm->vals[channel] = 0xffff;
	}
}

static void do_fade(void)
{
	uint8_t level = 255;
	do {
		level--;
		pwm_set(&pwm0, 0, (level << 8));
		pwm_set(&pwm0, 1, ~(level << 8));
		delay_ms(15);
	} while (level);
}

void display(uint32_t sentence)
{
	*fade_in = sentence & ~onscreen;
	*fade_out = onscreen & ~sentence;
	onscreen = sentence;
	pwm_set(&pwm0, 0, 0);
	pwm_set(&pwm0, 1, 0);
	pwm_start();
	do_fade();
	pwm_stop();
}

void set_with_mask(volatile uint32_t *reg, uint32_t mask, uint32_t val)
{
	*reg &= ~mask;
	*reg |= val & mask;
}

void setup_leds(void)
{
	/* Set all our LED pins as GPIO outputs */
	set_with_mask(&LPC_IOCON->PIO0_8, 0x3, 0x0);
	set_with_mask(&LPC_IOCON->PIO0_9, 0x3, 0x0);
	set_with_mask(&LPC_IOCON->SWCLK_PIO0_10, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->TDI_PIO0_11, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->TMS_PIO0_12, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->TDO_PIO0_13, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->TRST_PIO0_14, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->SWDIO_PIO0_15, 0x3, 0x1);
	LPC_GPIO->DIR[0] |= (0xFF << 8);
}

int main(void) {
	uint8_t magic;

	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	setup_leds();
	rtc_init();
	usart_init(USART_BOOTLOADER);
	init_timer16_0();

	usart_send("Hello", 5);

	/*
	magic = rtc_peek(0x0);
	if (magic != MAGIC_MARKER)
	{
		usart_send("Entering program mode...\r\n", 26);
		program_mode();
	}

	while (1) {
		delay_ms(1000);
		rtc_read_date(&date);
		usart_send((char *)&date, 8);
	}
	*/

	while (1) {
		uint32_t leds = 1;

		for (leds = 1; leds < 256; leds <<= 1) {
			display(LED(BREAKFAST) | LED(ITS_TIME));
			delay_ms(1000);
			display(LED(HOME) | LED(DINNER));
			delay_ms(1000);
		}
	}

	return 0;
}

