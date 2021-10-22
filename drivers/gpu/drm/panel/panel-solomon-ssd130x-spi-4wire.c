// SPDX-License-Identifier: GPL-2.0

#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include "panel-solomon-ssd130x.h"


static int ssd130x_spi_4wire_command(struct ssd130x_panel *ssd130x,
				     uint8_t *cmd_buf,
				     size_t bytes)
{
	int ret;

	gpiod_set_value_cansleep(ssd130x->dc, 0);
	ret = spi_write(ssd130x->spi, cmd_buf, bytes);
	/* Prevent SPI line interference from writing to the command buffer. */
	gpiod_set_value_cansleep(ssd130x->dc, 1);

	return ret;
}

static int ssd130x_command_single(struct ssd130x_panel *ssd130x,
				  uint8_t cmd)
{
	int ret;
	size_t cmd_buf_size = sizeof(cmd);

	uint8_t *cmd_buf = kzalloc(cmd_buf_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);

	*cmd_buf = cmd;

	ret = ssd130x_spi_4wire_command(ssd130x, cmd_buf, cmd_buf_size);

	kfree(cmd_buf);

	return ret;
}

static int ssd130x_command_1_param(struct ssd130x_panel *ssd130x,
				   uint8_t cmd,
				   uint8_t param)
{
	int ret;
	size_t words = 2;
	size_t word_size = sizeof(uint8_t);
	size_t cmd_buf_size = words * word_size;

	uint8_t *cmd_buf = kcalloc(words, word_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);
	
	cmd_buf[0] = cmd;
	cmd_buf[1] = param;

	ret = ssd130x_spi_4wire_command(ssd130x, cmd_buf, cmd_buf_size);

	kfree(cmd_buf);

	return ret;
}

static int ssd130x_command_2_params(struct ssd130x_panel *ssd130x,
				    uint8_t cmd,
				    uint8_t param1,
				    uint8_t param2)
{
	int ret;
	size_t words = 3;
	size_t word_size = sizeof(uint8_t);
	size_t cmd_buf_size = words * word_size;

	uint8_t *cmd_buf = kcalloc(words, word_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);

	cmd_buf[0] = cmd;
	cmd_buf[1] = param1;
	cmd_buf[2] = param2;

	ret = ssd130x_spi_4wire_command(ssd130x, cmd_buf, cmd_buf_size);

	kfree(cmd_buf);

	return ret;
}
