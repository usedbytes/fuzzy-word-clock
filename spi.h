/*
 * Simple SPI implementation for the LPC11U24
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

#include <stddef.h>

#include "LPC11Uxx.h"

enum ssp_select {
	SSP0 = 0,
	/* TODO: SSP1, */
};

enum spi_framesz {
	FRAMESZ_4BIT = 0x3,
	FRAMESZ_5BIT = 0x4,
	FRAMESZ_6BIT = 0x5,
	FRAMESZ_7BIT = 0x6,
	FRAMESZ_8BIT = 0x7,
	FRAMESZ_9BIT = 0x8,
	FRAMESZ_10BIT = 0x9,
	FRAMESZ_11BIT = 0xA,
	FRAMESZ_12BIT = 0xB,
	FRAMESZ_13BIT = 0xC,
	FRAMESZ_14BIT = 0xD,
	FRAMESZ_15BIT = 0xE,
	FRAMESZ_16BIT = 0xF,
};

struct spi_dev;

/* Initialise the SSP
 */
struct spi_dev *spi_init(enum ssp_select devno, enum spi_framesz framesz);

/* Transfer data over the bus
 *
 * Transfer "len" frames. If tx_data is not NULL, then this will be clocked out
 * on MOSI (otherwise zeros will be transmitted). If rx_data is not NULL, then
 * the received data will be stored into it from MISO.
 * Blocks until all of the data has been transferred.
 * The data will be cast depending on framesz:
 *  <= 8 bits: uint8_t
 *  <= 16 bits: uint16_t
 * N.B. The data should be right-aligned.
 */
void spi_transfer(struct spi_dev *dev, void *tx_data, void *rx_data,
		size_t len);
