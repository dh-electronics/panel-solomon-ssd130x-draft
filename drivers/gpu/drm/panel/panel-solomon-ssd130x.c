// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include "panel-solomon-ssd130x.h"


int ssd130x_bus_independent_probe(struct ssd130x_panel *ssd130x,
				  struct device *dev,
				  struct device_node *node)
{
	const struct ssd130x_panel_info *const device_info =
		of_device_get_match_data(dev);
	int ret;


	ssd130x->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ssd130x->reset)) {
		DRM_DEV_ERROR(dev, "Failed to get gpio 'reset'\n");
		ret = PTR_ERR(ssd130x->reset);
		return ret;
	}

	ssd130x->dc = devm_gpiod_get_optional(dev, "dc_sa0", GPIOD_OUT_LOW);
	if (IS_ERR(ssd130x->dc)) {
		DRM_DEV_ERROR(dev, "Failed to get gpio 'dc_sa0'\n");
		ret = PTR_ERR(ssd130x->dc);
		return ret;
	}

	/* TODO: ENODEV should be returned to signal
	 * that the critical section cannot be entered
	 * because the device has been unplugged.
	 */


	/* Get core logic supply */
	ssd130x->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(ssd130x->vdd)) {
		ret = PTR_ERR(ssd130x->vdd);
		return ret;
	}

	/*
	 * TODO: Evaluate registering VBAT via alias
	 *
	 * If this is possible, the use_charge_pump flag could be
	 * autoconfigured by evaluating the regulators.
	 * This could potentially be cleaner than probing for the optional vbat.
	 *
	ret = devm_regulator_register_supply_alias(ssd130x_dev->vcc, "vcc",
						   ssd130x_dev->vbat , "vbat");
	 */

	/* Get optional external charge pump regulator
	 *
	 * This is a specific for the SSD1306 display.
	 *
	 * The SSD1306 can be connected to either a
	 * low voltage power source via VBAT or (3.3V to 4.2V)
	 * the usual VCC voltage supply (7V to 15V)
	 * for driving the display.
	 *
	 * TODO: Possibly move this to a device specific switch statement
	 */
	ssd130x->vbat = devm_regulator_get_optional(dev, "vbat");
	if (IS_ERR(ssd130x->vbat)) {
		ret = PTR_ERR(ssd130x->vbat);

		if (ret != -ENODEV) {
			if (ret != -EPROBE_DEFER)
				dev_err(dev,
					"failed to request regulator: %d\n",
					ret);
			return ret;
		}

		ssd130x->vbat = NULL;
	}
	else {
		ssd130x->display_settings.use_charge_pump = true;
	}

	/* Get panel driving supply */
	if (!ssd130x->vbat) {
		ssd130x->vcc = devm_regulator_get(dev, "vcc");
		if (IS_ERR(ssd130x->vcc)){
			ret = PTR_ERR(ssd130x->vcc);
			return ret;
		}
		else {
			ssd130x->display_settings.use_charge_pump = false;
		}
	}


	/* TODO: Remove this if the configuration of these values is refactored
	to a switch statement with modes instead of the device tree */
	if (of_property_read_u8(node, "solomon,width",
				 &ssd130x->display_settings.width))
		ssd130x->display_settings.width = device_info->default_width;

	if (of_property_read_u8(node, "solomon,height",
				 &ssd130x->display_settings.height))
		ssd130x->display_settings.height = device_info->default_height;

	if (of_property_read_u8(node, "solomon,height-mm,",
				&ssd130x->display_settings.height_mm))
		ssd130x->display_settings.height_mm = 0;

	if (of_property_read_u8(node, "solomon,width-mm,",
				&ssd130x->display_settings.width_mm))
		ssd130x->display_settings.width_mm = 0;

	/* TODO: Page and COM offset
	if (of_property_read_u8(node, "solomon,page-offset",
				&ssd130x_dev->display_settings->page_offset))
		ssd130x->display_settings->page_offset = 0;

	if (of_property_read_u8(node, "solomon,com-offset",
				&ssd130x_dev->display_settings->com_offset))
		ssd130x->display_settings->com_offset = 0;
	*/

	if (of_property_read_u8(node, "solomon,pre-charge-period-1",
				&ssd130x->display_settings
				.pre_charge_period_dclocks_phase1))
		ssd130x->display_settings
		.pre_charge_period_dclocks_phase1 = 2;

	if (of_property_read_u8(node, "solomon,pre-charge-period-2",
				&ssd130x->display_settings
				.pre_charge_period_dclocks_phase2))
		ssd130x->display_settings
		.pre_charge_period_dclocks_phase2 = 2;


	ssd130x->display_settings.seg_remap =
			of_property_read_bool(node,"solomon,segment-remap");
	ssd130x->display_settings.com_seq_pin_cfg =
			of_property_read_bool(node, "solomon,com-seq-pin-cfg");
	ssd130x->display_settings.com_lr_remap =
			of_property_read_bool(node, "solomon,com-lr-remap");
	ssd130x->display_settings.com_scan_dir_inv =
			of_property_read_bool(node, "solomon,com-scan-dir-inv");
	ssd130x->display_settings.inverse_display =
			of_property_read_bool(node, "solomon,inverse-colors");


	ssd130x->display_settings.contrast = HALF_CONTRAST;


	if (of_property_read_u8(node, "solomon,clock-divide-ratio",
				&ssd130x->display_settings.clock_divide_ratio))
		ssd130x->display_settings.clock_divide_ratio =
				device_info->default_clock_divide_ratio;

	if (of_property_read_u8(node, "solomon,oscillator-frequency",
				&ssd130x->display_settings
					.oscillator_frequency))
		ssd130x->display_settings.oscillator_frequency =
				device_info->default_oscillator_frequency;

	return 0;
}

