#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "LPC11Uxx.h"

#include "iap.h"
#include "ds1302.h"
#include "lut.h"
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
#define N_CHANNELS  8

//#define DEBUG
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

#define N_MEALS   5
#define N_TIMES   8

#define EEPROM_TIME_OFFSET 4
#define EEPROM_BRIGHTNESS_OFFSET (EEPROM_TIME_OFFSET + (N_TIMES * 2))

#define EEPROM_VERSION 1 // Increment this every time the EEPROM layout changes
#define EEPROM_MAGIC (((uint32_t)(('f' << 24) | ('u' << 16) | ('z' << 8) | ('z' << 0))) + EEPROM_VERSION)

#define BREAKFAST   0
#define LUNCH       1
#define HOME        2
#define DINNER      3
#define BED         4
#define NEARLY      5
#define PAST        6
#define SLEEP       7 // Deliberately 7 - Only used for EEPROM so doesn't alias with ITS_TIME

#define ITS_TIME    7
#define BIT(x)      (1 << (x))

const uint8_t channel_map[] = {
	[BREAKFAST] =  2,
	[LUNCH]     =  5,
	[HOME]      =  7,
	[DINNER]    =  3,
	[BED]       =  6,
	[NEARLY]    =  0,
	[PAST]      =  4,
	[ITS_TIME]  =  1,
};

const uint8_t sequence[] = {
	(0),
	(BIT(ITS_TIME) | BIT(NEARLY) | BIT(BREAKFAST)),
	(BIT(ITS_TIME) | 0           | BIT(BREAKFAST)),
	(BIT(ITS_TIME) | BIT(PAST)   | BIT(BREAKFAST)),

	(BIT(ITS_TIME) | BIT(NEARLY) | BIT(LUNCH)    ),
	(BIT(ITS_TIME) | 0           | BIT(LUNCH)    ),
	(BIT(ITS_TIME) | BIT(PAST)   | BIT(LUNCH)    ),

	(BIT(ITS_TIME) | BIT(NEARLY) | BIT(HOME)     ),
	(BIT(ITS_TIME) | 0           | BIT(HOME)     ),
	(BIT(ITS_TIME) | BIT(PAST)   | BIT(HOME)     ),

	(BIT(ITS_TIME) | BIT(NEARLY) | BIT(DINNER)   ),
	(BIT(ITS_TIME) | 0           | BIT(DINNER)   ),
	(BIT(ITS_TIME) | BIT(PAST)   | BIT(DINNER)   ),

	(BIT(ITS_TIME) | BIT(NEARLY) | BIT(BED)      ),
	(BIT(ITS_TIME) | 0           | BIT(BED)      ),
	(BIT(ITS_TIME) | BIT(PAST)   | BIT(BED)      ),

	(0),
};

#define TIME(h, m) ((h << 8) | (m))
uint16_t times[] = {
	[BREAKFAST] = TIME(0x08, 0x00),
	[LUNCH]     = TIME(0x12, 0x20),
	[HOME]      = TIME(0x17, 0x30),
	[DINNER]    = TIME(0x19, 0x00),
	[BED]       = TIME(0x22, 0x00),
	[NEARLY]    = TIME(0x00, 0x15),
	[PAST]      = TIME(0x00, 0x15),
	[SLEEP]     = TIME(0x01, 0x00),
};

struct timeband {
	uint16_t start;
	uint16_t end;
	uint8_t sentence;
};

int n_timebands = 0;
struct timeband timebands[20];
uint16_t brightness = 0xffff;

uint8_t hours(uint16_t time)
{
	return bcd_to_dec(time >> 8);
}

uint8_t mins(uint16_t time)
{
	return bcd_to_dec(time & 0xFF);
}

uint16_t time_add(uint16_t a, uint16_t b)
{
	int h = hours(a) + hours(b);
	int m = mins(a) + mins(b);

	while (m >= 60) {
		h++;
		m -= 60;
	}

	while (h >= 24) {
		h -= 24;
	}

	return TIME(dec_to_bcd(h), dec_to_bcd(m));
}

