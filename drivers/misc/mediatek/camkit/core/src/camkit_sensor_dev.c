/*
 * camkit_sensor_dev.c
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

#include "camkit_sensor_dev.h"

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#include <securec.h>

#include "camkit_driver_types.h"
#include "camkit_driver_interface.h"

struct camera_dev_ctrl g_dev_ctrl[CAMKIT_SENSOR_IDX_MAX_NUM];

struct camera_dev_ctrl *get_camera_dev_ctrl(uint32 index)
{
	if (index < CAMKIT_SENSOR_IDX_MIN_NUM ||
		index >= CAMKIT_SENSOR_IDX_MAX_NUM)
		return NULL;
	else
		return &g_dev_ctrl[index];
}

struct pin_info {
	uint32 id;
	char *name;
	enum camkit_hw_pin_type type;
};

static const struct pin_info pin_table[] = {
	{ CAMKIT_PIN_GPIO, "pwdn",        CAMKIT_HW_PIN_PDN },
	{ CAMKIT_PIN_GPIO, "rst",         CAMKIT_HW_PIN_RST },
	{ CAMKIT_PIN_GPIO, "rst1",        CAMKIT_HW_PIN_RST1 },
	{ CAMKIT_PIN_GPIO, "avdd_en",     CAMKIT_HW_PIN_AVDD_EN },
	{ CAMKIT_PIN_GPIO, "avdd_sel",    CAMKIT_HW_PIN_AVDD_SEL },
	{ CAMKIT_PIN_GPIO, "dvdd_en",     CAMKIT_HW_PIN_DVDD_EN },
	{ CAMKIT_PIN_GPIO, "dvdd_sel",    CAMKIT_HW_PIN_DVDD_SEL },
	{ CAMKIT_PIN_GPIO, "iovdd_en",    CAMKIT_HW_PIN_IOVDD_EN },
	{ CAMKIT_PIN_GPIO, "avdd1_en",    CAMKIT_HW_PIN_AVDD1_EN },
	{ CAMKIT_PIN_GPIO, "afvdd_en",    CAMKIT_HW_PIN_AFVDD_EN },
	{ CAMKIT_PIN_GPIO, "5v_boost",    CAMKIT_HW_PIN_5V_BOOST },
	{ CAMKIT_PIN_GPIO, "mipi_sw_en",  CAMKIT_HW_PIN_MIPI_SWITCH_EN },
	{ CAMKIT_PIN_GPIO, "mipi_sw_sel", CAMKIT_HW_PIN_MIPI_SWITCH_SEL },

	{ CAMKIT_PIN_MCLK, "mclk",        CAMKIT_HW_PIN_MCLK },

	{ CAMKIT_PIN_LDO,  "cam_vcama",   CAMKIT_HW_PIN_AVDD },
	{ CAMKIT_PIN_LDO,  "cam_vcama1",  CAMKIT_HW_PIN_AVDD1 },
	{ CAMKIT_PIN_LDO,  "cam_vcamd",   CAMKIT_HW_PIN_DVDD },
	{ CAMKIT_PIN_LDO,  "cam_vcamio",  CAMKIT_HW_PIN_DOVDD },
	{ CAMKIT_PIN_LDO,  "cam_vmch",    CAMKIT_HW_PIN_VMCH },
	{ CAMKIT_PIN_LDO,  "cam_vcamaf",  CAMKIT_HW_PIN_AFVDD },

	{ CAMKIT_PIN_PMIC, "pmic_ldo1",   CAMKIT_HW_PIN_LDO1_PMIC },
	{ CAMKIT_PIN_PMIC, "pmic_ldo2",   CAMKIT_HW_PIN_LDO2_PMIC },
	{ CAMKIT_PIN_PMIC, "pmic_ldo3",   CAMKIT_HW_PIN_LDO3_PMIC },
	{ CAMKIT_PIN_PMIC, "pmic_ldo4",   CAMKIT_HW_PIN_LDO4_PMIC },
	{ CAMKIT_PIN_PMIC, "pmic_xbuck1", CAMKIT_HW_PIN_XBUCK1_PMIC },
};

static char* find_pin_name(enum camkit_hw_pin_type type, int32 id)
{
	uint32 i = 0;
	uint32 size = camkit_array_size(pin_table);

	for (i = 0; i < size; i++) {
		if (pin_table[i].type == type && pin_table[i].id == id)
			return pin_table[i].name;

		if (pin_table[i].type == type && pin_table[i].id != id)
			return NULL;
	}

	log_info("pin: [%d,%d] not found, please check gpio_pin_table", id, type);

	return NULL;
}

#if 0
static int get_pmic_channel(int32 pin)
{
	int pmic_channel;

	switch (pin) {
	case CAMKIT_HW_PIN_LDO1_PMIC:
		pmic_channel = VOUT_LDO_1;
		break;
	case CAMKIT_HW_PIN_LDO2_PMIC:
		pmic_channel = VOUT_LDO_2;
		break;
	case CAMKIT_HW_PIN_LDO3_PMIC:
		pmic_channel = VOUT_LDO_3;
		break;
	case CAMKIT_HW_PIN_LDO4_PMIC:
		pmic_channel = VOUT_LDO_4;
		break;
	case CAMKIT_HW_PIN_XBUCK1_PMIC:
		pmic_channel = VOUT_BUCK_1;
		break;
	default:
		pmic_channel = VOUT_MAX;
		break;
	};

	return pmic_channel;
}

static int32 camkit_sensor_handle_pmic(struct camera_dev_ctrl *dev_ctrl,
	int32 pin_type, int32 pin_val, uint32 pin_delay)
{
	struct hw_comm_pmic_cfg_t pmic_config;
	errno_t ret;
	struct hw_pmic_ctrl_t *hw_pmic_ctrl = NULL;

	hw_pmic_ctrl = hw_get_pmic_ctrl();

	pr_info("%s sensor_idx:%d, pin:%d, pin_val:%u\n",
		__func__, sensor_idx, pin, pin_val);

	if (!hw_pmic_ctrl || !hw_pmic_ctrl->func_tbl ||
		!hw_pmic_ctrl->func_tbl->pmic_power_cfg) {
		pr_err("input args has NULL.\n");
		return ERR_INVAL;
	}

	ret = memset_s(&pmic_config, sizeof(pmic_config), 0, sizeof(pmic_config));
	if (ret != EOK)
		pr_err("pmic_info memset_s fail, ret = %d\n", ret);

	pmic_config.pmic_num = MAIN_PMIC;
	pmic_config.pmic_power_type = get_pmic_channel(pin);
	pmic_config.pmic_power_voltage = get_pmic_val(pin_val);

	if (pmic_config.pmic_power_voltage > HWPMIC_VOLTAGE_MIN)
		pmic_config.pmic_power_state = PMIC_POWER_ON;
	else
		pmic_config.pmic_power_state = PMIC_POWER_OFF;

	pr_info("%s sensor_idx:%d, channel:%d, voltage:%u, status:%d\n",
		__func__, sensor_idx, pmic_config.pmic_power_type,
		pmic_config.pmic_power_voltage, pmic_config.pmic_power_state);

	hw_pmic_ctrl->func_tbl->pmic_power_cfg(CAM_PMIC_REQ, &pmic_config);

	return ERR_NONE;
}
#endif

static int32 camkit_sensor_init_pinctrl(
	struct camera_dev_ctrl *dev_ctrl)
{
	int32 i  = 0;
	int32 ret = ERR_NONE;
	struct gpio_info *gpio = NULL;
	char pin_name[MAX_PIN_NAME_LEN] = {0};
	struct mclk_info *mclk = NULL;

	dev_ctrl->pinctrl = devm_pinctrl_get(dev_ctrl->dev);

	if (IS_ERR_OR_NULL(dev_ctrl->pinctrl)) {
		dev_ctrl->pinctrl = NULL;
		log_err("Cannot find pinctrl");
		return ERR_INVAL;
	}

	// get the states of gpio pinctrls
	for (i = 0; i < CAMKIT_HW_PIN_MAX_NUM; i++) {
		gpio = &dev_ctrl->gpio_table[i];
		if (gpio->pin_type == 0)
			continue;

		ret = snprintf_s(pin_name, sizeof(pin_name), sizeof(pin_name) - 1, "%s_1", gpio->name);
		if (ret < 0) {
			log_err("snprintf_s failed\n");
			return ERR_INVAL;
		}
 		gpio->gpio_active = pinctrl_lookup_state(dev_ctrl->pinctrl, pin_name);
		if (IS_ERR_OR_NULL(gpio->gpio_active)) {
			log_info("get gpio: %s active state fail", pin_name);
			return ERR_INVAL;
		}

		ret = snprintf_s(pin_name, sizeof(pin_name), sizeof(pin_name) - 1, "%s_0", gpio->name);
		if (ret < 0) {
			log_err("snprintf_s failed\n");
			return ERR_INVAL;
		}
 		gpio->gpio_suspend = pinctrl_lookup_state(dev_ctrl->pinctrl, pin_name);
		if (IS_ERR_OR_NULL(gpio->gpio_suspend)) {
			log_info("get gpio: %s suspend state fail", pin_name);
			return ERR_INVAL;
		}

		log_info("get camera[%d] gpio: %s pinctrl state ok",
			dev_ctrl->sensor_idx, gpio->name);
	}

	// get the states of mclk pinctrls
	for (i = MCLK_STATE_DISABLE; i < MCLK_STATE_MAX; i++) {
		mclk = &(dev_ctrl->mclk_table[i]);
		mclk->state = pinctrl_lookup_state(dev_ctrl->pinctrl, mclk->name);
		if (IS_ERR_OR_NULL(mclk->state)) {
			log_err("get %s pinctrl state fail", mclk->name);
			return ERR_INVAL;
		}

		log_info("get camera[%d] %s pinctrl state ok",
			dev_ctrl->sensor_idx, mclk->name);
	}

	hwsensor_get_mclk_drv_current(dev_ctrl->sensor_idx, &dev_ctrl->mclk_idx);
	log_info("set camera[%d] mclk index to: %d",
		dev_ctrl->sensor_idx, dev_ctrl->mclk_idx);

	return ERR_NONE;
}

static int32 camkit_sensor_release_pinctrl(
	struct camera_dev_ctrl *dev_ctrl)
{
	if (dev_ctrl->pinctrl) {
		devm_pinctrl_put(dev_ctrl->pinctrl);
		dev_ctrl->pinctrl = NULL;
		log_info("release camera[%d] pinctrl ok", dev_ctrl->sensor_idx);
	}

	return ERR_NONE;
}

static int32 camkit_sensor_handle_gpio(struct camera_dev_ctrl *dev_ctrl,
	int32 pin_type, int32 pin_val, uint32 pin_delay)
{
	int32 rc = ERR_NONE;
	struct gpio_info *gpio = NULL;

	gpio = &(dev_ctrl->gpio_table[pin_type]);
	if (pin_val < 0 || gpio->pin_type != pin_type ||
		IS_ERR_OR_NULL(gpio->gpio_active) ||
		IS_ERR_OR_NULL(gpio->gpio_suspend) ||
		IS_ERR_OR_NULL(dev_ctrl->pinctrl)) {
		log_err("set gpio [%d : %d] fail", pin_type, pin_val);
		return ERR_NONE;
	}

	if (pin_val > 0)
		rc = pinctrl_select_state(dev_ctrl->pinctrl, gpio->gpio_active);
	else
		rc = pinctrl_select_state(dev_ctrl->pinctrl, gpio->gpio_suspend);

	if (rc)
		log_err("select gpio state fail");

	mdelay(pin_delay);

	log_info("set camera[%d] gpio:%s %d OK", dev_ctrl->sensor_idx,
		gpio->name, pin_val);

	return rc;
}

static int32 camkit_sensor_handle_mclk(struct camera_dev_ctrl *dev_ctrl,
	int32 pin_type, int32 pin_val, uint32 pin_delay)
{
	int32 rc = ERR_NONE;
	struct mclk_info *mclk = NULL;
	struct pinctrl_state* mclk_active = NULL;
	struct pinctrl_state* mclk_suspend = NULL;

	if (pin_val < 0 || pin_type != CAMKIT_HW_PIN_MCLK ||
		IS_ERR_OR_NULL(dev_ctrl->pinctrl)) {
		log_err("invalid parameter");
		return ERR_INVAL;
	}

	mclk = &(dev_ctrl->mclk_table[MCLK_STATE_DISABLE]);
	mclk_suspend = mclk->state;
	if (IS_ERR_OR_NULL(mclk_suspend)) {
		log_err("get mclk pinctrl suspend state fail");
		return ERR_INVAL;
	}

	if (dev_ctrl->mclk_idx < MCLK_STATE_ENABLE_2MA ||
		dev_ctrl->mclk_idx >= MCLK_STATE_MAX)
		mclk_active = dev_ctrl->mclk_table[MCLK_STATE_ENABLE_4MA].state;
	else
		mclk_active = dev_ctrl->mclk_table[dev_ctrl->mclk_idx].state;
	if (IS_ERR_OR_NULL(mclk_active)) {
		log_err("get mclk pinctrl active state fail");
		return ERR_INVAL;
	}

	if (pin_val > 0)
		rc = pinctrl_select_state(dev_ctrl->pinctrl, mclk_active);
	else
		rc = pinctrl_select_state(dev_ctrl->pinctrl, mclk_suspend);

	if (rc)
		log_err("select mclk state fail");

	mdelay(pin_delay);

	log_err("set camera[%d] mclk: %d OK, val:%d", dev_ctrl->sensor_idx,
		dev_ctrl->mclk_idx, pin_val);

	return rc;
}

static int32 camkit_sensor_enable_regulator(struct camera_dev_ctrl *dev_ctrl,
	struct regulator_info *ldo, uint32 min_volt, uint32 max_volt)
{
	int32_t rc = 0;

	ldo->regulator = regulator_get_optional(dev_ctrl->dev, ldo->name);
	if (IS_ERR_OR_NULL(ldo->regulator)) {
		log_err("get ldo: %s fail", ldo->name);
		ldo->regulator = NULL;
		return ERR_INVAL;
	}

	rc = regulator_set_voltage(ldo->regulator, min_volt, max_volt);
	if (rc)
		log_err("%s set voltage [%d,%d] failed", ldo->name, min_volt, max_volt);

	rc = regulator_enable(ldo->regulator);
	if (rc) {
		log_err("%s regulator_enable failed", ldo->name);
		regulator_put(ldo->regulator);
		ldo->regulator = NULL;
		return rc;
	}

	log_info("camera[%d] set %s voltage [%d,%d] OK", dev_ctrl->sensor_idx,
		ldo->name, min_volt, max_volt);

	return rc;
}

static int32 camkit_sensor_disable_regulator(struct regulator_info *ldo)
{
	int32_t rc = 0;

	if (ldo->regulator) {
		rc = regulator_disable(ldo->regulator);
		if (rc)
			log_err("%s regulator_disable failed", ldo->name);

		regulator_put(ldo->regulator);
		ldo->regulator = NULL;

		log_info("%s disable OK", ldo->name);
	}

	return rc;
}

static int32 camkit_sensor_handle_regulator(
	struct camera_dev_ctrl *dev_ctrl,
	int32 pin_type, int32 pin_val, uint32 pin_delay)
{
	int32 rc = ERR_NONE;
	struct regulator_info *regulator = NULL;

	if (pin_val < 0) {
		log_err("invalid parameter");
		return ERR_INVAL;
	}

	regulator = &(dev_ctrl->ldo_table[pin_type]);

	if (pin_val > 0)
		camkit_sensor_enable_regulator(dev_ctrl, regulator, pin_val, pin_val);
	else
		camkit_sensor_disable_regulator(regulator);

	mdelay(pin_delay);

	return rc;
}

static int32 camkit_sensor_handle_power_sequence(
	struct camera_dev_ctrl *dev_ctrl,
	struct camkit_hw_power_info_t *power_info)
{
	struct camkit_hw_power_info_t *pwr_info = power_info;
	int32 i;

	for (i = 0; i < CAMKIT_POWER_INFO_MAX; i++) {
		if (pwr_info[i].pin_type == CAMKIT_HW_PIN_NONE)
			break;

		switch (pwr_info[i].pin_type) {
		case CAMKIT_HW_PIN_PDN:
		case CAMKIT_HW_PIN_RST:
		case CAMKIT_HW_PIN_AVDD_EN:
		case CAMKIT_HW_PIN_AVDD_SEL:
		case CAMKIT_HW_PIN_DVDD_EN:
		case CAMKIT_HW_PIN_DVDD_SEL:
		case CAMKIT_HW_PIN_IOVDD_EN:
		case CAMKIT_HW_PIN_AVDD1_EN:
		case CAMKIT_HW_PIN_AFVDD_EN:
		case CAMKIT_HW_PIN_RST1:
		case CAMKIT_HW_PIN_MIPI_SWITCH_EN:
		case CAMKIT_HW_PIN_MIPI_SWITCH_SEL:
			camkit_sensor_handle_gpio(dev_ctrl, pwr_info[i].pin_type,
				pwr_info[i].pin_val, pwr_info[i].pin_delay);
			break;

		case CAMKIT_HW_PIN_AVDD:
		case CAMKIT_HW_PIN_AVDD1:
		case CAMKIT_HW_PIN_DVDD:
		case CAMKIT_HW_PIN_DOVDD:
		case CAMKIT_HW_PIN_VMCH:
		case CAMKIT_HW_PIN_AFVDD:
			camkit_sensor_handle_regulator(dev_ctrl, pwr_info[i].pin_type,
				pwr_info[i].pin_val, pwr_info[i].pin_delay);
			break;

		case CAMKIT_HW_PIN_MCLK:
			camkit_sensor_handle_mclk(dev_ctrl, pwr_info[i].pin_type,
				pwr_info[i].pin_val, pwr_info[i].pin_delay);
			break;

#if 0
		case CAMKIT_HW_PIN_LDO1_PMIC:
		case CAMKIT_HW_PIN_LDO2_PMIC:
		case CAMKIT_HW_PIN_LDO3_PMIC:
		case CAMKIT_HW_PIN_LDO4_PMIC:
		case CAMKIT_HW_PIN_XBUCK1_PMIC:
			camkit_sensor_handle_pmic(dev_ctrl, pwr_info[i].pin_type,
				pwr_info[i].pin_val, pwr_info[i].pin_delay);
			break;
#endif

		default:
			log_err("unsupported pin type:%d", pwr_info[i].pin_type);
			break;
		}
	}

	return ERR_NONE;
}

int32 camkit_sensor_power_on(uint32 sensor_idx,
	struct camkit_hw_power_info_t *power_on_info)
{
	int32 rc = ERR_NONE;
	struct camera_dev_ctrl *dev_ctrl = NULL;

	log_info("to power on camera %d", sensor_idx);
	return_err_if_null(power_on_info);

	dev_ctrl = get_camera_dev_ctrl(sensor_idx);
	if (!dev_ctrl) {
		log_err("invalid parameter");
		return ERR_INVAL;
	}

	if (dev_ctrl->power_status == CAMKIT_HW_POWER_STATUS_ON) {
		log_err("camera[%d] already power on", sensor_idx);
		return ERR_INVAL;
	}

	rc = camkit_sensor_init_pinctrl(dev_ctrl);
	if (rc) {
		log_err("initial device hardware source fail");
		return rc;
	}

	rc = camkit_sensor_handle_power_sequence(dev_ctrl, power_on_info);
	if (rc) {
		log_err("power on fail");
		return rc;
	}

	dev_ctrl->power_status = CAMKIT_HW_POWER_STATUS_ON;

	return ERR_NONE;
}

int32 camkit_sensor_power_down(uint32 sensor_idx,
	struct camkit_hw_power_info_t *power_down_info)
{
	int32 rc = ERR_NONE;
	struct camera_dev_ctrl *dev_ctrl = NULL;

	log_info("power down camera %d", sensor_idx);
	return_err_if_null(power_down_info);

	dev_ctrl = get_camera_dev_ctrl(sensor_idx);
	if (!dev_ctrl) {
		log_err("invalid parameter");
		return ERR_INVAL;
	}

	if (dev_ctrl->power_status == CAMKIT_HW_POWER_STATUS_OFF) {
		log_err("camera[%d] already power off", sensor_idx);
		return ERR_INVAL;
	}

	rc = camkit_sensor_handle_power_sequence(dev_ctrl, power_down_info);
	if (rc)
		log_err("power down fail");

	camkit_sensor_release_pinctrl(dev_ctrl);

	dev_ctrl->power_status = CAMKIT_HW_POWER_STATUS_OFF;

	return ERR_NONE;
}

int32 camkit_sensor_get_dt_version(uint32 *ver)
{
	int32 rc = ERR_NONE;
	struct device_node *of_node = NULL;
	static uint32 dt_ver;
	static uint32 init;

	return_err_if_null(ver);

	if (!init) {
		of_node = of_find_compatible_node(NULL, NULL, "mediatek,camera");
		if (!of_node) {
			log_err("mediatek,camera not configure in platform dts");
			return ERR_INVAL;
		}

		rc = of_property_read_u32(of_node, "version", (uint32 *)&dt_ver);
		if (rc < 0)
			log_err("version not found, use old solution");

		log_info("dt version:%d", dt_ver);
		init = 1;
	}

	*ver = dt_ver;

	return rc;
}

static int32_t camkit_sensor_driver_fill_subdev_data(
	struct sensor_dev_ctrl *dev_ctrl, struct device_node *node)
{
	int32 i = 0;
	int32 ret = ERR_NONE;
	struct platform_device *sub_dev = NULL;
	struct camera_dev_ctrl *sub_dev_ctrl = NULL;
	uint32 sensor_idx = 0;
	const char *sensor_idx_str = NULL;
	struct gpio_info *gpio = NULL;
	char pin_name[MAX_PIN_NAME_LEN] = {0};
	char *lookup_names = NULL;
	struct regulator_info *ldo = NULL;
	struct mclk_info *mclk = NULL;

	sub_dev = of_find_device_by_node(node);
	if (!sub_dev) {
		log_err("sub device not found");
		return ERR_INVAL;
	}

	ret = of_property_read_string(node, "sensor-index", &sensor_idx_str);
	if (ret) {
		log_err("sensor index not config, please check");
		return ERR_INVAL;
	}

	(void)sscanf_s(sensor_idx_str, "%u", &sensor_idx);
	log_info("sensor_index: %u", sensor_idx);
	if (sensor_idx >= CAMKIT_SENSOR_IDX_MAX_NUM) {
		log_err("sensor index out of range");
		return ERR_INVAL;
	}

	sub_dev_ctrl = get_camera_dev_ctrl(sensor_idx);
	if (!sub_dev_ctrl) {
		log_err("sub device ctrl not exist");
		return ERR_INVAL;
	}
	sub_dev_ctrl->sensor_idx = sensor_idx;

	sub_dev_ctrl->pinctrl = devm_pinctrl_get(&sub_dev->dev);
	if (IS_ERR(sub_dev_ctrl->pinctrl)) {
		log_err("Cannot find pinctrl");
		return ERR_INVAL;
	}

	sub_dev_ctrl->dev = &sub_dev->dev;

	// get the states of gpio pinctrls
	for (i = 0; i < CAMKIT_HW_PIN_MAX_NUM; i++) {
		gpio = &sub_dev_ctrl->gpio_table[i];
		lookup_names = find_pin_name((enum camkit_hw_pin_type)i, CAMKIT_PIN_GPIO);
		if (!lookup_names)
			continue;

		ret = snprintf_s(pin_name, sizeof(pin_name), sizeof(pin_name) - 1, "%s_1", lookup_names);
		if (ret < 0) {
			log_err("snprintf_s failed\n");
			return ERR_INVAL;
		}
 		gpio->gpio_active = pinctrl_lookup_state(
 			sub_dev_ctrl->pinctrl, pin_name);
		if (IS_ERR_OR_NULL(gpio->gpio_active)) {
			log_info("please check pin: %s in dtsi", pin_name);
			continue;
		}

		ret = snprintf_s(pin_name, sizeof(pin_name), sizeof(pin_name) - 1, "%s_0", lookup_names);
		if (ret < 0) {
			log_err("snprintf_s failed\n");
			return ERR_INVAL;
		}
 		gpio->gpio_suspend = pinctrl_lookup_state(
 			sub_dev_ctrl->pinctrl, pin_name);
		if (IS_ERR_OR_NULL(gpio->gpio_suspend)) {
			log_info("please check pin: %s in dtsi", pin_name);
			continue;
		}

		gpio->pin_type = (enum camkit_hw_pin_type)i;
		if (strncpy_s(gpio->name, MAX_PIN_NAME_LEN, lookup_names,
			MAX_PIN_NAME_LEN - 1) != EOK) {
			log_err("strncpy_s fail");
			return ERR_INVAL;
		}

		log_info("get gpio: %s pinctrl state ok", lookup_names);
	}

	// get the states of mclk pinctrls
	for (i = MCLK_STATE_DISABLE; i < MCLK_STATE_MAX; i++) {
		if (i == MCLK_STATE_DISABLE) {
			ret = snprintf_s(pin_name, sizeof(pin_name), sizeof(pin_name) - 1, "mclk_off");
			if (ret < 0) {
				log_err("snprintf_s failed\n");
				return ERR_INVAL;
			}
		} else {
			ret = snprintf_s(pin_name, sizeof(pin_name), sizeof(pin_name) - 1, "mclk_%dmA", i << 1);
			if (ret < 0) {
				log_err("snprintf_s failed\n");
				return ERR_INVAL;
			}
		}
		mclk = &(sub_dev_ctrl->mclk_table[i]);
		mclk->state = pinctrl_lookup_state(sub_dev_ctrl->pinctrl, pin_name);
		if (IS_ERR_OR_NULL(mclk->state))
			log_info("please check pin: %s in dtsi", pin_name);
		else
			log_info("get %s pinctrl state ok", pin_name);

		if (strncpy_s(mclk->name, MAX_PIN_NAME_LEN, pin_name,
			MAX_PIN_NAME_LEN - 1) != EOK) {
			log_err("strncpy_s fail");
			return ERR_INVAL;
		}
	}

	sub_dev_ctrl->mclk_idx = MCLK_STATE_ENABLE_4MA;

	// get regulator
	for (i = 0; i < CAMKIT_HW_PIN_MAX_NUM; i++) {
		ldo = &sub_dev_ctrl->ldo_table[i];
		lookup_names = find_pin_name(i, CAMKIT_PIN_LDO);
		if (!lookup_names)
			continue;

		ret = snprintf_s(pin_name, sizeof(pin_name), sizeof(pin_name) - 1, "%s", lookup_names);
		if (ret < 0) {
			log_err("snprintf_s failed\n");
			return ERR_INVAL;
		}
		if (strncpy_s(ldo->name, MAX_PIN_NAME_LEN, pin_name,
			MAX_PIN_NAME_LEN - 1) != EOK) {
			log_err("strncpy_s fail");
			return ERR_INVAL;
		}

		log_info("fill ldo name: %s ok", ldo->name);
	}

	//only to save the gpio pinctrl name, pinctrl state
	//will select again when sensor power on/off
	devm_pinctrl_put(sub_dev_ctrl->pinctrl);
	sub_dev_ctrl->pinctrl = NULL;

	return ERR_NONE;
}

static int32_t camkit_sensor_driver_parse_dt(
	struct sensor_dev_ctrl *dev_ctrl)
{
	int32 rc = ERR_NONE;
	struct device_node *parent = dev_ctrl->of_node;
	struct device_node *child = NULL;
	int32 cam_num;

	cam_num = of_get_child_count(parent);
 	rc = of_platform_populate(dev_ctrl->of_node, NULL, NULL, dev_ctrl->dev);
 	if (rc)
 		log_err("failed to add sub device, rc=%d", rc);

	for_each_child_of_node(parent, child) {
		camkit_sensor_driver_fill_subdev_data(dev_ctrl, child);
 	}

	return rc;
}

static int32_t camkit_sensor_driver_probe(
	struct platform_device *pdev)
{
	int32_t rc = 0;
 	struct sensor_dev_ctrl *camkit_dev_ctrl = NULL;

	pr_err("camkit_sensor_driver_probe");

 	/* Create sensor control structure */
 	camkit_dev_ctrl = devm_kzalloc(&pdev->dev,
 		sizeof(struct sensor_dev_ctrl), GFP_KERNEL);
 	if (!camkit_dev_ctrl)
 		return ERR_NOMEM;

 	camkit_dev_ctrl->pdev = pdev;
 	camkit_dev_ctrl->dev = &pdev->dev;
 	camkit_dev_ctrl->of_node = pdev->dev.of_node;

	camkit_sensor_driver_parse_dt(camkit_dev_ctrl);

	platform_set_drvdata(pdev, camkit_dev_ctrl);

	return rc;
}

