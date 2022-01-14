/* SPDX-License-Identifier: GPL-2.0 */

#include "panel-solomon-ssd130x.h"

/**
 * ssd130x_spi_setup() - 
 * @ssd130x: The ssd130x_panel whose SPI should be set up.
 * @three_wire: Boolean to indicate if the SPI bus should be set up according to
 * the displays 3-wire mode with 9 bits per word.
 * False means 4-wire mode with 8 bits per word.
 * 
 * Returns zero on success, -EPROTONOSUPP if the required bits per word are
 * not supported on the SPI controller. For other errors see spi_setup(). 
 */
int ssd130x_spi_setup(struct ssd130x_panel *ssd130x, bool three_wire);
