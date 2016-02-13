/*
 * Util functions
 *
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

#include "usart.h"
#include "util.h"

volatile uint32_t msTicks = 0;

void SysTick_Handler(void) {
	msTicks++;
}

void delay_ms(uint32_t ms) {
	uint32_t now = msTicks;
	while ((msTicks-now) < ms);
}

void set_with_mask(volatile uint32_t *reg, uint32_t mask, uint32_t val)
{
	*reg &= ~mask;
	*reg |= val & mask;
}

void u32_to_str(uint32_t val, char *buf)
{
	int i = 8;
	while (i--) {
		if ((val & 0xf) <= 9)
			buf[i] = (val & 0xf) + '0';
		else
			buf[i] = (val & 0xf) + ('a' - 10);
		val >>= 4;
	}
}

void dump_mem(const void *addr, size_t len)
{
	const char *caddr = addr;
	char c;
	char align = ((uint32_t)caddr) & 0xf;
	char buf[] = "deadbeef:";

	len += ((uint32_t)caddr) & 0xf;


	u32_to_str((uint32_t)caddr, buf);
	usart_send(buf, 9);
	align = 1 + (align * 3);
	while (align--)
		usart_send(" ", 1);
	usart_send("v", 1);
	usart_send("\r\n", 2);

	caddr = (const char *)((uint32_t)caddr & ~0xf);

	while (len--) {
		if (!((uint32_t)caddr & 0xf)) {
			u32_to_str((uint32_t)caddr, buf);
			usart_send(buf, 9);
		}

		c = *caddr;
		caddr++;

		buf[0] = ' ';
		if ((c & 0xf) <= 9)
			buf[2] = (c & 0xf) + '0';
		else
			buf[2] = (c & 0xf) + ('a' - 10);
		c >>= 4;
		if ((c & 0xf) <= 9)
			buf[1] = (c & 0xf) + '0';
		else
			buf[1] = (c & 0xf) + ('a' - 10);
		usart_send(buf, 3);

		if (!len || !((uint32_t)caddr & 0xf))
			usart_send("\r\n", 2);
	}
}
