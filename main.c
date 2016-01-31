#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "ds1302.h"
#include "lpd8806.h"
#include "pwm.h"
#include "segment_display.h"
#include "spi.h"
#include "usart.h"
#include "util.h"

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

struct rtc_date date;

struct pwm_16b *pwm;
uint32_t *fade_out;
uint32_t *fade_in;
uint32_t onscreen = 0;

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

static void do_fade(void)
{
	uint8_t level = 0xFF;
	do {
		level--;
		pwm_set(pwm, 0, level);
		pwm_set(pwm, 1, ~level);
		delay_ms(18);
	} while (level);
}

void display(uint32_t sentence)
{
	*fade_in = sentence & ~onscreen;
	*fade_out = onscreen & ~sentence;
	onscreen = sentence;
	pwm_set(pwm, 0, 0xFF);
	pwm_set(pwm, 1, 0);
	pwm_start(pwm);
	do_fade();
	pwm_stop(pwm);
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

#define N_LEDS 26
#define MIN_DELAY 18
#define DELAY_DELTA 80
#define HUE_STEP 2
#define DECAY 1

#define GREEN 0x0F0000
#define RED 0x000F00
#define BLUE 0x00000F

int main(void) {
	struct spi_dev *spidev;
	uint8_t magic;
	uint32_t fb[3] = { GREEN, BLUE, RED };
	struct segment_display *disp;

	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	//setup_leds();
	rtc_init();
	spidev = spi_init(SSP0, FRAMESZ_8BIT);
	usart_init(USART_BOOTLOADER);
	pwm = pwm_init(0, &LPC_GPIO->PIN[0],
	               (uint32_t[]){ 0x3 << 9, 0x3 << 11, 0x3 << 13 });
	fade_out = &pwm->pins[0];
	fade_in = &pwm->pins[1];

	usart_send("Hello", 5);

	lpd8806_init(spidev, 26);
	//lpd8806_update(spidev, fb, 3);

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

	disp = display_init(spidev);

	while (1) {
		uint32_t count;
		for (count = 0; count < 256; count++) {
			display_transition(disp, count, (count + 1) & 0xFF);
		}
	}

	return 0;
}

