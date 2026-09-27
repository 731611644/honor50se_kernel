/*
 * camkit_dts_adapter.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * parse pin type and value from dts
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
#include <linux/kernel.h>
#include "camkit_dts_adapter.h"

#ifdef CONFIG_OF
/* device tree */
#include <linux/of.h>
#endif

#include <securec.h>

#include "camkit_driver_types.h"

#define PIN_TYPE "pin-type"
#define PIN_ID   "pin-id"

static struct IMGSENSOR_HW_CFG hw_config[IMGSENSOR_SENSOR_IDX_MAX_NUM];
static struct camkit_sensor_power_settings_idx_map_t g_power_setting_map[IMGSENSOR_SENSOR_IDX_MAX_NUM];

struct pin_property {
	const char *pin_name;
	enum IMGSENSOR_HW_PIN pin_type;
};

static const struct pin_property pin_prop_table[] = {
	{ "pwdn", IMGSENSOR_HW_PIN_PDN },
	{ "rst", IMGSENSOR_HW_PIN_RST },
	{ "avdd_en", IMGSENSOR_HW_PIN_AVDD_EN },
	{ "avdd_sel", IMGSENSOR_HW_PIN_AVDD_SEL },
	{ "dvdd_en", IMGSENSOR_HW_PIN_DVDD_EN },
	{ "dvdd_sel", IMGSENSOR_HW_PIN_DVDD_SEL },
	{ "iovdd_en", IMGSENSOR_HW_PIN_IOVDD_EN },
	{ "avdd1_en", IMGSENSOR_HW_PIN_AVDD1_EN },
	{ "afvdd_en", IMGSENSOR_HW_PIN_AFVDD_EN },
	{ "rst1", IMGSENSOR_HW_PIN_RST1 },
	{ "5v_boost", IMGSENSOR_HW_PIN_5V_BOOST },
	{ "avdd", IMGSENSOR_HW_PIN_AVDD },
	{ "avdd1", IMGSENSOR_HW_PIN_AVDD1 },
	{ "dvdd", IMGSENSOR_HW_PIN_DVDD },
	{ "dovdd", IMGSENSOR_HW_PIN_DOVDD },
	{ "vmch", IMGSENSOR_HW_PIN_VMCH },
	{ "afvdd", IMGSENSOR_HW_PIN_AFVDD },
	{ "ldo1_pmic", IMGSENSOR_HW_PIN_LDO1_PMIC },
	{ "ldo2_pmic", IMGSENSOR_HW_PIN_LDO2_PMIC },
	{ "ldo3_pmic", IMGSENSOR_HW_PIN_LDO3_PMIC },
	{ "ldo4_pmic", IMGSENSOR_HW_PIN_LDO4_PMIC },
	{ "xbuck1_pmic", IMGSENSOR_HW_PIN_XBUCK1_PMIC },
#ifdef MIPI_SWITCH
	{ "mipi_sw_en", IMGSENSOR_HW_PIN_MIPI_SWITCH_EN },
	{ "mipi_sw_sel", IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL },
#endif
	{ "mclk", IMGSENSOR_HW_PIN_MCLK },
};

static void get_power_info_from_dts(struct device_node *of_node,
	struct IMGSENSOR_HW_CFG *cam_cfg)
{
	int pin_count;
	int id_count;
	int i;
	int j;
	int max_size;
	int ret;
	unsigned int ids[IMGSENSOR_HW_POWER_INFO_MAX] = {0};
	const char *pin_names[IMGSENSOR_HW_POWER_INFO_MAX] = {""};

	pin_count = of_property_count_strings(of_node, PIN_TYPE);
	if (pin_count <= 0 || pin_count >= IMGSENSOR_HW_POWER_INFO_MAX) {
		log_err("pin_count = %d", pin_count);
		return;
	}
	log_info("pin_count: %d\n", pin_count);
	ret = of_property_read_string_array(of_node, PIN_TYPE,
		pin_names, pin_count);
	if (ret < 0) {
		log_err("get pin-type fail");
		return;
	}

	id_count = of_property_count_elems_of_size(of_node,
		PIN_ID, sizeof(uint32));
	if (id_count != pin_count) {
		log_err("id and pin count:%d mismatch", id_count);
		return;
	}
	log_info("id_count: %d\n", id_count);
	ret = of_property_read_u32_array(of_node, PIN_ID,
		(unsigned int *)ids, id_count);
	if (ret < 0) {
		log_err("get pin-ids fail");
		return;
	}

	max_size = camkit_array_size(pin_prop_table);
	for (i = 0; i < pin_count; i++) {
		for (j = 0; j < max_size; j++) {
			if (!strcmp(pin_names[i], pin_prop_table[j].pin_name)) {
				cam_cfg->pwr_info[i].pin = pin_prop_table[j].pin_type;
				cam_cfg->pwr_info[i].id = (enum IMGSENSOR_HW_ID)ids[i];
				dts_dbg("pwr_info[%d].pin: %u", i, cam_cfg->pwr_info[i].pin);
				dts_dbg("pwr_info[%d].id: %u", i, cam_cfg->pwr_info[i].id);
				break;
			}
		}
	}
}

struct IMGSENSOR_HW_CFG *get_sensor_hw_cfg(void)
{
	int rc;
	int32 i;
	uint32 sensor_index = 0;
	uint32 i2c_index = 0;
	const char *sensor_index_str = NULL;
	int32 cam_num;
	struct device_node *of_node = NULL;
	struct device_node *child_node = NULL;
	char child_node_name[32] = {0};
	struct IMGSENSOR_HW_CFG *cam_cfg = NULL;