uint16_t time_sub(uint16_t a, uint16_t b)
{
	int h = hours(a) - hours(b);
	int m = mins(a) - mins(b);

	while (m < 0) {
		h--;
		m += 60;
	}

	while (h < 0) {
		h += 24;
	}
	while (h >= 24) {
		h -= 24;
	}

	return TIME(dec_to_bcd(h), dec_to_bcd(m));
}

void build_timebands()
{
	int i;
	timebands[n_timebands] = (struct timeband){
		.start = TIME(0, 0),
		.sentence = 0,
	};
	n_timebands++;

	for (i = 0; i < N_MEALS; i++) {
		uint16_t tmp = time_sub(times[i], times[NEARLY]);
		timebands[n_timebands] = (struct timeband){
			.start = tmp,
			.sentence = BIT(ITS_TIME) | BIT(NEARLY) | BIT(i),
		};
		n_timebands++;

		timebands[n_timebands] = (struct timeband){
			.start = times[i],
			.sentence = BIT(ITS_TIME) | 0 | BIT(i),
		};
		n_timebands++;

		tmp = time_add(times[i], times[PAST]);
		timebands[n_timebands] = (struct timeband){
			.start = tmp,
			.sentence = BIT(ITS_TIME) | BIT(PAST) | BIT(i),
		};
		n_timebands++;
	}

	timebands[n_timebands] = (struct timeband){
		.start = time_add(times[BED], times[SLEEP]),
		.sentence = 0,
	};
	n_timebands++;
}

enum button_state {
	NONE,
	PRESSED,
	HELD,
};
volatile enum button_state button = NONE;
enum clock_mode {
	NORMAL,
	BLANKED,
	DEMO,
};
volatile enum clock_mode mode = NORMAL;
volatile bool dirty = false;

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
		//LPC_GPIO->NOT[DBG_PORT] = DBG_PIN1;
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

static void init_timer16(void)
{
	LPC_CTxxBx_Type *timer = LPC_CT16B0;

	NVIC_SetPriority(TIMER_16_0_IRQn, 0);
	NVIC_EnableIRQ(TIMER_16_0_IRQn);

	/* Turn on the clock - 48MHz */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 7);

	/* Conservative */
	//timer->PR = 80;
	timer->PR = 60;

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

	/*
	 * If previous flip hasn't completed, we try to cancel it.
	 * The timer routing will race with us, but all possibilities are
	 * fine:
	 *  - If the timer already swapped, then the condition is false and we're fine
	 *  - If the timer swaps in between the check and the set, then queued_set
	 *    simply doesn't change
	 *  - If both the check and set happen before the timer swaps, then the
	 *    previous flip is cancelled
	 */
	if (queued_set != in_use_set) {
		NVIC_DisableIRQ(TIMER_16_0_IRQn);
		queued_set = in_use_set;
		NVIC_EnableIRQ(TIMER_16_0_IRQn);
	}

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
	values[channel] = lut1d(value);
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

void print_u32(uint32_t val)
{
	char buf[11] = "        \r\n";
	u32_to_str(val, buf);
	usb_usart_print(buf);
}

