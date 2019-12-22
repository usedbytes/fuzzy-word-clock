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

#define DEBUG
#ifdef DEBUG
#define DBG_PORT (0)
#define DBG_PIN  (1 << 19)
#define DBG_PIN1  (1 << 18)
#define DBG_HIGH()   LPC_GPIO->SET[DBG_PORT] = DBG_PIN
#define DBG_LOW()    LPC_GPIO->CLR[DBG_PORT] = DBG_PIN
#define DBG_TOGGLE() LPC_GPIO->NOT[DBG_PORT] = DBG_PIN
#else
#define DBG_HIGH() {}
#define DBG_LOW()  {}
#define DBG_TOGGLE() {}
#endif

#define PWM_CHANNELS   8
#define PWM_RESOLUTION 10
#define MIN_CYCLES_LOG2 2

volatile uint32_t bitslices[2][PWM_RESOLUTION];
volatile uint8_t in_use_set = 0;
volatile uint8_t queued_set = 0;
volatile uint16_t values[PWM_CHANNELS];

void TIMER_16_0_Handler(void) {
	static int bit = 0;
	static int dir = 1;
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

		LPC_GPIO->MPIN[0] = set[bit];
		bit += dir;

#ifdef DEBUG
		LPC_GPIO->NOT[DBG_PORT] = DBG_PIN1;
#endif
		if (dir > 0 && bit == PWM_RESOLUTION) {
			in_use_set = queued_set;
			set = bitslices[in_use_set];
			bit = PWM_RESOLUTION - 1;
			dir = -1;
		} else if (dir < 0 && bit == -1) {
			in_use_set = queued_set;
			set = bitslices[in_use_set];
			bit = 0;
			dir = 1;
		}
		period = 1 << (MIN_CYCLES_LOG2 + bit);
	}
	timer->IR = status;
	DBG_LOW();
}

static void init_timer16()
{
	LPC_CTxxBx_Type *timer = LPC_CT16B0;

	NVIC_SetPriority(TIMER_16_0_IRQn, 0);
	NVIC_EnableIRQ(TIMER_16_0_IRQn);

	/* Turn on the clock - 48MHz */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 7);

	/* Conservative */
	timer->PR = 80;
	//timer->PR = 60;

	/* Use MR3 as overflow interupt */
	timer->MR3 = 0;
	timer->MCR = (1 << 9);
	timer->TCR = 0x1;
}

const uint8_t gamma8[] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
	2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
	5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
	10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
	17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
	25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
	37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
	51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
	69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
	90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
	115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
	144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
	177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
	215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

void pwm_flip() {
	int i;
	uint16_t mask = 1 << (16 - PWM_RESOLUTION);

	/* Only one flip in flight */
	while (queued_set != in_use_set);

	volatile uint32_t *slice = bitslices[!queued_set];
	for (i = 0; i < PWM_RESOLUTION; i++, mask <<= 1) {
		slice[i] = (
			   ((values[0] & mask) ? PIN_LED0 : 0) |
			   ((values[1] & mask) ? PIN_LED1 : 0) |
			   ((values[2] & mask) ? PIN_LED2 : 0) |
			   ((values[3] & mask) ? PIN_LED3 : 0) |
			   ((values[4] & mask) ? PIN_LED4 : 0) |
			   ((values[5] & mask) ? PIN_LED5 : 0) |
			   ((values[6] & mask) ? PIN_LED6 : 0) |
			   ((values[7] & mask) ? PIN_LED7 : 0)
		);
	}
	queued_set = !queued_set;
}

void pwm_set(uint8_t channel, uint16_t value) {
	/* FIXME: HAX, only works for 8 bit */
	//values[channel] = gamma8[value >> 8] << 8;
	values[channel] = value;
}

void leds_init(void)
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

	LPC_GPIO->MASK[0] = ~(PIN_LED0 | PIN_LED1 | PIN_LED2 | PIN_LED3 | PIN_LED4 | PIN_LED5 | PIN_LED6 | PIN_LED7);
}

void u32_to_str(uint32_t val, char *buf)
{
	int i = 8;
	while (i--) {
		if ((val & 0xf) <= 9)
			buf[i] = (val & 0xf) + '0';
		else
			buf[i] = (val & 0xf) + ('a' - 10);
		val >>= 4;
	}
}

int main(void) {
	char buf[11] = "        \r\n";
	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

#ifdef DEBUG
	LPC_GPIO->DIR[DBG_PORT] |= DBG_PIN | DBG_PIN1;
	LPC_GPIO->CLR[DBG_PORT] = DBG_PIN | DBG_PIN1;
#endif

	usb_init();
	leds_init();

	delay_ms(100);

	init_timer16(0);

	uint16_t val = 0x0000;

	while(1) {
		/*
		if (val == 0 || val == 0x9000) {
			pwm_set(0, 0x8000);
			pwm_flip();
			delay_ms(500);
			val = 0x7000;
		}
		*/
		val += 64;
		pwm_set(0, val);
		pwm_set(1, val + 0x300);
		pwm_set(2, val + 0x470);
		pwm_set(3, val + 0x1470);
		pwm_set(4, val + 0x2470);
		pwm_set(5, val + 0x9d70);
		pwm_set(6, val + 0xa490);
		pwm_set(7, val + 0xe470);
		//pwm_set(1, val);
		//pwm_set(2, val);
		pwm_flip();
		delay_ms(10);
		u32_to_str(val, buf);
		usb_usart_print(buf);
		usb_usart_print("\r\n");
	}

	return 0;
}
