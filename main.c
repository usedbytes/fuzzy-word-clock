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
	while ((msTicks-now) < ms)
		;
}

struct pwm_16b {
	LPC_CTxxBx_Type *timer;
	__IO uint32_t *port;
	uint32_t pins[3];
	uint16_t vals[3];
};

struct pwm_16b pwm0 = {
	LPC_CT16B0,
	&LPC_GPIO->PIN[0],
	{ 0x3 << 9, 0x3 << 11, 0x3 << 13 },
	{ 0, 0, 0 },
};

void TIMER_16_0_Handler(void) {
	uint32_t status = LPC_CT16B0->IR;
	bool overflow = status & (1 << 3);
	int channel;

	/* Turn all channels off if there was an overflow */
	if (overflow) {
		*pwm0.port &= ~(pwm0.pins[0] | pwm0.pins[1] | pwm0.pins[2]);
		pwm0.timer->MR[0] = pwm0.vals[0];
		pwm0.timer->MR[1] = pwm0.vals[1];
		pwm0.timer->MR[2] = pwm0.vals[2];
	}

	/* Selectively turn them on if there was a match */
	for (channel = 0; channel < 3; channel++) {
		uint32_t val = pwm0.timer->MR[channel];

		/* Latch in the new value */
		//pwm0.timer->MR[channel] = pwm0.vals[channel];

		if (status & (1 << channel)) {
			if (overflow && (val >= 0x8000)) {
				/* Don't bother turning on because we missed our chance */
				continue;
			}
			*pwm0.port |= (pwm0.pins[channel]);
		}
	}
	LPC_CT16B0->IR = status;
	return;
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
		pwm->vals[channel] = 0xffff;
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
	int32_t level = section & 1 ? min + fade : max - fade;
	if (level > 0xffff)
		level = 0xffff;
	else if (level < 0)
		level = 0;

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

void set_with_mask(volatile uint32_t *reg, uint32_t mask, uint32_t val)
{
	*reg &= ~mask;
	*reg |= val & mask;
}

void setup_leds(void)
{
	/* Set all our LED pins as GPIO outputs */
	set_with_mask(&LPC_IOCON->PIO0_8, 0x3, 0x0);
	set_with_mask(&LPC_IOCON->PIO0_9, 0x3, 0x0);
	set_with_mask(&LPC_IOCON->SWCLK_PIO0_10, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->TDI_PIO0_11, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->TMS_PIO0_12, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->TDO_PIO0_13, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->TRST_PIO0_14, 0x3, 0x1);
	set_with_mask(&LPC_IOCON->SWDIO_PIO0_15, 0x3, 0x1);
	LPC_GPIO->DIR[0] |= (0xFF << 8);
}

int main(void) {
	uint32_t hue = 0;
	uint16_t rgb[3], sat;
	uint32_t val;
	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	//LPC_GPIO->DIR[1] = (1 << 2) | (1 << 23) | (1 << 24) | (1 << 31);

	setup_leds();
	init_timer16_0();
	sat = 0xffff;
	val = 0x8000;
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

