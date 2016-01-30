/*
 * LPD8806 segment display implementation
 *
 * Copyright Brian Starkey 2015 <stark3y@gmail.com>
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
#include "lpd8806.h"
#include "spi.h"
#include "segment_display.h"
#include "util.h"

#define N_LEDS 26
#define N_SEGMENTS 8
#define FADE_STEP 2

extern uint32_t hsvtorgb(unsigned char h, unsigned char s, unsigned char v);

uint32_t framebuffer[N_LEDS] = { 0 };

struct lpd8806_segment {
	int start;
	int len;
};

struct lpd8806_segment segments[N_SEGMENTS] = {
	{0, 4},
	{4, 4},
	{8, 3},
	{11, 3},
	{14, 3},
	{17, 3},
	{20, 3},
	{23, 3},
};

struct segment_display {
	struct spi_dev *spi;
	struct lpd8806_segment *segments;
} lpd8806_display;

typedef uint32_t segment_mask;

void lpd8806_set_segment(struct segment_display *disp,
						 struct lpd8806_segment *segment,
						 uint32_t value)
{
	int nleds = segment->len;
	while (nleds--) {
		framebuffer[segment->start + nleds] = hsvtorgb(0, 0, value);
	}
}

struct segment_display *display_init(struct spi_dev *spi)
{
	lpd8806_display.spi = spi;
	lpd8806_display.segments = segments;
	return &lpd8806_display;
}

void display_transition(struct segment_display *disp,
		segment_mask from, segment_mask to)
{
	uint32_t bring_in = to & ~from;
	uint32_t fade_out = from & ~to;
	uint32_t in_val = 0;
	uint32_t out_val = 0x7f;
	int i;

	for (in_val = 0; in_val <= 0x7f; in_val += FADE_STEP,
	                                 out_val -= FADE_STEP) {
		for (i = 0; i < N_SEGMENTS; i++) {
			if (bring_in & (1 << i)) {
				lpd8806_set_segment(disp, &disp->segments[i], in_val);
			}
			if (fade_out & (1 << i)) {
				lpd8806_set_segment(disp, &disp->segments[i], out_val);
			}
		}
		lpd8806_update(disp->spi, framebuffer, N_LEDS);
		delay_ms(16);
	}
	for (i = 0; i < N_SEGMENTS; i++) {
		if (bring_in & (1 << i)) {
			lpd8806_set_segment(disp, &disp->segments[i], 0x7f);
		}
		if (fade_out & (1 << i)) {
			lpd8806_set_segment(disp, &disp->segments[i], 0);
		}
	}
	lpd8806_update(disp->spi, framebuffer, N_LEDS);
}
