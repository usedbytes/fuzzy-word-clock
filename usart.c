/*
 * Simple USART implementation for the LPC11U24
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

#define LSR_RDR   (1 << 0)
#define LSR_OE    (1 << 1)
#define LSR_PE    (1 << 2)
#define LSR_FE    (1 << 3)
#define LSR_BI    (1 << 4)
#define LSR_THRE  (1 << 5)
#define LSR_TEMT  (1 << 6)
#define LSR_RXFE  (1 << 7)
#define LSR_TXERR (1 << 8)

#define INTID_RDA 0x2
#define INTID_CTI 0x6

struct usart_pincfg {
	__IO uint32_t *regs[2];
	uint32_t vals[2];
};

static const struct usart_pincfg lpc11u24_usart_pincfg[] = {
	[USART_BOOTLOADER] = {
		{ &LPC_IOCON->PIO0_18, &LPC_IOCON->PIO0_19 },
		{ 0x11, 0x11 },
	},
	[USART_PIO1_26_27] = {
		{ &LPC_IOCON->PIO1_26, &LPC_IOCON->PIO1_27 },
		{ 0x12, 0x12 },
	},
};

void usart_init(enum usart_select pins)
{
	const struct usart_pincfg *pincfg = &lpc11u24_usart_pincfg[pins];
	/* Turn on the clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 12);
	LPC_SYSCON->UARTCLKDIV = 1;

	/* Set up the pins */
	*pincfg->regs[0] = pincfg->vals[0]; /* RXD */
	*pincfg->regs[1] = pincfg->vals[1]; /* TXD */

	/*
	Baud rate = PCLK / (16 x [DLM:DLL] x (1 + DivAddVal/MulVal))
	Baud rate x 16 = PCLK / ([DLM:DLL] x (1 + DivAddVal/MulVal))
	[DLM:DLL] x (1 + DivAddVal/DivMulVal) = PCLK / Baud x 16

	[DLM:DLL] = PCLK / 16 x Baud
	(DivAddVal / DivMulVal) x 16 x baud = PCLK - ([DLM:DLL] x 16 x baud)
	(DivAddVal / DivMulVal) = PCLK / (16 x Baud) - [DLM:DLL]
	*/

	/* 115200 baud at 48 MHz USART_PCLK (115089.514 baud) */
	LPC_USART->LCR |= 0x80;
	LPC_USART->DLM = 0;
	LPC_USART->DLL = 17;
	LPC_USART->FDR |= (15 << 4) | (8 << 0);
	LPC_USART->LCR &= ~0x80;

	/* 8n1 */
	LPC_USART->LCR |= 0x3;

	/* FIFOEN, 4 byte trigger level */
	LPC_USART->FCR |= (1 << 6) | 1;

	/* Enable Tx */
	LPC_USART->TER |= 0x80;

	/*
	 * Enable the RDA/CTI interrupt flags. We don't enable the IRQ because
	 * we'll poll the flags directly
	 */
	LPC_USART->IER |= 0x1;
}

void usart_send(const char *buffer, size_t len)
{
	do {
		size_t i;
		/* Flush the last transmission */
		while (!(LPC_USART->LSR & LSR_TEMT));

		/* Send the next 16-byte chunk */
		for (i = 0; (i < len) && (i < 16); i++) {
			LPC_USART->THR = buffer[i];
		}
		len -= i;
		buffer = buffer + i;
	} while (len > 0);
}

void usart_print(const char *str)
{
	int i = 0;
	while (str[i++]);

	usart_send(str, i);
}

size_t usart_recv(char *buffer, size_t len)
{
	size_t recv = 0;

	while (recv < len) {
		uint32_t iid;
		uint32_t i;

		/* Wait for characters or a timeout */
		do {
			iid = (LPC_USART->IIR >> 1) & 0x7;
		} while ((iid != INTID_RDA) && (iid != INTID_CTI));

		switch (iid) {
		case INTID_RDA:
			/* Drain the FIFO (trigger level set to 4) */
			i = len - recv;
			if (i > 4)
				i = 4;
			while (i) {
				buffer[recv++] = LPC_USART->RBR;
				i--;
			}
			break;
		case INTID_CTI:
			/* EOM - Read the leftovers */
			buffer[recv++] = LPC_USART->RBR;
			break;
		}

		/* Nothing left to read */
		if (!(LPC_USART->LSR & LSR_RDR))
			break;
	}
	return recv;
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

void print_u32(uint32_t val)
{
	static char buf[] = "deadbeef\r\n";
	u32_to_str(val, buf);
	usart_print(buf);
}
