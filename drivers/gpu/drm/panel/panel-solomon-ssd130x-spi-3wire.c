// SPDX-License_identifier: GPL-2.0
#include "panel-solomon-ssd130x.h"

#define COMMAND 0
#define DATA 1


struct ssd130x_spi_9bit_word {
	uint8_t dc;
	uint8_t byte;
} __attribute__ ((packed));


static int ssd130x_command_single(struct ssd130x_panel *ssd130x,
				  uint8_t cmd)
{
	struct ssd130x_spi_9bit_word *cmd_buf;
	int ret;
	size_t word_size = sizeof(struct ssd130x_spi_9bit_word);

	cmd_buf = kzalloc(word_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);

	cmd_buf->dc = COMMAND;
	cmd_buf->byte = cmd;

	ret = spi_write(ssd130x->spi, cmd_buf, word_size);

	kfree(cmd_buf);

	return ret;
}

static int ssd130x_command_1_param(struct ssd130x_panel *ssd130x,
				   uint8_t cmd,
				   uint8_t param)
{
	struct ssd130x_spi_9bit_word *cmd_buf;
	int ret;
	size_t words = 2;
	size_t word_size = sizeof(struct ssd130x_spi_9bit_word);
	size_t cmd_buf_size = words * word_size;

	cmd_buf = kcalloc(words, word_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);
	
	cmd_buf[0].dc = COMMAND;
	cmd_buf[0].byte = cmd;
	cmd_buf[1].dc = COMMAND;
	cmd_buf[1].byte = param;

	ret = spi_write(ssd130x->spi, cmd_buf, cmd_buf_size);
	kfree(cmd_buf);

	return ret;
}

static int ssd130x_command_2_params(struct ssd130x_panel *ssd130x,
				    uint8_t cmd,
				    uint8_t param1,
				    uint8_t param2)
{
	struct ssd130x_spi_9bit_word *cmd_buf;
	int ret;
	size_t words = 3;
	size_t word_size = sizeof(struct ssd130x_spi_9bit_word);
	size_t cmd_buf_size = words * word_size;

	cmd_buf = kcalloc(words, word_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);
	
	cmd_buf[0].dc = COMMAND;
	cmd_buf[0].byte = cmd;
	cmd_buf[1].dc = COMMAND;
	cmd_buf[1].byte = param1;
	cmd_buf[2].dc = COMMAND;
	cmd_buf[2].byte = param2;

	ret = spi_write(ssd130x->spi, cmd_buf, cmd_buf_size);
	kfree(cmd_buf);

	return ret;
}
