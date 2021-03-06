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

struct rtc_date {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t date;
	uint8_t month;
	uint8_t day;
	uint8_t year;
	uint8_t write_protect;
} __attribute__ ((aligned(4)));

/* Initialise the RTC port */
void rtc_init(void);
/* Burst read/write clock memory */
void rtc_read_date(struct rtc_date *data);
void rtc_write_date(struct rtc_date *data);
/* Burst read/write RAM */
void rtc_read_ram(uint8_t *data, unsigned int len);
void rtc_write_ram(uint8_t *data, unsigned int len);
/* Peek/poke RAM location */
uint8_t rtc_peek(uint8_t addr);
void rtc_poke(uint8_t addr, uint8_t data);

uint8_t dec_to_bcd(uint8_t dec);
uint8_t bcd_to_dec(uint8_t bcd);

char *rtc_date_to_str(struct rtc_date *date);
