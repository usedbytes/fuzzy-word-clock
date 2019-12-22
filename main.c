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

#define MIN_CYCLES 4

volatile uint32_t bitslices[2][PWM_RESOLUTION];
volatile int active_set = 0;

void TIMER_16_0_Handler(void) {
	static int bit = 0;
	static uint32_t mr = (1 << 10);
	static volatile uint32_t *set = bitslices[0];

	DBG_HIGH();
	LPC_CTxxBx_Type *timer = LPC_CT16B0;
	uint32_t status = timer->IR;

	if (status & (1 << 3)) {
		CH1_TOGGLE();

		//while(timer->TC >= timer->MR3);
		timer->TC = 0;
		timer->MR3 = mr - 1;

		LPC_GPIO->MPIN[1] = set[bit++];

		if (mr == 1 << (1 + (10 - PWM_RESOLUTION))) {
			mr = (1 << 10);
			bit = 0;
			set = bitslices[active_set];
		} else {
			mr >>= 1;
		}
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
	for (i = 0; i < PWM_RESOLUTION; i++) {
		if (val16 & (1 << (PWM_RESOLUTION - i - 1))) {
			slice[i] = PIN_CH2;
		} else {
			slice[i] = 0;
		}
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

