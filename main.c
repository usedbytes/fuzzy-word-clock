#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "usb_cdc.h"
#include "util.h"

#define PIN_CH2 (1 << 20)
#define PIN_CH1 (1 << 27)
#define PIN_CH0 (1 << 26)

#define DBG_HIGH() LPC_GPIO->SET[1] = PIN_CH0;
#define DBG_LOW()  LPC_GPIO->CLR[1] = PIN_CH0;
#define DBG_TOGGLE()  LPC_GPIO->NOT[1] = PIN_CH0;

#define CH1_HIGH() LPC_GPIO->SET[1] = PIN_CH1;
#define CH1_LOW()  LPC_GPIO->CLR[1] = PIN_CH1;
#define CH1_TOGGLE()  LPC_GPIO->NOT[1] = PIN_CH1;

#define PWM_RESOLUTION 8

#define MIN_CYCLES_LOG2 2

volatile uint32_t bitslices[2][PWM_RESOLUTION];
volatile int active_set = 0;

void TIMER_16_0_Handler(void) {
	static int bit = 0;
	static uint32_t period = (1 << MIN_CYCLES_LOG2);
	static volatile uint32_t *set = bitslices[0];

	DBG_HIGH();
	LPC_CTxxBx_Type *timer = LPC_CT16B0;
	uint32_t status = timer->IR;

	if (status & (1 << 3)) {
		CH1_TOGGLE();

		/*
		 * Fun fact: You can't use reset-on-match mode and change the
		 * match value in the interrupt handler.
		 * The reset happens 1 prescaled clock cycle after the match
		 * interrupt, which means if you change the match here, the
		 * reset for the old match doesn't happen.
		 * You can spin waiting for the reset before changing the MR,
		 * but that blows my timing budget:
		 *     while (timer->MR3 >= period - 1);
		 * Instead, we clear the counter manually. It will introduce
		 * some jitter, but ultimately is probably fine.
		 */
		timer->TC = 0;
		timer->MR3 = period - 1;

		LPC_GPIO->MPIN[1] = set[bit++];

		if (bit == PWM_RESOLUTION) {
			set = bitslices[active_set];
			bit = 0;
		}
		period = 1 << (MIN_CYCLES_LOG2 + bit);
	}
	timer->IR = status;
	DBG_LOW();
}

static void init_timer16(int idx)
{
	static LPC_CTxxBx_Type *const timers[] = {
		LPC_CT16B0,
		LPC_CT16B1,
	};

	LPC_CTxxBx_Type *timer = timers[idx];

	/* Depend on the interrupt numbering being sane */
	NVIC_SetPriority(TIMER_16_0_IRQn + idx, 3);
	NVIC_EnableIRQ(TIMER_16_0_IRQn + idx);

	/* Turn on the clock - 48MHz */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << (7 + idx));

	timer->PR = 163;

	/* Use MR3 as overflow interupt */
	timer->MR3 = 0;
	timer->MCR = (1 << 9);
	timer->TCR = 0x1;
}

void set_pwm(uint16_t val16) {
	int i;
	val16 >>= (16 - PWM_RESOLUTION);

	volatile uint32_t *slice = bitslices[!active_set];
	for (i = 0; i < PWM_RESOLUTION; i++, val16 >>= 1) {
		slice[i] = (
			(val16 & 1) * PIN_CH2 |
			0
		);
	}
	active_set = !active_set;
}

int main(void) {
	const char *p = (const char *)(0x000000c0);
	uint32_t start, duration;

	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

	LPC_GPIO->DIR[1] |= PIN_CH0 | PIN_CH1 | PIN_CH2;

	usb_init();

	LPC_GPIO->CLR[1] = PIN_CH0 | PIN_CH1 | PIN_CH2;
	delay_ms(100);

	LPC_GPIO->MASK[1] = ~(PIN_CH2);

	init_timer16(0);

	uint16_t val = 0x0000;
	set_pwm(val);

	while(1) {
		set_pwm(val);
		val += 256;
		delay_ms(20);
	}

	return 0;
}