void WAKEUP0_Handler(void)
{
	uint32_t stat = LPC_GPIO_PIN_INT->IST;
	if (stat & 1) {
		if (LPC_GPIO_PIN_INT->ISEL & 1) {
			LPC_CT16B1->TCR = 0x0;

			LPC_GPIO_PIN_INT->ISEL &= ~(1 << 0);
			LPC_GPIO_PIN_INT->FALL = (1 << 0);
			LPC_GPIO_PIN_INT->RISE = (1 << 0);
			LPC_GPIO_PIN_INT->CIENF = (1 << 0);
			LPC_GPIO_PIN_INT->CIENR = (1 << 0);

			/* Reset to falling-edge triggered */
			LPC_GPIO_PIN_INT->SIENF = (1 << 0);

			if (LPC_CT16B1->TC < 700) {
				button = PRESSED;
			}
			LPC_CT16B1->TC = 0x0;

		} else {
			LPC_GPIO_PIN_INT->FALL = (1 << 0);
			LPC_GPIO_PIN_INT->RISE = (1 << 0);
			LPC_GPIO_PIN_INT->CIENF = (1 << 0);
			LPC_GPIO_PIN_INT->CIENR = (1 << 0);

			LPC_CT16B1->MR3 = 150;
			LPC_CT16B1->MCR = (1 << 9) | (1 << 11);
			LPC_CT16B1->TC = 0x0;
			LPC_CT16B1->TCR = 0x1;
		}
	}
	LPC_GPIO_PIN_INT->IST = stat;
}

void TIMER_16_1_Handler(void)
{
	uint32_t status = LPC_CT16B1->IR;
	if (status & (1 << 3)) {
		/* 30ms expired */
		/* Select high level triggered */
		NVIC_DisableIRQ(FLEX_INT0_IRQn);
		LPC_GPIO_PIN_INT->ISEL = (1 << 0);
		LPC_GPIO_PIN_INT->FALL = (1 << 0);
		LPC_GPIO_PIN_INT->RISE = (1 << 0);
		LPC_GPIO_PIN_INT->SIENF = (1 << 0);
		LPC_GPIO_PIN_INT->SIENR = (1 << 0);

		/* Stop after 3s */
		LPC_CT16B1->MR2 = 2100;
		LPC_CT16B1->MCR = (1 << 6) | (1 << 8);
		LPC_CT16B1->TC = 0x0;
		LPC_CT16B1->TCR = 0x1;
		NVIC_EnableIRQ(FLEX_INT0_IRQn);
	}
	if (status & (1 << 2)) {
		/* 3s expired */
		NVIC_DisableIRQ(FLEX_INT0_IRQn);
		LPC_GPIO_PIN_INT->ISEL &= ~(1 << 0);
		LPC_GPIO_PIN_INT->FALL = (1 << 0);
		LPC_GPIO_PIN_INT->CIENF = (1 << 0);
		LPC_GPIO_PIN_INT->CIENR = (1 << 0);
		LPC_GPIO_PIN_INT->IST = (1 << 0);

		/* Reset to falling edge */
		LPC_GPIO_PIN_INT->SIENF = (1 << 0);

		button = HELD;
		NVIC_EnableIRQ(FLEX_INT0_IRQn);
	}
	LPC_CT16B1->IR = status;
}

static void init_timer32()
{
	LPC_CTxxBx_Type *timer = LPC_CT32B0;

	NVIC_SetPriority(TIMER_32_0_IRQn, 3);
	NVIC_EnableIRQ(TIMER_32_0_IRQn);

	/* Turn on the clock - 48MHz */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 9);

	/* 2kHz */
	timer->PR = 24000;

	/* 10 ms overflow */
	timer->MR3 = 20;
	timer->MCR = (1 << 9) | (1 << 10);
	timer->TCR = 0x1;
}

const uint32_t anim_length = 2048;
volatile uint32_t anim_start;
uint16_t state[N_CHANNELS];
uint16_t start[N_CHANNELS];
uint16_t target[N_CHANNELS];

uint32_t ms_since(uint32_t since)
{
	uint32_t now = msTicks;
	if (now < since) {
		return now + (0xffffffff - since) + 1;
	}

	return now - since;
}

uint32_t lerp(uint32_t from, uint32_t to, uint32_t pos, uint32_t max)
{
	int32_t diff = to - from;
	uint32_t x = (pos << 16) / max;

	if (pos >= max) {
		return to;
	}

	return from + ((diff * x) / 65536);
}

