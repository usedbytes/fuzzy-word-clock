#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "usb_cdc.h"
#include "util.h"

#define PIN_LED0 (1 << 15)
#define PIN_LED1 (1 << 14)
#define PIN_LED2 (1 << 13)
#define PIN_LED3 (1 << 12)
#define PIN_LED4 (1 << 11)
#define PIN_LED5 (1 << 10)
#define PIN_LED6 (1 <<  9)
#define PIN_LED7 (1 <<  8)

#ifdef DEBUG
#define PIN_DBG (1 << 26)
#define DBG_HIGH() LPC_GPIO->SET[1] = PIN_DBG
#define DBG_LOW()  LPC_GPIO->CLR[1] = PIN_DBG
#define DBG_TOGGLE()  LPC_GPIO->NOT[1] = PIN_DBG
#else
#define DBG_HIGH() {}
#define DBG_LOW()  {}
#define DBG_TOGGLE() {}
#endif

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

		LPC_GPIO->MPIN[0] = set[bit++];

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
			(val16 & 1) * PIN_LED0 |
			(val16 & 1) * PIN_LED1 |
			(val16 & 1) * PIN_LED2 |
			(val16 & 1) * PIN_LED3 |
			(val16 & 1) * PIN_LED4 |
			(val16 & 1) * PIN_LED5 |
			(val16 & 1) * PIN_LED6 |
			(val16 & 1) * PIN_LED7 |
			0
		);
	}
	active_set = !active_set;
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
	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

#ifdef DEBUG
	LPC_GPIO->DIR[1] |= PIN_DBG;
	LPC_GPIO->CLR[1] = PIN_DBG;
#endif

	usb_init();
	setup_leds();

	delay_ms(100);

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
