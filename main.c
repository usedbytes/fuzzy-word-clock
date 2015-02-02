#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"
#include "usart.h"
#include "ds1302.h"

#define RED_BIT   (1 << 2)
#define GREEN_BIT (1 << 23)
#define BLUE_BIT  (1 << 24)

volatile uint32_t msTicks = 0;

void SysTick_Handler(void) {
	msTicks++;
}

void delay_ms(uint32_t ms) {
	uint32_t now = msTicks;
	while ((msTicks-now) < ms);
}

struct pwm_16b {
	LPC_CTxxBx_Type *timer;
	__IO uint32_t *port;
	uint32_t pins[3];
	uint16_t vals[3];
};

struct pwm_16b pwm0 = {
	LPC_CT16B0,
	&LPC_GPIO->PIN[1],
	{ RED_BIT, GREEN_BIT, BLUE_BIT },
	{ 0, 0, 0 },
};

void TIMER_16_0_Handler(void) {
	uint32_t status = LPC_CT16B0->IR;
	bool overflow = status & (1 << 3);
	int channel;

	/* Turn all channels off if there was an overflow */
	if (overflow) {
		*pwm0.port &= ~(pwm0.pins[0] | pwm0.pins[1] | pwm0.pins[2]);
	}

	/* Selectively turn them on if there was a match */
	for (channel = 0; channel < 3; channel++) {
		uint32_t val = pwm0.timer->MR[channel];

		/* Latch in the new value */
		pwm0.timer->MR[channel] = pwm0.vals[channel];

		if (status & (1 << channel)) {
			if (overflow && (val >= 0x8000)) {
				/* Don't bother turning on because we missed our chance */
				continue;
			}
			*pwm0.port |= (pwm0.pins[channel]);
		}
	}
	LPC_CT16B0->IR = status;
}

void init_timer16_0()
{
	/* Turn on the clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 7);
	LPC_CT16B0->PR = 4;
	/* Use MR3 as overflow interupt */
	LPC_CT16B0->MR3 = 0x0;
	LPC_CT16B0->MCR = (1 << 9);
	LPC_CT16B0->TCR = 0x1;
	NVIC_SetPriority(TIMER_16_0_IRQn, 3);
	NVIC_EnableIRQ(TIMER_16_0_IRQn);
}

void pwm_set(struct pwm_16b *pwm, uint32_t channel, uint16_t value)
{
	if (value) {
		value = ~value;
		pwm->vals[channel] = value;
		pwm->timer->MCR |= (1 << (channel * 3));
	} else {
		pwm->timer->MCR &= ~(1 << (channel * 3));
		*pwm->port &= ~pwm->pins[channel];
		pwm->vals[channel] = 0;
	}
}

void hsv2rgb(uint32_t h, uint16_t s, uint16_t v, uint16_t *rgb)
{
	int section = h / 0x10000;
	uint16_t offset = h % 0x10000;
	uint32_t max = v;
	uint32_t min = (max * (0x10000 - s)) >> 16;
	uint32_t range = max - min;
	uint32_t fade = (range * offset) >> 16;
	uint32_t level = section & 1 ? min + fade : max - fade;

	/*
	101  R <g !b
	000 >r  G !b
	001 !r  G <b
	010 !r >g  B
	011 <r !g  B
	100  R !g >B
	*/

	switch (section) {
	case 0:
		rgb[0] = level;
		rgb[1] = max;
		rgb[2] = min;
		break;
	case 1:
		rgb[0] = min;
		rgb[1] = max;
		rgb[2] = level;
		break;
	case 2:
		rgb[0] = min;
		rgb[1] = level;
		rgb[2] = max;
		break;
	case 3:
		rgb[0] = level;
		rgb[1] = min;
		rgb[2] = max;
		break;
	case 4:
		rgb[0] = max;
		rgb[1] = min;
		rgb[2] = level;
		break;
	case 5:
		rgb[0] = max;
		rgb[1] = level;
		rgb[2] = min;
		break;
	}
}

int main(void) {
	uint32_t hue = 0;
	uint16_t rgb[3], sat, val;
	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	LPC_GPIO->DIR[1] = (1 << 2) | (1 << 23) | (1 << 24);

	/* We're using some JTAG pins for the RTC. Unlike other pins,
	 * they don't default to GPIO :-(
	 */
	LPC_IOCON->TDO_PIO0_13 |= 1;
	LPC_IOCON->TMS_PIO0_12 |= 1;
	LPC_IOCON->TDI_PIO0_11 |= 1;

	init_timer16_0();
	sat = 0xffff;
	val = 0x1000;
	while (1) {
		for (hue = 0; hue < 0xFFFF * 6; hue += 100) {
			hsv2rgb(hue, sat, val, rgb);
			pwm_set(&pwm0, 0, rgb[0]);
			pwm_set(&pwm0, 1, rgb[1]);
			pwm_set(&pwm0, 2, rgb[2]);
			delay_ms(1);
		}
	}

	return 0;
}