static int camkit_sensor_driver_remove(struct platform_device *pdev)
{
	struct sensor_dev_ctrl *dev_ctrl = NULL;

	dev_ctrl = platform_get_drvdata(pdev);
	if (!dev_ctrl) {
		log_err("camkit device is NULL");
		return ERR_NONE;
	}

	log_info("remove camkit platform driver");

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, dev_ctrl);

	return 0;
}

static const struct of_device_id camkit_sensor_driver_dt_match[] = {
	{.compatible = "mediatek,camera"},
	{}
};

MODULE_DEVICE_TABLE(of, camkit_sensor_driver_dt_match);

static struct platform_driver camkit_sensor_driver = {
	.probe = camkit_sensor_driver_probe,
	.driver = {
		.name = "camera",
		.owner = THIS_MODULE,
		.of_match_table = camkit_sensor_driver_dt_match,
		.suppress_bind_attrs = true,
	},
	.remove = camkit_sensor_driver_remove,
};

static int __init camkit_sensor_driver_init(void)
{
	int32_t rc = 0;

	pr_err("camkit_sensor_driver_init");
	rc = platform_driver_register(&camkit_sensor_driver);
	if (rc < 0) {
		pr_err("platform_driver_register Failed: rc = %d", rc);
		return rc;
	}

	return rc;
}

static void __exit camkit_sensor_driver_exit(void)
{
	platform_driver_unregister(&camkit_sensor_driver);
}

//late_initcall(camkit_sensor_driver_init);
module_init(camkit_sensor_driver_init);
module_exit(camkit_sensor_driver_exit);

MODULE_DESCRIPTION("camkit device driver");
MODULE_LICENSE("GPL v2");