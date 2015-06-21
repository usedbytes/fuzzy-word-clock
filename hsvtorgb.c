/*
 * Based on
 * http://web.mit.edu/storborg/Public/hsvtorgb.c
 * HSV to RGB conversion function with only integer
 * math
 */
#include <stdint.h>

#define G_SHIFT 16
#define R_SHIFT 8
#define B_SHIFT 0

#define COLOR(__r, __g, __b) ((__r << R_SHIFT) |\
		(__g << G_SHIFT) | (__b << B_SHIFT))

uint32_t hsvtorgb(unsigned char h, unsigned char s, unsigned char v)
{
    unsigned char region, fpart, p, q, t;

    if(s == 0) {
        /* color is grayscale */
		v >>= 1;
        return COLOR(v, v, v);
    }

    /* make hue 0-5 */
    region = h / 43;
    /* find remainder part, make it from 0-255 */
    fpart = (h - (region * 43)) * 6;

    /* calculate temp vars, doing integer multiplication */
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * fpart) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - fpart)) >> 8))) >> 8;

    /* assign temp vars based on color cone region */
    switch(region) {
        case 0:
			return COLOR(v >> 1, t >> 1, p >> 1);
        case 1:
			return COLOR(q >> 1, v >> 1, p >> 1);
        case 2:
			return COLOR(p >> 1, v >> 1, t >> 1);
        case 3:
			return COLOR(p >> 1, q >> 1, v >> 1);
        case 4:
			return COLOR(t >> 1, p >> 1, v >> 1);
        default:
			return COLOR(v >> 1, p >> 1, q >> 1);
    }
}