void TIMER_32_0_Handler(void)
{
	uint32_t status = LPC_CT32B0->IR;
	if (status & (1 << 3)) {
		uint32_t tmp = ms_since(anim_start);

		uint32_t i, val = 0;
		for (i = 0; i < N_CHANNELS; i++) {
			if (state[i] != target[i]) {
				val = lerp(start[i], target[i], tmp, anim_length);
				state[i] = val;
				pwm_set(channel_map[i], val);
			}
		}

		pwm_flip();
	}
	LPC_CT32B0->IR = status;
}

void display(uint8_t sentence)
{
	int i;

	NVIC_DisableIRQ(TIMER_32_0_IRQn);
	for (i = 0; i < N_CHANNELS; i++) {
		start[i] = state[i];
		if (sentence & (1 << i)) {
			target[i] = brightness;
		} else {
			target[i] = 0;
		}
	}
	anim_start = msTicks;
	NVIC_EnableIRQ(TIMER_32_0_IRQn);
}

void button_init(void)
{
	// Configure as falling edge interrupt
	// On falling edge, start timer for 30ms
	// After 30ms enable high-level interrupt, set timer with 3s timeout
	// On high level, stop timer - pressed
	// One timer expire, stop timer - held

	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 19);

	NVIC_SetPriority(FLEX_INT0_IRQn, 3);
	NVIC_EnableIRQ(FLEX_INT0_IRQn);

	LPC_SYSCON->PINTSEL[0] = 1;
	LPC_GPIO_PIN_INT->FALL = (1 << 0);
	LPC_GPIO_PIN_INT->SIENF = (1 << 0);

	LPC_CTxxBx_Type *timer = LPC_CT16B1;

	NVIC_SetPriority(TIMER_16_1_IRQn, 3);
	NVIC_EnableIRQ(TIMER_16_1_IRQn);

	/* Turn on the clock - 48MHz */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 8);

	/* Ticks at ~732.4 Hz */
	timer->PR = 65535;
}

void sleep_until(uint8_t day, uint16_t time)
{
	uint32_t sleep_start;
	struct rtc_date date;
	while (1) {
		rtc_read_date(&date);
		if (date.day == day) {
			if (TIME(date.hours, date.minutes) >= time) {
				return;
			}
		}

		sleep_start = msTicks;
		while ((ms_since(sleep_start)) < (1000 * 60));
	}
}

int parse_time(char *buf, char **saveptr, uint16_t *time)
{
	char *tok = strtok_r(buf, ":", saveptr);
	uint8_t h, m;
	if (tok == NULL) {
		return -1;
	}
	if (strlen(tok) != 2) {
		return -1;
	}
	h = ((tok[0] - '0') << 4) | (tok[1] - '0');

	tok = strtok_r(NULL, "\r\n", saveptr);
	if (strlen(tok) != 2) {
		return -1;
	}
	m = ((tok[0] - '0') << 4) | (tok[1] - '0');

	*time = TIME(h, m);

	return 0;
}

int parse_u16_hex(char *buf, char **saveptr, uint16_t *val)
{
	char *tok = strtok_r(buf, " \r", saveptr);
	uint16_t tmp = 0;
	int i;
	if (tok == NULL) {
		return -1;
	}
	if (strlen(tok) != 6) {
		return -1;
	}

	for (i = 2; i < 6; i++) {
		tmp <<= 4;

		if ((tok[i] >= 'a') && (tok[i] <= 'f')) {
			tmp |= (tok[i] - 'a') + 0xA;
		} else if ((tok[i] >= 'A') && (tok[i] <= 'F')) {
			tmp |= (tok[i] - 'A') + 0xA;
		} else if ((tok[i] >= '0') && (tok[i] <= '9')) {
			tmp |= (tok[i] - '0');
		}
	}

	*val = tmp;

	return 0;
}

