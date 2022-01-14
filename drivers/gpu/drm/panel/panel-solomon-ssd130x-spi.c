// SPDX-License-Identifier: GPL-2.0

#include <linux/spi/spi.h>

#include "panel-solomon-ssd130x.h"
#include "panel-solomon-ssd130x-spi.h"


int ssd130x_spi_setup(struct ssd130x_panel *ssd130x, bool three_wire)
{
	struct spi_device *spi = ssd130x->spi;
	uint8_t bits_per_word;
	
	spi_set_drvdata(spi, ssd130x);
	
	spi->max_speed_hz = 10000000; // 10 MHz, except SSD1305 with 4 MHz
	spi->mode = SPI_MODE_0 // All displays (1305-9) operate in SPI mode 0.
		    | SPI_NO_RX; // SPI is transmit only.

	if (three_wire) {
		bits_per_word = 9; // 3-wire data word size
	} else {
		bits_per_word = 8; // 4-wire data word size
	}

	if (!spi_is_bpw_supported(spi, bits_per_word)) {
		dev_err(&spi->dev,
		"host does not support %d bits per word transfers\n",
		bits_per_word);
		return -EPROTONOSUPPORT;
	}
	spi->bits_per_word = bits_per_word;

	return spi_setup(spi);
}
