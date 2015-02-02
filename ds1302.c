/*
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>

#include "LPC11Uxx.h"
#include "ds1302.h"

#define RTC_DDR  LPC_GPIO->DIR[0]
#define RTC_PORT LPC_GPIO->PIN[0]
#define RTC_PIN  LPC_GPIO->PIN[0]
#define RTC_CE   13
#define RTC_IO   12
#define RTC_SCK  11
#define DELAY_US 5

/* I wouldn't trust this to be too accurate... */
void _delay_us(uint32_t us)
{
	while (us) {
		uint32_t i;
		/* Assumes 48 MHz clock */
		for (i = 0; i < 48; i++) {
			asm("nop");
		}
		us--;
	}
}

static void shift_in(uint8_t *data)
{
	uint8_t i = 0x01;
	*data = 0;
	while (i) {
		RTC_PORT &= ~(1 << RTC_SCK);
		_delay_us(DELAY_US);
		if (RTC_PIN & (1 << RTC_IO))
			*data |= i;
		RTC_PORT |= (1 << RTC_SCK);
		_delay_us(DELAY_US);
		i <<= 1;
	}
}

static void shift_out(uint8_t data)
{
	uint8_t i = 0x01;
	while (i) {
		RTC_PORT &= ~(1 << RTC_SCK);
		if (data & i)
			RTC_PORT |= (1 << RTC_IO);
		else
			RTC_PORT &= ~(1 << RTC_IO);
		_delay_us(DELAY_US);
		RTC_PORT |= (1 << RTC_SCK);
		_delay_us(DELAY_US);
		i <<= 1;
	}
}

void rtc_init()
{
	RTC_DDR = (1 << RTC_CE) | (1 << RTC_SCK);
}

void rtc_read_date(struct rtc_date *data)
{
	uint8_t i;
	RTC_PORT &= ~((1 << RTC_SCK) | (1 << RTC_IO));
	RTC_DDR |= (1 << RTC_IO);
	RTC_PORT |= (1 << RTC_CE);
	shift_out(0xBF);

	RTC_PORT &= ~(1 << RTC_IO);
	RTC_DDR &= ~(1 << RTC_IO);
	for (i = 0; i < 8; i++) {
		shift_in((uint8_t *)data + i);
	}
	RTC_PORT &= ~(1 << RTC_CE);
}

void rtc_write_date(struct rtc_date *data)
{
	uint8_t i;
	RTC_PORT &= ~((1 << RTC_SCK) | (1 << RTC_IO));
	RTC_DDR |= (1 << RTC_IO);
	RTC_PORT |= (1 << RTC_CE);
	shift_out(0xBE);

	for (i = 0; i < 8; i++) {
		shift_out(((uint8_t *)data)[i]);
	}
	RTC_PORT &= ~(1 << RTC_CE);
}

