// SPDX-License-Identifier: GPL-2.0

#include <linux/backlight.h>

#include "panel-solomon-ssd130x.h"
#include "panel-solomon-ssd130x-backlight.h"


static int ssd130x_update_backlight(struct backlight_device *bdev)
{
	struct ssd130x_panel *ssd130x = bl_get_data(bdev);
	int ret;
	int brightness = bdev->props.brightness;

	ssd130x->display_settings.contrast = brightness;

	return ssd130x_command_1_param(ssd130x,
				       SSD130X_SET_CONTRAST_CONTROL,
				       ssd130x->display_settings.contrast);
};

static int ssd130x_get_brightness(struct backlight_device *bdev)
{
	struct ssd130x_panel *ssd130x = bl_get_data(bdev);

	return ssd130x->display_settings.contrast;
};

static const struct backlight_ops ssd130x_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ssd130x_update_backlight,
	.get_brightness = ssd130x_get_brightness
};


static const struct backlight_properties ssd130x_backlight_props = {
	.brightness = HALF_CONTRAST,
	.max_brightness = MAX_CONTRAST,
	.type = BACKLIGHT_RAW,
	.scale = BACKLIGHT_SCALE_UNKNOWN /* TODO: Update when known */
};


int ssd130x_backlight_register(struct ssd130x_panel *ssd130x)
{
	struct device *dev = ssd130x->dev;
	int ret;

	ssd130x->backlight =
			devm_backlight_device_register(
					dev, dev_name(dev),
					dev, ssd130x,
					&ssd130x_backlight_ops,
					&ssd130x_backlight_props);
	if (IS_ERR(ssd130x->backlight)) {
		ret = PTR_ERR(ssd130x->backlight);
		dev_err(dev, "Unable to register backlight device (%d)\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ssd130x_backlight_register);
