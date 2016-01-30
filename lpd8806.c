/* Basic LPD8806 Interface
 *
 * Copyright Brian Starkey <starkey@gmail.com> 2015
 */
#include "lpd8806.h"

/* Little endian only... (and byte-addressable memory) */
static inline void send_color(struct spi_dev *dev, uint32_t color)
{
	color |= 0x808080;
	spi_transfer(dev, (uint8_t *)&color, NULL, 3);
}

void lpd8806_init(struct spi_dev *dev, int n_leds)
{
	int n = n_leds;
	/* I don't know why two resets are necessary */
	lpd8806_reset(dev, n_leds);
	while(n--)
		send_color(dev, 0);
	lpd8806_reset(dev, n_leds);
}

void lpd8806_reset(struct spi_dev *dev, int n_leds)
{
	while (n_leds > 0) {
		spi_transfer(dev, NULL, NULL, 1);
		n_leds -= 32;
	}
}

void lpd8806_update(struct spi_dev *dev, uint32_t *colors, int n_leds)
{
	int n = n_leds;
	while (n--)
		send_color(dev, *colors++);
	lpd8806_reset(dev, n_leds);
}