/* 2019-12-25-12:25:00 */
int parse_datetime(char *buf, char **saveptr, struct rtc_date *date)
{
	char *tok = strtok_r(buf, "-", saveptr);
	if (tok == NULL) {
		return -1;
	}
	if (strlen(tok) != 4) {
		return -1;
	}
	date->year = ((tok[2] - '0') << 4) | (tok[3] - '0');

	tok = strtok_r(NULL, "-", saveptr);
	if (tok == NULL) {
		return -1;
	}
	if (strlen(tok) != 2) {
		return -1;
	}
	date->month = ((tok[0] - '0') << 4) | (tok[1] - '0');

	tok = strtok_r(NULL, "-", saveptr);
	if (tok == NULL) {
		return -1;
	}
	if (strlen(tok) != 2) {
		return -1;
	}
	date->date = ((tok[0] - '0') << 4) | (tok[1] - '0');

	tok = strtok_r(NULL, ":", saveptr);
	if (tok == NULL) {
		return -1;
	}
	if (strlen(tok) != 2) {
		return -1;
	}
	date->hours = ((tok[0] - '0') << 4) | (tok[1] - '0');

	tok = strtok_r(NULL, ":", saveptr);
	if (tok == NULL) {
		return -1;
	}
	if (strlen(tok) != 2) {
		return -1;
	}
	date->minutes = ((tok[0] - '0') << 4) | (tok[1] - '0');

	tok = strtok_r(NULL, " \r", saveptr);
	if (tok == NULL) {
		return -1;
	}
	if (strlen(tok) != 2) {
		return -1;
	}
	date->seconds = ((tok[0] - '0') << 4) | (tok[1] - '0');

	return 0;
}

int set_meal(int meal, char *buf, char **saveptr)
{
	int ret;
	uint16_t time;

	ret = parse_time(buf, saveptr, &time);
	if (ret) {
		return ret;
	}

	return iap_eeprom_write(EEPROM_TIME_OFFSET + (meal * 2), &time, 2);
}

int get_meal(int meal)
{
	int ret;
	char buf[10];
	int idx = 0;
	uint16_t time = 0;

	ret = iap_eeprom_read(EEPROM_TIME_OFFSET + (meal * 2), &time, 2);
	if (ret) {
		return ret;
	}

	buf[idx++] = ((time >> 12) & 0xf) + '0';
	buf[idx++] = ((time >> 8) & 0xf) + '0';
	buf[idx++] = ':';

	buf[idx++] = ((time >> 4) & 0xf) + '0';
	buf[idx++] = ((time >> 0) & 0xf) + '0';
	buf[idx++] = '\0';
	usb_usart_print("\r\n");
	usb_usart_print(buf);
	usb_usart_print("\r\n");

	return 0;
}

int handle_set_command(char *buf, char **saveptr)
{
	int ret = -1;
	char *tok = strtok_r(buf, " ", saveptr);
	if (tok == NULL) {
		return -1;
	}

	if (!strcmp(tok, "TIME")) {
		struct rtc_date date = { 0 };

		ret = parse_datetime(NULL, saveptr, &date);
		if (ret) {
			return ret;
		}

		usb_usart_print("\r\n");
		usb_usart_print(rtc_date_to_str(&date));
		rtc_write_date(&date);
		usb_usart_print("\r");
	} else if (!strcmp(tok, "BREAKFAST")) {
		ret = set_meal(BREAKFAST, NULL, saveptr);
	} else if (!strcmp(tok, "LUNCH")) {
		ret = set_meal(LUNCH, NULL, saveptr);
	} else if (!strcmp(tok, "HOME")) {
		ret = set_meal(HOME, NULL, saveptr);
	} else if (!strcmp(tok, "DINNER")) {
		ret = set_meal(DINNER, NULL, saveptr);
	} else if (!strcmp(tok, "BED")) {
		ret = set_meal(BED, NULL, saveptr);
	} else if (!strcmp(tok, "SLEEP")) {
		ret = set_meal(SLEEP, NULL, saveptr);
	} else if (!strcmp(tok, "NEARLY")) {
		ret = set_meal(NEARLY, NULL, saveptr);
	} else if (!strcmp(tok, "PAST")) {
		ret = set_meal(PAST, NULL, saveptr);
	} else if (!strcmp(tok, "BRIGHTNESS")) {
		uint16_t val;
		ret = parse_u16_hex(NULL, saveptr, &val);
		if (ret) {
			return ret;
		}
		brightness = val;
		ret = iap_eeprom_write(EEPROM_BRIGHTNESS_OFFSET, &val, 2);
	}

	if (!ret) {
		dirty = true;
		usb_usart_print("\nOK\r\n");
	}

	return ret;
}

