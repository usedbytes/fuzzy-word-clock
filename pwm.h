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
#ifndef __PWM_H__
#define __PWM_H__
#include <stdint.h>

#include "LPC11Uxx.h"

struct pwm_16b {
	LPC_CTxxBx_Type *timer;
	__IO uint32_t *port;
	uint32_t pins[3];
	uint16_t vals[3];
};

struct pwm_16b *pwm_init(int idx, __IO uint32_t *port, uint32_t pins[3]);
void pwm_start(struct pwm_16b *pwm);
void pwm_stop(struct pwm_16b *pwm);
void pwm_set(struct pwm_16b *pwm, uint32_t channel, uint8_t value);

#endif /* __PWM_H__ */
