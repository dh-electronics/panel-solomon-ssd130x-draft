// SPDX-License-Identifier: GPL-2.0

#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include "panel-solomon-ssd130x.h"
#include "panel-solomon-ssd130x-backlight.h"


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

static int ssd130x_spi_4wire_probe(struct spi_device *spi)
{
	struct device_node *node = spi->dev.of_node;
	struct device *dev = &spi->dev;
	struct ssd130x_panel *ssd130x;

	int ret;

	ret = ssd130x_setup_dma_mask(dev);
	if (ret)
		return ret;

	ssd130x = devm_kzalloc(dev, sizeof(struct ssd130x_panel), GFP_KERNEL);
	if (!ret)
		return -ENOMEM;

	ssd130x->spi = spi;

	ssd130x_bus_independent_probe(ssd130x, dev, node);

	ssd130x->dc = devm_gpiod_get(dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(ssd130x->dc)) {
		DRM_DEV_ERROR(dev, "Failed to get gpio 'dc' (data/command)\n");
		ret = PTR_ERR(ssd130x->dc);
		return ret;
	}

	drm_panel_init(&ssd130x->panel, dev,
		       &ssd130x_panel_funcs,
		       DRM_MODE_CONNECTOR_SPI);

	/* The backlight is a software backlight, hence the initialization after
	 * the panel initialization
	 */
	ret = ssd130x_backlight_register(ssd130x);
	if (ret)
		return ret;

	drm_panel_add(&ssd130x->panel);

	return 0;
}

static int ssd130x_spi_4wire_remove(struct spi_device *spi)
{
	struct ssd130x_panel *ssd130x = spi_get_drvdata(spi);
	int ret;

	ret = drm_panel_disable(&ssd130x->panel);
	if (ret < 0)
		dev_err(&ssd130x->dev,
			"failed to disable panel during removal, %d\n", ret);

	ret = drm_panel_unprepare(&ssd130x->panel);
	if (ret < 0)
		dev_err(&ssd130x->dev,
			"failed to unprepare panel during removal, %d\n", ret);

	drm_panel_remove(&ssd130x->panel);

	return 0;
}

static const struct spi_device_id ssd130x_ids[] = {
	{ "ssd1306", 0},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(spi, ssd130x_ids);

static const struct spi_driver ssd130x_spi_4wire_driver = {
	.id_table = ssd130x_ids,
	.probe = ssd130x_spi_4wire_probe,
	.remove = ssd130x_spi_4wire_remove,
	.driver = { 
		.name = "ssd130x",
		.owner = THIS_MODULE,
		.of_match_table = ssd130x_of_match,
	},
};
module_spi_driver(ssd130x_spi_4wire_driver);

MODULE_AUTHOR("Dominik Kierner <dkierner@dh-electronics.com>");
MODULE_DESCRIPTION("Solomon SSD130x panel 4-wire SPI driver");
MODULE_LICENSE("GPL v2");