int handle_get_command(char *buf, char **saveptr)
{
	int ret = -1;
	char *tok = strtok_r(buf, "\r", saveptr);
	if (tok == NULL) {
		return -1;
	}

	if (!strcmp(tok, "TIME")) {
		struct rtc_date date = { 0 };
		rtc_read_date(&date);

		usb_usart_print("\r\n");
		usb_usart_print(rtc_date_to_str(&date));
		usb_usart_print("\r\n");
		ret = 0;
	} else if (!strcmp(tok, "BREAKFAST")) {
		ret = get_meal(BREAKFAST);
	} else if (!strcmp(tok, "LUNCH")) {
		ret = get_meal(LUNCH);
	} else if (!strcmp(tok, "HOME")) {
		ret = get_meal(HOME);
	} else if (!strcmp(tok, "DINNER")) {
		ret = get_meal(DINNER);
	} else if (!strcmp(tok, "BED")) {
		ret = get_meal(BED);
	} else if (!strcmp(tok, "SLEEP")) {
		ret = get_meal(SLEEP);
	} else if (!strcmp(tok, "NEARLY")) {
		ret = get_meal(NEARLY);
	} else if (!strcmp(tok, "PAST")) {
		ret = get_meal(PAST);
	} else if (!strcmp(tok, "BRIGHTNESS")) {
		uint16_t val;
		char str[] = "\r\n0xXXXXXXXX\r\n";
		ret = iap_eeprom_read(EEPROM_BRIGHTNESS_OFFSET, &val, 2);
		if (ret) {
			return ret;
		}
		u32_to_str(val, &str[4]);
		usb_usart_print(str);
	}

	return ret;
}

int handle_command(char *buf)
{
	int ret = 0;
	char *saveptr;
	char *tok = strtok_r(buf, " \r", &saveptr);

	if (tok == NULL) {
		return -1;
	}

	if (!strcmp(tok, "RESET")) {
		usb_usart_print("\nRESET\r\n");
		NVIC_SystemReset();
		return 0;
	} else if (!strcmp(tok, "ID")) {
		uint32_t *result;
		char str[9];
		str[8] = '\0';

		ret = iap_read_partid(&result);
		if (ret) {
			return ret;
		}
		usb_usart_print("\nPARTID: ");
		u32_to_str(result[0], str);
		usb_usart_print(str);
		usb_usart_print("\r");

		ret = iap_read_uid(&result);
		if (ret) {
			return ret;
		}
		usb_usart_print("\nUID: ");
		u32_to_str(result[0], str);
		usb_usart_print(str);
		usb_usart_print(" ");
		u32_to_str(result[1], str);
		usb_usart_print(str);
		usb_usart_print(" ");
		u32_to_str(result[2], str);
		usb_usart_print(str);
		usb_usart_print(" ");
		u32_to_str(result[3], str);
		usb_usart_print(str);
		usb_usart_print("\r\n");
		return 0;
	} else if (!strcmp(tok, "SET")) {
		return handle_set_command(NULL, &saveptr);
	} else if (!strcmp(tok, "GET")) {
		return handle_get_command(NULL, &saveptr);
	}

	return -1;
}

