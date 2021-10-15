// SPDX-License_identifier: GPL-2.0

#include <linux/i2c.h>
#include <linux/slab.h>

#include "panel-solomon-ssd130x.h"


#define DATA 0x40
#define CONTINUATION 0x80


struct ssd130x_i2c_word {
	uint8_t control_byte;
	uint8_t data;
} __attribute__((packed));


static int ssd130x_copy_data_to_i2c_word_array(size_t bytes,
					       uint8_t *src,
					       struct ssd130x_i2c_word *dst)
{
	int i;
	int ret;

	for (i = 0; i < bytes; ++i) {
		dst[i].control_byte = (CONTINUATION | DATA);
		dst[i].data = src[i];
	}

	return ret;
}

static int ssd130x_command_single(struct ssd130x_panel *ssd130x,
				  uint8_t cmd)
{
	struct ssd130x_i2c_word *cmd_buf;
	int ret;
	size_t word_size = sizeof(struct ssd130x_i2c_word);

	cmd_buf = kzalloc(word_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);

	cmd_buf->control_byte = CONTINUATION;
	cmd_buf->data = cmd;

	ret = i2c_master_send_dmasafe(ssd130x->i2c, cmd_buf, word_size);
	kfree(cmd_buf);

	if (ret < 0)
		return ret;

	return 0;
}

static int ssd130x_command_1_param(struct ssd130x_panel *ssd130x,
				   uint8_t cmd,
				   uint8_t param)
{
	struct ssd130x_i2c_word *cmd_buf;
	int ret;
	size_t words = 2;
	size_t word_size = sizeof(struct ssd130x_i2c_word);
	size_t cmd_buf_size = words * word_size;

	cmd_buf = kcalloc(words, word_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);

	cmd_buf[0].control_byte = CONTINUATION;
	cmd_buf[0].data = cmd;
	cmd_buf[1].control_byte = CONTINUATION;
	cmd_buf[1].data = param;

	ret = i2c_master_send_dmasafe(ssd130x->i2c, cmd_buf, cmd_buf_size);
	kfree(cmd_buf);

	if (ret < 0)
		return ret;

	return 0;
}

static int ssd130x_command_2_params(struct ssd130x_panel *ssd130x,
				    uint8_t cmd,
				    uint8_t param1,
				    uint8_t param2)
{
	struct ssd130x_i2c_word *cmd_buf;
	int ret;
	size_t words = 3;
	size_t word_size = sizeof(struct ssd130x_i2c_word);
	size_t cmd_buf_size = words * word_size;

	cmd_buf = kcalloc(words, word_size, GFP_KERNEL);
	if (IS_ERR(cmd_buf))
		return PTR_ERR(cmd_buf);

	cmd_buf[0].control_byte = CONTINUATION;
	cmd_buf[0].data = cmd;
	cmd_buf[1].control_byte = CONTINUATION;
	cmd_buf[1].data = param1;
	cmd_buf[2].control_byte = CONTINUATION;
	cmd_buf[2].data = param2;

	ret = i2c_master_send_dmasafe(ssd130x->i2c, cmd_buf, cmd_buf_size);
	kfree(cmd_buf);

	if (ret < 0)
		return ret;

	return 0;
}