/**
 * ssd130x_sw_init - Initialize the configuration of a ssd130x_panel device
 * @ssd130x The ssd130x_panel to initialize.
 *
 * Software Configuration & Initialization Sequence
 * Based on the SSD1306 manual from Solomon
 *
 * Tilde [~] marked entries do not appear in the software initialization
 * sequence described in the manual and have been placed where they seem
 * most fit.
 *
 * Set MUX ratio (A8h, 3Fh)
 * Set display offset (D3h, 00h)
 * Set display start line (40h)
 * Set segment re-map (A0h/A1h)
 * Set COM output scan direction (C0h/C8h)
 * Set COM pins hardware configuration (DAh, 02)
 *
 * Set contrast (81h, F7h)
 * Set pre-charge period (D9h, <device specific default >) [~]
 * Set VCOMH deselect level (DBh, <device specific default>) [~]
 * Entire display On (A4h)
 * Set normal/inverse display (A6h)
 * Set display clock divide ratio / oscillator frequency (D5h, 80h)
 * 
 * Part of the enable sequence:
 * Enable charge pump regulator (8Dh, 14h)
 * Display on (AFh)
 */
static int ssd130x_sw_init(struct ssd130x_panel *ssd130x)
{
	uint8_t com_pins_cfg;
	uint8_t pre_charge_period_phase_1_2;
	uint8_t display_clock;
	int ret;

	/* Set MUX ratio */
	ret = ssd130x_command_1_param(ssd130x,
				      SSD130X_SET_MULTIPLEX_RATIO,
				      (ssd130x->display_settings.height - 1));
	if (ret)
		return ret;

	/* Set display offset */
	ret = ssd130x_command_1_param(ssd130x,
				      SSD130X_SET_DISPLAY_OFFSET,
				      ssd130x->display_settings.com_offset);
	if (ret)
		return ret;

	/* Set display start line */
	ret = ssd130x_command_single(ssd130x,
				     SSD130X_SET_DISPLAY_START_LINE_ZERO |
				     (ssd130x->display_settings
				     .display_start_line & 0x7f));
	if (ret)
		return ret;

	/* Set segment re-map */
	if (ssd130x->display_settings.seg_remap)
		ret = ssd130x_command_single(ssd130x, SSD130X_SEG_REMAP_ON);
	else
		ret = ssd130x_command_single(ssd130x, SSD130X_SEG_REMAP_OFF);
	if (ret)
		return ret;

	/* Set COM output scan direction */
	if (ssd130x->display_settings.com_scan_dir_inv)
		ret =
		ssd130x_command_single(ssd130x,
				       SSD130X_SET_SCAN_DIRECTION_INVERTED);
	else
		ret =
		ssd130x_command_single(ssd130x,
				       SSD130X_SET_SCAN_DIRECTION_NORMAL);
	if (ret)
		return ret;

	/* Set COM pins hardware configuration
	 *
	 * Default configuration, command unchanged (0xDA):
	 * Alternative COM pin configuration (Bit[4] = 1b)
	 * Disable COM Left/Right remap (Bit[5] = 0b)
	 */
	com_pins_cfg = 0x02 /* Base byte for COM pin data */
		| !ssd130x->display_settings.com_seq_pin_cfg << 4
		| ssd130x->display_settings.com_lr_remap << 5;
	ret = ssd130x_command_1_param(ssd130x,
				      SSD130X_SET_COM_PINS_CONFIG,
				      com_pins_cfg);
	if (ret)
		return ret;

	/* TODO: Remove this, when it has been proven that an earlier call by
	 * the backlight does not affect the correct initialization of the
	 * device.
	 */
	/* Set contrast */
	ret = ssd130x_command_1_param(ssd130x,
				      SSD130X_SET_CONTRAST_CONTROL,
				      ssd130x->display_settings.contrast);
	if (ret)
		return ret;
	
	/* Set pre-charge period */
	pre_charge_period_phase_1_2 =
				ssd130x->display_settings
					.pre_charge_period_dclocks_phase1
				| (ssd130x->display_settings
					.pre_charge_period_dclocks_phase2 << 4);

	ret = ssd130x_command_1_param(ssd130x,
				      SSD130X_SET_PRECHARGE_PERIOD,
				      pre_charge_period_phase_1_2);
	if (ret)
		return ret;

	/* Set VCOMH deselect level */
	ret = ssd130x_command_1_param(ssd130x,
				      SSD130X_SET_VCOMH_DESELECT_LEVEL,
				      ssd130x->display_settings
				      .vcomh_deselect_level);
	if (ret)
		return ret;

	/* Entire display on */
	ret = ssd130x_command_single(ssd130x, SSD130X_ENTIRE_DISPLAY_ON);
	if (ret)
		return ret;

	/* Set normal/inverse display */
	if (ssd130x->display_settings.inverse_display)
		ret = ssd130x_command_single(ssd130x,
					     SSD130X_SET_DISPLAY_MODE_INVERSE);
	else
		ssd130x_command_single(ssd130x,
				       SSD130X_SET_DISPLAY_MODE_NORMAL);
	if (ret)
		return ret;

	/* Set display clock divide ratio/oscillator frequency
	 *
	 * Data byte contains the display clock's
	 * divide ratio         (A[3:0]) and
	 * oscillator frequency (A[7:4]).
	 */
	display_clock = (
		((ssd130x->display_settings.clock_divide_ratio - 1) & 0x0f) |
		((ssd130x->display_settings.oscillator_frequency & 0x0f) << 4));
	ret = ssd130x_command_1_param(ssd130x,
				      SSD130X_SET_DISPLAY_CLOCK,
				      display_clock);
	if (ret)
		return ret;

	return 0;
}
