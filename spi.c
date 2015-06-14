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

#include "lpc11uxx/LPC11Uxx.h"
#include <stdint.h>
#include "spi.h"

/* Control Register 0 */
#define CR0_DSS(_framesz) ((_framesz & 0xF) << 0)
#define CR0_FRF(_format)  ((_format & 0x3) << 4)
#define		FRF_SPI    0
#define		FRF_TI     1
#define		FRF_uWIRE  2
#define	CR0_CPOL          (1 << 6)
#define	CR0_CPHA          (1 << 7)
#define	CR0_SCR(_clks)    ((_clks & 0xFF) << 8)
/* Control Register 1 */
#define CR1_LBM           (1 << 0)
#define CR1_SSE           (1 << 1)
#define CR1_MS            (1 << 2)
#define CR1_SOD           (1 << 3)
/* Status Bits */
#define SR_TFE            (1 << 0)
#define SR_TNF            (1 << 1)
#define SR_RNE            (1 << 2)
#define SR_RFF            (1 << 3)
#define SR_BSY            (1 << 4)
/* Interrupt bits */
#define IRQ_OR            (1 << 0)
#define IRQ_RT            (1 << 1)
#define IRQ_RX            (1 << 2)
#define IRQ_TX            (1 << 3)

#define SSP_FIFO_SIZE 8

struct spi_iocfg {
	__IO uint32_t *regs[3];
	uint32_t iocon[3];
	uint8_t clk_bit;
	uint8_t rst_bit;
};

static const struct spi_iocfg spi_iocfgs[] = {
	[SSP0] = {
		.regs = {
			&LPC_IOCON->PIO0_8,
			&LPC_IOCON->PIO0_9,
			&LPC_IOCON->SWCLK_PIO0_10
		},
		/* Select SPI function mode with "open drains" */
		.iocon = {
			(1 << 10) | (1 << 0),
			(1 << 10) | (1 << 0),
			(1 << 10) | (2 << 0)
		},
		.clk_bit = 11,
		.rst_bit = 0,
	},
};

struct spi_dev {
	/* Static instance data */
	const struct spi_iocfg *const iocfg;
	LPC_SSPx_Type *const base;

	/* Runtime Config */
	enum spi_framesz frame_size;
};

struct spi_dev spi_devices[] = {
	[SSP0] = {
		.iocfg = &spi_iocfgs[SSP0],
		.base = LPC_SSP0,
	},
};

static void spi_reset(const struct spi_dev *dev)
{
	LPC_SYSCON->PRESETCTRL &= ~(1 << dev->iocfg->rst_bit);
	LPC_SYSCON->PRESETCTRL |= (1 << dev->iocfg->rst_bit);
}

struct spi_dev *spi_init(enum ssp_select devno, enum spi_framesz framesz)
{
	struct spi_dev *dev = &spi_devices[devno];
	const struct spi_iocfg *pincfg = &spi_iocfgs[devno];

	/* Turn on the clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << pincfg->clk_bit);

	switch (devno) {
	case SSP0:
		/* FIXME: Configurable clock speed */
		LPC_SYSCON->SSP0CLKDIV = 24;
	}

	/* Set up the pins */
	*pincfg->regs[0] = pincfg->iocon[0]; /* MISO */
	*pincfg->regs[1] = pincfg->iocon[1]; /* MOSI */
	*pincfg->regs[2] = pincfg->iocon[2]; /* SCK */

	spi_reset(dev);

	/* FIXME: What to do here? */
	dev->base->CPSR = 2;

	/* FIXME: What SCR do we want? */
	dev->base->CR0 = CR0_DSS(framesz) | CR0_FRF(FRF_SPI);

	dev->base->CR1 = CR1_SSE;

	return dev;
}

void spi_transfer(struct spi_dev *dev, void *tx_data, void *rx_data,
		size_t len)
{
	uint32_t i = 0;

	while (len--) {
		if (tx_data) {
			if (dev->frame_size > FRAMESZ_8BIT) {
				/* Apparently I can't do this in one go */
				LPC_SSP0->DR = *(uint16_t *)tx_data;
				tx_data = (uint16_t *)tx_data + 1;
			} else {
				LPC_SSP0->DR = *(uint8_t *)tx_data;
				tx_data = (uint8_t *)tx_data + 1;
			}
		} else {
			LPC_SSP0->DR = 0;
		}

		if (!len || (++i == SSP_FIFO_SIZE)) {
			/* Wait for transfer and drain the RX FIFO */
			while (LPC_SSP0->SR & SR_BSY);
			while (LPC_SSP0->SR & SR_RNE) {
				if (rx_data) {
					if (dev->frame_size > FRAMESZ_8BIT) {
						*(uint16_t *)rx_data = LPC_SSP0->DR;
						rx_data = (uint16_t *)rx_data + 1;
					} else {
						*(uint8_t *)rx_data = LPC_SSP0->DR;
						rx_data = (uint8_t *)rx_data + 1;
					}
				} else {
					/* Useless read to drain the FIFO */
					i = LPC_SSP0->DR;
				}
			}
			i = 0;
		}
	}
}
