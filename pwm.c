/*
 * PWM using 16 bit timers
 *
 * Copyright Brian Starkey 2014-2016 <stark3y@gmail.com>
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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "LPC11Uxx.h"
#include "pwm.h"

struct pwm_16b pwm0 = {
	LPC_CT16B0,
	&LPC_GPIO->PIN[0],
	{ 0x3 << 9, 0x3 << 11, 0x3 << 13 },
	{ 0, 0, 0 },
};

static void pwm_handler(struct pwm_16b *pwm) {
	uint32_t status = pwm->timer->IR;
	bool overflow = status & (1 << 3);
	uint32_t state = *pwm->port;
	uint32_t to_clear = 0;
	uint32_t to_set = 0;
	int channel;

	/* Turn enabled channels on if there was an overflow */
	if (overflow) {
		for (channel = 0; channel < 3; channel++) {
			if (pwm->vals[channel])
				to_set |= pwm->pins[channel];
		}
	}

	/* Selectively turn them off if there was a match */
	for (channel = 0; channel < 3; channel++) {
		if (status & (1 << channel)) {
			to_clear |= pwm->pins[channel];
			pwm->timer->MR[channel] = pwm->vals[channel];
		}
	}

	to_clear = ~to_clear;
	state |= to_set;
	state &= to_clear;
	*pwm->port = state;

	pwm->timer->IR = status;
	return;
}

void TIMER_16_0_Handler(void) {
	pwm_handler(&pwm0);
}

static void init_timer16_0(LPC_CTxxBx_Type *timer)
{
	int idx = timer - LPC_CT16B0;

	/* Turn on the clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << (7 + idx));
	timer->PR = 4;

	/* Use MR3 as overflow interupt */
	timer->MR3 = 0x0;
	timer->MCR = (1 << 9);
	timer->TCR = 0x1;

	/* Depend on the interrupt numbering being sane */
	NVIC_SetPriority(TIMER_16_0_IRQn + idx, 3);
	NVIC_EnableIRQ(TIMER_16_0_IRQn + idx);
}

struct pwm_16b *pwm_init(int idx, __IO uint32_t *port, uint32_t pins[3])
{
	struct pwm_16b *pwm = &pwm0;

	/* pwm1 not implemented... crash :-( */
	if (idx)
		while(1);

	pwm->port = port;
	memcpy(pwm->pins, pins, sizeof(pwm->pins[0]) * 3);
	init_timer16_0(pwm->timer);

	return pwm;
}

void pwm_start(struct pwm_16b *pwm)
{
	pwm->timer->MR[0] = pwm0.vals[0];
	pwm->timer->MR[1] = pwm0.vals[1];
	pwm->timer->MR[2] = pwm0.vals[2];
	pwm->timer->TCR = 0x1;
}

void pwm_stop(struct pwm_16b *pwm)
{
	pwm->timer->TCR = 0x0;
}

void pwm_set(struct pwm_16b *pwm, uint32_t channel, uint8_t value)
{
	pwm->vals[channel] = value << 8;
	pwm->timer->MCR |= (1 << (channel * 3));
}
