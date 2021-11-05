/* SPDX-License-Identifier: GPL-2.0 */
/*
 * DRM panel driver for Solomon SSD130x controller family
 * 
 * Copyright 2012 Free Electrons
 * Copyright 2021-2022 DH electronics
 * 
 * Partially based on the the ssd1307fb linux fbdev driver by Free Electrons.
 */

#include <drm/drm_panel.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

#include <drm/drm_modes.h>


#define SSD130X_DATA				0x40
#define SSD130X_COMMAND				0x80

/* Command set */
#define SSD130X_SET_ADDRESS_MODE		0x20
#define SSD130X_ADDRESS_MODE_HORIZONTAL		0x00
#define SSD130X_ADDRESS_MODE_VERTICAL		0x01
#define SSD130X_ADDRESS_MODE_PAGE		0x02
#define SSD130X_SET_COL_RANGE			0x21
#define SSD130X_SET_PAGE_RANGE			0x22
#define SSD130X_SET_DISPLAY_START_LINE_ZERO	0x40
#define SSD130X_SET_CONTRAST_CONTROL		0x81
#define	SSD130X_CHARGE_PUMP			0x8d
#define SSD130X_CHARGE_PUMP_SETTING_OFF		0x10
#define SSD130X_CHARGE_PUMP_SETTING_ON		0x14
#define SSD130X_SET_LOOKUP_TABLE		0x91
#define SSD130X_SEG_REMAP_OFF			0xa0
#define SSD130X_SEG_REMAP_ON			0xa1
#define SSD130X_ENTIRE_DISPLAY_ON		0xa4
#define SSD130X_ENTIRE_DISPLAY_ON_IGNORE_RAM	0xa5
#define SSD130X_SET_DISPLAY_MODE_NORMAL		0xa6
#define SSD130X_SET_DISPLAY_MODE_INVERSE	0xa7
#define SSD130X_SET_MULTIPLEX_RATIO		0xa8
#define SSD130X_DISPLAY_OFF			0xae
#define SSD130X_DISPLAY_ON			0xaf
#define SSD130X_START_PAGE_ADDRESS		0xb0
#define SSD130X_SET_SCAN_DIRECTION_NORMAL	0xc0
#define SSD130X_SET_SCAN_DIRECTION_INVERTED	0xc8
#define SSD130X_SET_DISPLAY_OFFSET		0xd3
#define	SSD130X_SET_DISPLAY_CLOCK		0xd5
#define	SSD130X_SET_AREA_COLOR_MODE		0xd8
#define	SSD130X_SET_PRECHARGE_PERIOD		0xd9
#define	SSD130X_SET_COM_PINS_CONFIG		0xda
#define	SSD130X_SET_VCOMH_DESELECT_LEVEL	0xdb
#define SSD130X_NOP				0xe3

#define HALF_CONTRAST 127
#define MAX_CONTRAST 255


struct ssd130x_panel {
	int (*command) (struct ssd130x_panel *ssd130x,
			uint8_t cmd,
			uint8_t *params,
			size_t param_count
	);
	int (*data) (struct ssd130x_panel *ssd130x,
		     uint8_t *data,
		     size_t data_len
	);
	struct drm_panel panel;
	struct drm_display_mode mode;
	struct device *dev;
	union {
		struct spi_device *spi;
		struct i2c_client *i2c;
	};
	union {
		struct gpio_desc *dc;	/* Data command (4-wire SPI) */
		struct gpio_desc *sa0;	/* Slave address bit (I2C) */
	};
	struct gpio_desc *reset;	/* Reset */
	struct regulator *vdd;		/* Core logic supply */
	union {
		struct regulator *vcc;	/* Panel driving supply */
		struct regulator *vbat;	/* Charge pump regulator supply */
	};
	struct backlight_device *backlight;
		struct {
		unsigned int com_scan_dir_inv : 1;
		unsigned int com_seq_pin_cfg : 1;
		unsigned int com_lr_remap : 1;
		unsigned int seg_remap : 1;
		unsigned int inverse_display : 1;
		unsigned int use_charge_pump : 1;
		uint8_t height;
		uint8_t width;
		uint8_t height_mm;
		uint8_t width_mm;
		uint8_t display_start_line;
		uint8_t com_offset ;
		uint8_t contrast;
		uint8_t pre_charge_period_dclocks_phase1;
		uint8_t pre_charge_period_dclocks_phase2;
		uint8_t vcomh_deselect_level;
		uint8_t clock_divide_ratio;
		uint8_t oscillator_frequency;
	} display_settings;
	bool prepared;
	bool enabled;
};

static inline struct ssd130x_panel *drm_panel_to_ssd130x_panel(
					struct drm_panel *panel)
{
	return container_of(panel, struct ssd130x_panel, panel);
}

struct ssd130x_panel_info {
	unsigned int default_height;
	unsigned int default_width;
	uint8_t default_vcomh_deselect_level;
	uint8_t default_clock_divide_ratio;
	uint8_t default_oscillator_frequency;
	bool has_chargepump;
	bool need_pwm;
};

static struct ssd130x_panel_info ssd1306_panel_info = {
	.default_height = 64,
	.default_width = 128,
	.default_vcomh_deselect_level = 0x20,
	.default_clock_divide_ratio = 1,
	.default_oscillator_frequency = 8,
	.has_chargepump = true,
	.need_pwm = false,
};

int ssd130x_bus_independent_probe(struct ssd130x_panel *ssd130x,
				  struct device *dev,
				  struct device_node *node);

static const struct of_device_id ssd130x_of_match[] = {
	{
		.compatible = "solomon,ssd1306",
		.data = (void *)&ssd1306_panel_info,
	},
	{ }
};

/**
 * ssd130x_command() - Base function for sending commands to the display
 * @ssd130x: The ssd130x_panel to which commands should be sent.
 * @command_buf: Buffer from which the commands should be sent.
 * @bytes: Size of the command buffer.
 * 
 * This function serves as base for the multi-parameter functions.
 * It's implementation is defined by the protocol specific driver.
 */
int ssd130x_command(struct ssd130x_panel *ssd130x,
		    uint8_t *cmd_buf,
		    size_t bytes);

/**
 * ssd130x_command_single() - Send a zero parameter command to the display
 * @ssd130x: The ssd130x_panel to which the command should be sent.
 * @cmd: The zero parameter command
 * 
 * Returns -ENOMEM if the command buffer could not be allocated or another
 * error if the error occurred while trying to send the command.
 */
int ssd130x_command_single(struct ssd130x_panel *ssd130x,
			   uint8_t cmd);

/**
 * ssd130x_command_1_param() - Send a one parameter command to the display
 * @ssd130x: The ssd130x_panel to which the command should be sent.
 * @cmd: The base command
 * @param: The command parameter
 * 
 * Returns -ENOMEM if the command buffer could not be allocated or another
 * error if the error occurred while trying to send the command.
 */
int ssd130x_command_1_param(struct ssd130x_panel *ssd130x,
			    uint8_t cmd,
			    uint8_t param);

/**
 * ssd130x_command_2_params() - Send a two parameter command to the display
 * @ssd130x: ssd130x_panel to which the command should be sent.
 * @cmd: The base command
 * @param1: First command parameter
 * @param2: Second command parameter
 * 
 * Returns -ENOMEM if the command buffer could not be allocated or another
 * error if the error occurred while trying to send the command.
 */
int ssd130x_command_2_params(struct ssd130x_panel *ssd130x,
			     uint8_t cmd,
			     uint8_t param1,
			     uint8_t param2);