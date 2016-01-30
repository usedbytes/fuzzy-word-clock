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
#ifndef __USART_H__
#define __USART_H__

#include <stddef.h>

#include "LPC11Uxx.h"

enum usart_select {
	USART_BOOTLOADER = 0,
	USART_PIO1_26_27,
};

/* Initialise the USART
 *
 * Set up the USART on the selected pins, 115200 8n1
 */
void usart_init(enum usart_select pins);

/* Send data over the USART
 *
 * Blocks until all of the message is in the TX FIFO, flushing any pending
 * transmission first (and waiting for each 16 byte chunk to be transmitted
 * if len > 16)
 */
void usart_send(char *buffer, size_t len);

/* Receive data from the USART
 *
 * Attempt to receive 'len' bytes into 'buffer'. If a timeout occurs, return
 * the number of characters received. The RX FIFO is not flushed before
 * starting.
 * Note: This will block until there is at least 1 character in the FIFO!
 */
size_t usart_recv(char *buffer, size_t len);

#endif /* __USART_H__ */
