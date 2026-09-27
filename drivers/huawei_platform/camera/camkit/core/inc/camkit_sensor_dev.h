/*
 * camkit_sensor_dev.h
 *
 * Copyright (c) 2021-2021 Honor Technologies Co., Ltd.
 *
 * camera device driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef CAMKIT_SENSOR_DEV_H
#define CAMKIT_SENSOR_DEV_H

#include <linux/atomic.h>

#include "kd_camkit_define.h"

#define MAX_PIN_NAME_LEN 32

enum pin_id {
	CAMKIT_PIN_NONE = 0,
	CAMKIT_PIN_GPIO,
	CAMKIT_PIN_MCLK,
	CAMKIT_PIN_LDO,
	CAMKIT_PIN_PMIC,
	CAMKIT_PIN_MAX,
};

enum mclk_state {
	MCLK_STATE_DISABLE = 0,
	MCLK_STATE_ENABLE_2MA,
	MCLK_STATE_ENABLE_4MA,
	MCLK_STATE_ENABLE_6MA,
	MCLK_STATE_ENABLE_8MA,
	MCLK_STATE_MAX,
};

struct regulator_info {
	char name[MAX_PIN_NAME_LEN];
	struct regulator *regulator;
};

struct gpio_info {
	char name[MAX_PIN_NAME_LEN];
	enum camkit_hw_pin_type pin_type;
 	struct pinctrl_state* gpio_active;
 	struct pinctrl_state* gpio_suspend;
};

struct mclk_info {
	char name[MAX_PIN_NAME_LEN];
	struct pinctrl_state* state;
};

struct camera_dev_ctrl {
	struct device *dev;
	uint32 sensor_idx;
	uint32 mclk_idx;
	int32  power_status;
	struct pinctrl* pinctrl;
	struct gpio_info gpio_table[CAMKIT_HW_PIN_MAX_NUM];
	struct mclk_info mclk_table[MCLK_STATE_MAX];
	struct regulator_info ldo_table[CAMKIT_HW_PIN_MAX_NUM];
};

struct sensor_dev_ctrl {
	struct platform_device *pdev;
	struct device *dev;
	struct device_node *of_node;
};

int32 camkit_sensor_power_on(uint32 sensor_idx,
	struct camkit_hw_power_info_t *power_on_info);
int32 camkit_sensor_power_down(uint32 sensor_idx,
	struct camkit_hw_power_info_t *power_down_info);
int32 camkit_sensor_get_dt_version(uint32 *ver);

#endif // CAMKIT_SENSOR_DEV_H