	of_node = of_find_compatible_node(NULL, NULL, "mediatek,camera");
	if (!of_node) {
		log_err("mediatek,camera not configure in platform dts");
		return hw_config;
	}

	cam_num = of_get_child_count(of_node);
	dts_dbg("cam_num: %d", cam_num);
	if (cam_num <= 0 || cam_num >= IMGSENSOR_SENSOR_IDX_MAX_NUM) {
		log_err("DTS configuration is wrong, please check");
		return hw_config;
	}

	// Initialize sensor index to be the same as SENSOR_IDX.
	for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++)
		hw_config[i].sensor_idx = i;

	for (i = 0; i < cam_num; i++) {
		(void)sprintf_s(child_node_name, sizeof(child_node_name),
			"huawei,camera%d", i);
		dts_dbg("of node name: %s", child_node_name);
		child_node = of_find_compatible_node(NULL, NULL,
			child_node_name);

		of_property_read_string(child_node, "sensor-index",
			&sensor_index_str);
		(void)sscanf_s(sensor_index_str, "%u", &sensor_index);
		dts_dbg("sensor_index: %u", sensor_index);
		if (sensor_index >= IMGSENSOR_SENSOR_IDX_MAX_NUM) {
			log_err("sensor index out of range");
			return hw_config;
		}
		cam_cfg = hw_config + sensor_index;
		cam_cfg->sensor_idx = (enum IMGSENSOR_SENSOR_IDX)sensor_index;

		rc = of_property_read_u32(child_node, "i2c-dev-index",
			(uint32 *)&i2c_index);
		if (rc < 0) {
			log_err("get i2c-dev-index fail");
			return hw_config;
		}
		dts_dbg("i2c_index: %u", i2c_index);
		cam_cfg->i2c_dev = (enum IMGSENSOR_I2C_DEV)i2c_index;

		(void)get_power_info_from_dts(child_node, cam_cfg);
	}

	return hw_config;
}

enum IMGSENSOR_I2C_DEV get_sensor_i2c_dev(const int sensor_idx)
{
	int index = sensor_idx;

	if (index < 0 || index >= IMGSENSOR_SENSOR_IDX_MAX_NUM) {
		log_err("sensor index[%u] out of range", sensor_idx);
		index = IMGSENSOR_SENSOR_IDX_MAIN;
	}

	return hw_config[index].i2c_dev;
}

struct camkit_sensor_power_settings_idx_map_t *get_power_setting_map(void)
{
	errno_t ret;
	int i;
	struct device_node *of_node = NULL;
	struct device_node *child_node = NULL;
	char child_node_name[32] = {0};
	struct camkit_sensor_power_settings_idx_map_t *pwr_cfg = NULL;
	uint32 sensor_index = 0;
	const char *sensor_index_str = NULL;
	int32 cam_num;
	uint32 power_setting_idx = 0;
	uint32 map_valid = 0;
	static uint32 get_flag = 0;

	if (get_flag == 1)
		return g_power_setting_map;

	log_info("%s: enter\n", __func__);
	ret = memset_s(g_power_setting_map, sizeof(g_power_setting_map),
		0, sizeof(g_power_setting_map));
	if (ret != EOK) {
		log_err("%s: power settings map memset failed, ret %d\n", __func__, ret);
		return g_power_setting_map;
	}

	of_node = of_find_compatible_node(NULL, NULL, "mediatek,camera");
	if (!of_node) {
		log_info("%s: not support power info extend by dts\n", __func__);
		return g_power_setting_map;
	}

	cam_num = of_get_child_count(of_node);
	dts_dbg("cam_num: %d", cam_num);
	if (cam_num <= 0 || cam_num >= IMGSENSOR_SENSOR_IDX_MAX_NUM) {
		log_err("DTS configuration is wrong, please check");
		return g_power_setting_map;
	}

	for (i = 0; i < cam_num; i++) {
		(void)sprintf_s(child_node_name, sizeof(child_node_name),
			"huawei,camera%d", i);
		child_node = of_find_compatible_node(NULL, NULL,
			child_node_name);

		of_property_read_string(child_node, "sensor-index",
			&sensor_index_str);
		(void)sscanf_s(sensor_index_str, "%u", &sensor_index);
		if (sensor_index >= IMGSENSOR_SENSOR_IDX_MAX_NUM) {
			log_err("sensor index out of range");
			return g_power_setting_map;
		}

		pwr_cfg = g_power_setting_map + sensor_index;
		ret = of_property_read_u32(child_node, "power-setting-idx",
			(u32 *)&power_setting_idx);
		if (ret < 0 || power_setting_idx > MAX_EXT_POWER_INFO_NUM) {
			log_info("%s: read power-setting-idx failed\n", __func__);
			return g_power_setting_map;
		}

		ret = of_property_read_u32(child_node, "is_map_vaild", (u32 *)&map_valid);
		if (ret < 0) {
			log_info("%s: read is_map_vaild failed\n", __func__);
			return g_power_setting_map;
		}
		log_info("%s:sensor-index = %u, map_valid = %u, power_setting_idx=%u\n",
			__func__, sensor_index, map_valid, power_setting_idx);
		pwr_cfg->is_map_vaild = map_valid;
		pwr_cfg->power_setting_idx = power_setting_idx;
	}

	get_flag = 1;
	return g_power_setting_map;
}

