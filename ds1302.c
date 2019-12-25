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

#define RTC_DDR  LPC_GPIO->DIR[1]
#define RTC_PORT LPC_GPIO->PIN[1]
#define RTC_PIN  LPC_GPIO->PIN[1]
#define RTC_CE   26
#define RTC_IO   27
#define RTC_SCK  20
#define DELAY_US 5

#define RTC_READ            (1 << 0)
#define RTC_WRITE           (0 << 0)
#define RTC_CMD             (1 << 7)
#define RTC_RAM             (1 << 6)
#define RTC_CLOCK           (0 << 6)
#define RTC_ADDR(__addr) ((__addr & 0x1F) << 1)
#define RTC_CMD_CLOCK_BURST (RTC_ADDR(0x1F) | RTC_CLOCK | RTC_CMD)
#define RTC_CMD_RAM_BURST   (RTC_ADDR(0x1F) | RTC_RAM | RTC_CMD)

/* I wouldn't trust this to be too accurate... */
static void _delay_us(uint32_t us)
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

static void rtc_cmd(uint8_t cmd)
{
	RTC_PORT &= ~((1 << RTC_SCK) | (1 << RTC_IO));
	RTC_DDR |= (1 << RTC_IO);
	RTC_PORT |= (1 << RTC_CE);
	shift_out(cmd);

	if (cmd & RTC_READ) {
		RTC_PORT &= ~(1 << RTC_IO);
		RTC_DDR &= ~(1 << RTC_IO);
	}
}

static void rtc_read(uint8_t *data, unsigned int len)
{
	unsigned int i;
	for (i = 0; i < len; i++) {
		shift_in((uint8_t *)data + i);
	}
	RTC_PORT &= ~(1 << RTC_CE);
	_delay_us(DELAY_US);
}

static void rtc_write(uint8_t *data, unsigned int len)
{
	unsigned int i;
	for (i = 0; i < len; i++) {
		shift_out(((uint8_t *)data)[i]);
	}
	RTC_PORT &= ~(1 << RTC_CE);
	_delay_us(DELAY_US);
}

uint8_t dec_to_bcd(uint8_t dec) {
	return ((dec / 10) << 4) | (dec % 10);
}

uint8_t bcd_to_dec(uint8_t bcd) {
	return ((bcd >> 4) * 10) + (bcd & 0xf);
}

void rtc_init()
{
	RTC_DDR = (1 << RTC_CE) | (1 << RTC_SCK);
}

void rtc_read_date(struct rtc_date *data)
{
	rtc_cmd(RTC_CMD_CLOCK_BURST | RTC_READ);
	rtc_read((uint8_t *)data, sizeof(*data));
}

void rtc_write_date(struct rtc_date *data)
{
	rtc_cmd(RTC_CMD_CLOCK_BURST | RTC_WRITE);
	rtc_write((uint8_t *)data, sizeof(*data));
}

void rtc_read_ram(uint8_t *data, unsigned int len)
{
	rtc_cmd(RTC_CMD_RAM_BURST | RTC_READ);
	rtc_read(data, len);
}

void rtc_write_ram(uint8_t *data, unsigned int len)
{
	rtc_cmd(RTC_CMD_RAM_BURST | RTC_WRITE);
	rtc_write(data, len);
}

uint8_t rtc_peek(uint8_t addr)
{
	uint8_t data;
	uint8_t cmd = RTC_ADDR(addr) | RTC_CMD | RTC_READ | RTC_RAM;
	rtc_cmd(cmd);
	rtc_read(&data, 1);

	return data;
}

void rtc_poke(uint8_t addr, uint8_t data)
{
	uint8_t cmd = RTC_ADDR(addr) | RTC_CMD | RTC_WRITE | RTC_RAM;
	rtc_cmd(cmd);
	rtc_write(&data, 1);
}

char *rtc_date_to_str(struct rtc_date *date)
{
	static char buf[20];
	uint8_t tmp;
	int idx = 0;

	buf[idx++] = '2';
	buf[idx++] = '0';
	tmp = date->year;
	buf[idx++] = (tmp >> 4) + '0';
	buf[idx++] = (tmp & 0xf) + '0';
	buf[idx++] = '-';

	tmp = date->month;
	buf[idx++] = (tmp >> 4) + '0';
	buf[idx++] = (tmp & 0xf) + '0';
	buf[idx++] = '-';

	tmp = date->date;
	buf[idx++] = (tmp >> 4) + '0';
	buf[idx++] = (tmp & 0xf) + '0';
	buf[idx++] = '-';

	tmp = date->hours;
	buf[idx++] = (tmp >> 4) + '0';
	buf[idx++] = (tmp & 0xf) + '0';
	buf[idx++] = ':';

	tmp = date->minutes;
	buf[idx++] = (tmp >> 4) + '0';
	buf[idx++] = (tmp & 0xf) + '0';
	buf[idx++] = ':';

	tmp = date->seconds;
	buf[idx++] = (tmp >> 4) + '0';
	buf[idx++] = (tmp & 0xf) + '0';
	buf[idx++] = '\0';

	return buf;
}
