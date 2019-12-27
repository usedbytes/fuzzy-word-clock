#include "lut.h"

#define LUT_COEFF(c, dy) (((c) << 16) | (((uint16_t)((dy) * (1 << 10))) & 0xffff))

const uint32_t lut[32] = {
	LUT_COEFF(0, 0.11083984375),
	LUT_COEFF(227, 0.11083984375),
	LUT_COEFF(454, 0.11328125),
	LUT_COEFF(686, 0.1396484375),
	LUT_COEFF(972, 0.173828125),
	LUT_COEFF(1328, 0.2119140625),
	LUT_COEFF(1762, 0.25341796875),
	LUT_COEFF(2281, 0.29931640625),
	LUT_COEFF(2894, 0.34814453125),
	LUT_COEFF(3607, 0.4013671875),
	LUT_COEFF(4429, 0.4580078125),
	LUT_COEFF(5367, 0.5185546875),
	LUT_COEFF(6429, 0.5830078125),
	LUT_COEFF(7623, 0.65087890625),
	LUT_COEFF(8956, 0.72265625),
	LUT_COEFF(10436, 0.79833984375),
	LUT_COEFF(12071, 0.87744140625),
	LUT_COEFF(13868, 0.96044921875),
	LUT_COEFF(15835, 1.04736328125),
	LUT_COEFF(17980, 1.13818359375),
	LUT_COEFF(20311, 1.23193359375),
	LUT_COEFF(22834, 1.330078125),
	LUT_COEFF(25558, 1.43212890625),
	LUT_COEFF(28491, 1.53759765625),
	LUT_COEFF(31640, 1.64697265625),
	LUT_COEFF(35013, 1.76025390625),
	LUT_COEFF(38618, 1.87646484375),
	LUT_COEFF(42461, 1.99755859375),
	LUT_COEFF(46552, 2.12158203125),
	LUT_COEFF(50897, 2.25),
	LUT_COEFF(55505, 2.38134765625),
	LUT_COEFF(60382, 2.51611328125),
};

uint16_t lut1d(uint16_t x)
{
	int node = x / 2048;
	uint32_t coeff = lut[node];
	uint32_t xoffs = x % 2048;
	uint32_t yoffs = ((coeff & 0xffff) * xoffs) >> 10;

	return (coeff >> 16) + yoffs;
}

/* Test
#include <stdio.h>
#include <math.h>
uint16_t cie1931(uint16_t x)
{
	double L = ((double)(x) / 65535.0) * 100.0;
	if (L <= 8) {
		return (uint16_t)round((L / 902.3) * 65535);
	}
	return (uint16_t)round(pow(((L + 16.0) / 116.0), 3) * 65535);
}

int main(int argc, char *argv[])
{
	int i;
	for (i = 0; i < 65536; i++) {
		printf("%d, %d, %d\n", i, cie1931(i), lut1d(i));
	}

	return 0;
}
*/