void handle_usart()
{
	static char buf[32];
	static unsigned int len = 0;
	int ret = 0;

	if (!usb_usart_dtr()) {
		return;
	}

	while ((len < sizeof(buf)) && (buf[len - 1] != '\r')) {
		ret = usb_usart_recv(&buf[len], 1, 0);
		if (!ret) {
			break;
		}
		usb_usart_send(&buf[len], 1);
		len += ret;
	}

	if (buf[len - 1] == '\r') {
		ret = handle_command(buf);
		len = 0;
	} else if (len == sizeof(buf)) {
		usb_usart_flush_rx();
		len = 0;
		ret = -1;
	}

	if (ret) {
		usb_usart_print("\nERR\r\n");
	}
}

int main(void)
{
	char buf[11] = "        \r\n";
	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/1000);

#ifdef DEBUG
	LPC_GPIO->DIR[DBG_PORT] |= DBG_PIN | DBG_PIN1;
	LPC_GPIO->CLR[DBG_PORT] = DBG_PIN | DBG_PIN1;
#endif

	button_init();
	rtc_init();
	usb_init();
	leds_init();
	init_timer32();

	delay_ms(100);

	init_timer16();

	int ret, i = 0;

	uint32_t magic = 0;
	ret = iap_eeprom_read(0, &magic, 4);
	if (ret == 0) {
		if (magic == EEPROM_MAGIC) {
			/* Load times from EEPROM */
			for (i = 0; i < N_TIMES; i++) {
				ret = iap_eeprom_read(EEPROM_TIME_OFFSET + (i * 2), &times[i], 2);
				if (ret != 0) {
					break;
				}
			}
			iap_eeprom_read(EEPROM_BRIGHTNESS_OFFSET, &brightness, 2);
		} else {
			/* Store defaults to EEPROM */
			for (i = 0; i < N_TIMES; i++) {
				ret = iap_eeprom_write(EEPROM_TIME_OFFSET + (i * 2), &times[i], 2);
				if (ret != 0) {
					break;
				}
			}
			ret = iap_eeprom_write(EEPROM_BRIGHTNESS_OFFSET, &brightness, 2);
			if (ret == 0) {
				magic = EEPROM_MAGIC;
				iap_eeprom_write(0, &magic, 4);
			}
		}
	}

	build_timebands();

	struct rtc_date date;
	uint8_t sentence = 0;
	int band = 0;
	uint32_t demo_last_change = 0;
	while(1) {
		if (button == PRESSED) {
			if (mode == NORMAL) {
				mode = BLANKED;
			} else if (mode == BLANKED) {
				mode = NORMAL;
			}
			dirty = true;
			button = NONE;
		} else if (button == HELD) {
			if (mode == DEMO) {
				mode = NORMAL;
			} else if ((mode == NORMAL) || (mode == BLANKED)) {
				mode = DEMO;
			}
			dirty = true;
			button = NONE;
		}

		if (mode == DEMO) {
			if (ms_since(demo_last_change) >= anim_length + 512) {
				demo_last_change = msTicks;
				band++;
			}
			if (band >= n_timebands) {
				band = 0;
			}
		} else {
			rtc_read_date(&date);
			uint16_t time = TIME(date.hours, date.minutes);
			for (i = 0; i < n_timebands; i++) {
				if (time >= timebands[i].start) {
					band = i;
				}
			}
		}

		if (timebands[band].sentence != sentence) {
			sentence = timebands[band].sentence;
			if ((mode == BLANKED) &&
			    (sentence & (BIT(NEARLY)) &&
			    (sentence & (BIT(BREAKFAST))))) {
				mode = NORMAL;
			}
			dirty = true;
		}

		handle_usart();

		if (dirty) {
			if (mode == BLANKED) {
				brightness = 0;
			} else {
				iap_eeprom_read(EEPROM_BRIGHTNESS_OFFSET, &brightness, 2);
			}
			display(timebands[band].sentence);
			dirty = false;
		}

		delay_ms(10);
	}

	return 0;
}
