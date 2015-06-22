#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"
#include "usart.h"
#include "spi.h"
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
uint32_t *fade_out = &pwm0.pins[0];
uint32_t *fade_in = &pwm0.pins[1];
uint32_t onscreen = 0;

void TIMER_16_0_Handler(void) {
	uint32_t status = LPC_CT16B0->IR;
	bool overflow = status & (1 << 3);
	uint32_t state = LPC_GPIO->PIN[0];
	uint32_t to_clear = 0;
	uint32_t to_set = 0;
	int channel;

	/* Turn enabled channels on if there was an overflow */
	if (overflow) {
		for (channel = 0; channel < 3; channel++) {
			if (pwm0.vals[channel])
				to_set |= pwm0.pins[channel];
		}
	}

	/* Selectively turn them off if there was a match */
	for (channel = 0; channel < 3; channel++) {
		if (status & (1 << channel)) {
			to_clear |= pwm0.pins[channel];
			pwm0.timer->MR[channel] = pwm0.vals[channel];
		}
	}

	to_clear = ~to_clear;
	state |= to_set;
	state &= to_clear;
	LPC_GPIO->PIN[0] = state;

	LPC_CT16B0->IR = status;
	return;
}

void init_timer16_0()
{
	/* Turn on the clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 7);
	LPC_CT16B0->PR = 4;
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
	pwm0.timer->MR[0] = pwm0.vals[0];
	pwm0.timer->MR[1] = pwm0.vals[1];
	pwm0.timer->MR[2] = pwm0.vals[2];
	LPC_CT16B0->TCR = 0x1;
}

static void pwm_stop(void)
{
	LPC_CT16B0->TCR = 0x0;
}

void pwm_set(struct pwm_16b *pwm, uint32_t channel, uint8_t value)
{
	pwm->vals[channel] = value << 8;
	pwm->timer->MCR |= (1 << (channel * 3));
}

static void do_fade(void)
{
	uint8_t level = 0xFF;
	do {
		level--;
		pwm_set(&pwm0, 0, level);
		pwm_set(&pwm0, 1, ~level);
		delay_ms(18);
	} while (level);
}

void display(uint32_t sentence)
{
	*fade_in = sentence & ~onscreen;
	*fade_out = onscreen & ~sentence;
	onscreen = sentence;
	pwm_set(&pwm0, 0, 0xFF);
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

extern uint32_t hsvtorgb(unsigned char h, unsigned char s, unsigned char v);

/* Color like 0x00GGRRBB */
void send_color(struct spi_dev *spidev, uint32_t color)
{
	color |= 0x808080;
	spi_transfer(spidev, (uint8_t *)&color, NULL, 3);
}

void latch_strip(struct spi_dev *spidev)
{
	spi_transfer(spidev, NULL, NULL, 1);
}

#define N_LEDS 26
#define MIN_DELAY 18
#define DELAY_DELTA 80
#define HUE_STEP 2
#define DECAY 2

int main(void) {
	struct spi_dev *spidev;
	uint8_t magic;

	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	//setup_leds();
	rtc_init();
	spidev = spi_init(SSP0, FRAMESZ_8BIT);
	usart_init(USART_BOOTLOADER);
	init_timer16_0();

	usart_send("Hello", 5);

	unsigned char sat = 255;
	unsigned char val = 64;
	unsigned char hue = 0;

	uint8_t i, j = 0, active = 0;
	int dir = 1;
	int sin[] = {
		256, 225, 194, 165, 137, 110, 86, 64, 45, 29, 16, 7, 1, 0,
		1, 7, 16, 29, 45, 64, 86, 110, 137, 165, 194, 225, 256,
	};
	while (1) {
		for (i = 0; i < N_LEDS - 1; i++) {
			if (dir > 0)
				active = i;
			else
				active = N_LEDS - i - 1;

			for (j = 0; j < N_LEDS; j++) {
				uint32_t color = 0;
				int dist = (j - active) * dir;
				if (dist <= 0) {
					dist = dist < 0 ? -dist : dist;
					color = hsvtorgb(hue - (HUE_STEP * dist), sat, val >> (DECAY * dist));
				}
				send_color(spidev, color);
			}
			latch_strip(spidev);
			hue += HUE_STEP;

			uint32_t delay = MIN_DELAY + ((DELAY_DELTA * sin[i]) >> 8);
			delay_ms(delay);
		}
		dir *= -1;
	}

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

