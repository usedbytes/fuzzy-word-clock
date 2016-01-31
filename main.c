#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "ds1302.h"
#include "lpd8806.h"
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

int main(void) {
	struct spi_dev *spidev;
	struct segment_display *disp;

	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	usart_init(USART_BOOTLOADER);

	usart_send("Hello", 5);

	rtc_init();
	spidev = spi_init(SSP0, FRAMESZ_8BIT);
	lpd8806_init(spidev, 26);
	disp = display_init(spidev);

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
		uint32_t count;
		for (count = 0; count < 256; count++) {
			display_transition(disp, count, (count + 1) & 0xFF);
		}
	}

	return 0;
}

