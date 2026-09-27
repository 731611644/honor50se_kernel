/*
 * camkit_driver_interface.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * camkit sensor interface layer
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

#include "camkit_driver_interface.h"

#include "camkit_probe_sensor.h"
#include "camkit_io_adapter.h"
#include "camkit_driver_impl.h"
#include "camkit_dts_adapter.h"
#include <securec.h>
#include "cam_btb_check.h"
#if (DRIVER_VERSION == 2)
#include "imgsensor.h"
extern struct IMGSENSOR gimgsensor;
#endif

uint32 g_log_mask;
struct camkit_sensor_index_info_t g_sensor_idx_info[CAMKIT_SENSOR_IDX_MAX_NUM] = {0};

int32 hwsensor_set_log_mask(const uint32 mask)
{
	g_log_mask = mask;

	return ERR_NONE;
}

void hwsensor_save_sensor_probe_info(uint32 sensor_idx)
{
	uint32 ret;
	struct camkit_sensor *kit_sensor = NULL;

	kit_sensor = get_camkit_sensor(sensor_idx);
	if (!kit_sensor) {
		log_err("invalid sensor index");
		return;
	}

	g_sensor_idx_info[sensor_idx].sensor_index = sensor_idx;
	ret = strncpy_s(g_sensor_idx_info[sensor_idx].sensor_name,
		SENSOR_NAME_LEN,
		kit_sensor->kit_params->module_params->sensor_name,
		SENSOR_NAME_LEN - 1);
	if (ret != ERR_NONE)
		log_err("strncpy_s fail, ret = %d\n", ret);
	kit_sensor->kit_params->sensor_params->sensor_info.probe_flag = 1;
}

static bool hwsensor_match_name_and_index(
	const char *sensor_name, uint32 kit_sensor_index)
{
	size_t name_length;
	enum IMGSENSOR_SENSOR_IDX sensor_index = kit_sensor_index;

	if (sensor_index < CAMKIT_SENSOR_IDX_MIN_NUM ||
		sensor_index >= CAMKIT_SENSOR_IDX_MAX_NUM) {
		log_err("invalid sensor index %d\n", sensor_index);
		return false;
	}

	name_length = strlen(sensor_name);
	if (name_length == 0) {
		log_err("sensor name is empty\n");
		return false;
	}

	if (!strncmp(sensor_name, g_sensor_idx_info[sensor_index].sensor_name,
		name_length))
		return true;

	return false;
}

static bool hwsensor_match_impdeance_sensor(
	struct camkit_sensor_params *sensor_params)
{
	uint8 i;
	uint8 total_num;
	uint32 kit_sensor_index;
	char *sensor_name = NULL;
	struct camkit_high_impedance_info_t *high_impedance = NULL;
	struct camkit_high_impedance_sensor_t *sensor = NULL;

	if (!sensor_params) {
		log_err("sensor_params is null\n");
		return false;
	}

	high_impedance = &sensor_params->high_impedance_info;
	high_impedance->matched_flag = 0;
	high_impedance->matched_index = 0;

	total_num = high_impedance->total_num;
	if (total_num == 0 || total_num > MAX_HIGH_IMPEDANCE_SENSOR_NUM) {
		log_info("invalid total num %u\n", total_num);
		return false;
	}

	sensor = &high_impedance->sensor[0];
	for (i = 0; i < total_num; i++) {
		sensor_name = sensor[i].sensor_name;
		kit_sensor_index = sensor[i].sensor_index;
		if (hwsensor_match_name_and_index(sensor_name, kit_sensor_index)) {
			high_impedance->matched_flag = 1; /* matched ok: 1 */
			high_impedance->matched_index = i;
			log_info("success match the high impedance sensor %s idx %u\n",
				sensor_name, i);
			return true;
		}
	}

	return false;
}

static struct camkit_hw_power_info_t *hwsensor_get_high_impedance_power_info(
	struct camkit_sensor_params *sensor_params, uint32 sensor_idx,
	enum camkit_power_status pwr_status)
{
	struct camkit_high_impedance_info_t *info = NULL;
	struct camkit_hw_power_info_t *pwr_info = NULL;

	/* match condition: 1.need high impedace 2.sensor probe ok */
	if ((sensor_params->sensor_info.need_high_impedance != 1) ||
		(sensor_params->sensor_info.probe_flag != 1)) {
		log_info("no need to find high impdance sensor info\n");
		return NULL;
	}

	if (hwsensor_match_impdeance_sensor(sensor_params) == false)
		return NULL;

	info = &sensor_params->high_impedance_info;
	if (pwr_status == CAMKIT_HW_POWER_STATUS_ON) {
		log_info("%s: get high_impedance power on info, match idx %u\n",
			__func__, info->matched_index);
		pwr_info = info->sensor[info->matched_index].power_info;
	} else {
		log_info("%s: get high_impedance power down info, match idx %u\n",
			__func__, info->matched_index);
		pwr_info = info->sensor[info->matched_index].power_down_info;
	}
	return pwr_info;
}

static struct camkit_hw_power_info_t *hwsensor_get_ext_power_info(
	struct camkit_sensor_params *sensor_params, uint32 sensor_idx,
	enum camkit_power_status pwr_status)
{
	uint32 power_setting_idx;
	struct camkit_hw_power_info_t *pwr_info = NULL;
	struct camkit_extend_hw_power_info_t *ext_info = NULL;
	struct camkit_sensor_power_settings_idx_map_t *power_setting_map = NULL;

	power_setting_map = get_power_setting_map();
	ext_info = &sensor_params->ext_power_info;
	power_setting_idx = power_setting_map[sensor_idx].power_setting_idx;
	if (power_setting_map[sensor_idx].is_map_vaild &&
		power_setting_idx < ext_info->total_num) {
		if (pwr_status == CAMKIT_HW_POWER_STATUS_ON) {
			log_info("%s: success get ext power on info, idx %u\n",
				__func__, power_setting_idx);
			pwr_info = ext_info->info_pair[power_setting_idx].power_info;
		} else {
			log_info("%s: success get ext power off info, idx %u\n",
				__func__, power_setting_idx);
			pwr_info =
				ext_info->info_pair[power_setting_idx].power_down_info;
		}
	}
	return pwr_info;
}

static struct camkit_hw_power_info_t *hwsensor_get_power_info(
	struct camkit_sensor_params *sensor_params, uint32 sensor_idx,
	enum camkit_power_status pwr_status)
{
	struct camkit_hw_power_info_t *power_info = NULL;

	if (!sensor_params) {
		log_err("sensor_params is null\n");
		return NULL;
	}

	if (sensor_idx < CAMKIT_SENSOR_IDX_MIN_NUM  ||
		sensor_idx >= CAMKIT_SENSOR_IDX_MAX_NUM) {
		log_err("%s: invalid sensor idx %u\n", __func__, sensor_idx);
		return NULL;
	}

	power_info = hwsensor_get_ext_power_info(
		sensor_params, sensor_idx, pwr_status);
	if (power_info != NULL) {
		return power_info;
	}

	power_info = hwsensor_get_high_impedance_power_info(
		sensor_params, sensor_idx, pwr_status);
	if (power_info != NULL) {
		return power_info;
	}

	if (pwr_status == CAMKIT_HW_POWER_STATUS_ON)
		return sensor_params->power_info;
	else
		return sensor_params->power_down_info;
}

int32 hwsensor_power(struct camkit_sensor *sensor,
	enum camkit_power_status pwr_status)
{
	struct camkit_hw_power_info_t *power_info = NULL;
	uint32 sensor_idx;
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(sensor);
	return_err_if_null(sensor->kit_params);
	return_err_if_null(sensor->kit_params->sensor_params);

	sensor_params = sensor->kit_params->sensor_params;
	sensor_idx = sensor->sensor_idx;

	power_info = hwsensor_get_power_info(
		sensor_params, sensor_idx, pwr_status);
	adopt_sensor_power_sequence(
		sensor_idx,
		pwr_status,
		power_info);

	return ERR_NONE;
}

uint32 hwsensor_match_id(const uint32 sensor_idx, uint32 *match_id)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->match_id);

	return kit_sensor->sensor_ops->match_id(kit_sensor, match_id);
}

char *hwsensor_get_sensor_name(const uint32 sensor_idx)
{
	struct camkit_sensor *kit_sensor = NULL;

	kit_sensor = get_camkit_sensor(sensor_idx);
	if (kit_sensor == NULL || kit_sensor->kit_params == NULL ||
		kit_sensor->kit_params->module_params == NULL) {
		log_err("invalid parameter");
		return NULL;
	}

	return kit_sensor->kit_params->module_params->sensor_name;
}

int32 hwsensor_open(const uint32 sensor_idx)
{
	uint32 ret;
#if (DRIVER_VERSION == 2)
	struct IMGSENSOR *pimgsensor = &gimgsensor;
#endif
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->sensor_open);

	check_camera_btb_gpio_info(sensor_idx);
	// 1. power on sensor
	ret = hwsensor_power(kit_sensor, CAMKIT_HW_POWER_STATUS_ON);
#if (DRIVER_VERSION == 2)
	if (pimgsensor->imgsensor_oc_irq_enable)
		pimgsensor->imgsensor_oc_irq_enable(sensor_idx, true);
#endif
	if (ret != ERR_NONE) {
		log_err("power on sensor failed");
		return ERR_INVAL;
	}

	// 2. open sensor
	ret = kit_sensor->sensor_ops->sensor_open(kit_sensor);
	if (ret != ERR_NONE) {
		log_err("open sensor failed");
		ret = hwsensor_power(kit_sensor, CAMKIT_HW_POWER_STATUS_OFF);
#if (DRIVER_VERSION == 2)
		if (pimgsensor->imgsensor_oc_irq_enable)
			pimgsensor->imgsensor_oc_irq_enable(sensor_idx, false);
#endif
		return ret;
	}

	log_info("open sensor[%d] successfully", sensor_idx);
	return ERR_NONE;
}

int32 hwsensor_close(const uint32 sensor_idx)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
#if (DRIVER_VERSION == 2)
	struct IMGSENSOR *pimgsensor = &gimgsensor;
#endif

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->sensor_close);

	kit_sensor->sensor_ops->sensor_close(kit_sensor);

	(void)hwsensor_power(kit_sensor, CAMKIT_HW_POWER_STATUS_OFF);
#if (DRIVER_VERSION == 2)
	if (pimgsensor->imgsensor_oc_irq_enable)
		pimgsensor->imgsensor_oc_irq_enable(sensor_idx, false);
#endif
	return ERR_NONE;
}

uint32 hwsensor_get_sensor_info(const uint32 sensor_idx,
	struct camkit_sensor_info_t *sensor_info)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_sensor_info);
	return_err_if_null(sensor_info);

	kit_sensor->sensor_ops->get_sensor_info(kit_sensor, sensor_info);

	return ERR_NONE;
}

uint32 hwsensor_control(const uint32 sensor_idx, const uint32 scenario_id)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->control);

	return kit_sensor->sensor_ops->control(kit_sensor, scenario_id);
}

uint32 hwsensor_get_gain_range(const uint32 sensor_idx,
	uint32 *min_gain, uint32 *max_gain)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->kit_params);
	return_err_if_null(kit_sensor->kit_params->sensor_params);
	return_err_if_null(min_gain);
	return_err_if_null(max_gain);

	sensor_params = kit_sensor->kit_params->sensor_params;
	*min_gain = sensor_params->aec_info.min_gain;
	*max_gain = sensor_params->aec_info.max_gain;

	return ERR_NONE;
}

uint32 hwsensor_get_gain_info(const uint32 sensor_idx,
	uint32 *min_iso, uint32 *gain_step, uint32 *gain_type)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->kit_params);
	return_err_if_null(kit_sensor->kit_params->sensor_params);
	return_err_if_null(min_iso);
	return_err_if_null(gain_step);
	return_err_if_null(gain_type);

	sensor_params = kit_sensor->kit_params->sensor_params;
	*min_iso = sensor_params->aec_info.min_iso;
	*gain_step = sensor_params->aec_info.gain_step;
	*gain_type = sensor_params->aec_info.gain_type;

	return ERR_NONE;
}

uint32 hwsensor_get_min_shutter(const uint32 sensor_idx,
	uint32 *min_shutter)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->kit_params);
	return_err_if_null(kit_sensor->kit_params->sensor_params);
	return_err_if_null(min_shutter);

	sensor_params = kit_sensor->kit_params->sensor_params;
	*min_shutter = sensor_params->aec_info.min_linecount;

	return ERR_NONE;
}

uint32 hwsensor_get_period(const uint32 sensor_idx,
	uint16 *line_length, uint16 *frame_length)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->kit_params);
	return_err_if_null(kit_sensor->kit_params->sensor_params);
	return_err_if_null(line_length);
	return_err_if_null(frame_length);

	sensor_params = kit_sensor->kit_params->sensor_params;
	*line_length = sensor_params->sensor_ctrl.line_length;
	*frame_length = sensor_params->sensor_ctrl.frame_length;

	return ERR_NONE;
}

uint32 hwsensor_get_pclk(const uint32 sensor_idx, uint32 *pclk)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->kit_params);
	return_err_if_null(kit_sensor->kit_params->sensor_params);
	return_err_if_null(pclk);

	sensor_params = kit_sensor->kit_params->sensor_params;
	*pclk = sensor_params->sensor_ctrl.pclk;

	return ERR_NONE;
}

uint32 hwsensor_get_checksum(const uint32 sensor_idx, uint32 *checksum)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->kit_params);
	return_err_if_null(kit_sensor->kit_params->sensor_params);
	return_err_if_null(checksum);

	sensor_params = kit_sensor->kit_params->sensor_params;
	*checksum = sensor_params->sensor_info.checksum_value;

	return ERR_NONE;
}

uint32 hwsensor_get_scenario_pclk(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *pclk)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_scenario_pclk);
	return_err_if_null(pclk);

	return kit_sensor->sensor_ops->get_scenario_pclk(
		kit_sensor, scenario_id, pclk);
}

uint32 hwsensor_get_scenario_period(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *period)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_scenario_period);
	return_err_if_null(period);

	return kit_sensor->sensor_ops->get_scenario_period(
		kit_sensor, scenario_id, period);
}

uint32 hwsensor_set_shutter(const uint32 sensor_idx,
	const uint32 shutter)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_shutter);

	return kit_sensor->sensor_ops->set_shutter(kit_sensor, shutter);
}

uint32 hwsensor_set_gain(const uint32 sensor_idx,
	const uint16 gain)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_gain);

	return kit_sensor->sensor_ops->set_gain(kit_sensor, gain);
}

uint32 hwsensor_set_awb_gain(const uint32 sensor_idx,
	struct sensor_awb_gain *set_sensor_awb)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_awb_gain);

	return kit_sensor->sensor_ops->set_awb_gain(kit_sensor, set_sensor_awb);
}

uint32 hwsensor_set_video_mode(const uint32 sensor_idx,
	const uint16 framerate)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_video_mode);

	return kit_sensor->sensor_ops->set_video_mode(kit_sensor, framerate);
}

uint32 hwsensor_set_auto_flicker(const uint32 sensor_idx,
	const uint8 enable, const uint16 framerate)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_auto_flicker);

	return kit_sensor->sensor_ops->set_auto_flicker(
		kit_sensor, enable, framerate);
}

uint32 hwsensor_set_current_fps(const uint32 sensor_idx,
	const uint16 framerate)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_current_fps);

	return kit_sensor->sensor_ops->set_current_fps(
		kit_sensor, framerate);
}

uint32 hwsensor_streaming_control(const uint32 sensor_idx, uint8 enable)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->streaming_control);

	return kit_sensor->sensor_ops->streaming_control(
		kit_sensor, enable);
}

uint32 hwsensor_get_crop_info(const uint32 sensor_idx,
	const uint32 scenario_id,  struct camkit_sensor_output_info_t *win_info)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_crop_info);
	return_err_if_null(win_info);

	return kit_sensor->sensor_ops->get_crop_info(
		kit_sensor, scenario_id, win_info);
}

uint32 hwsensor_get_pdaf_info(const uint32 sensor_idx,
	const uint32 scenario_id, struct camkit_sensor_pdaf_info_t *pdaf_info)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_pdaf_info);
	return_err_if_null(pdaf_info);

	return kit_sensor->sensor_ops->get_pdaf_info(
		kit_sensor, scenario_id, pdaf_info);
}

uint32 hwsensor_set_test_pattern(const uint32 sensor_idx, uint8 enable)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_test_pattern);

	return kit_sensor->sensor_ops->set_test_pattern(kit_sensor, enable);
}

uint32 hwsensor_dump_reg(const uint32 sensor_idx)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->dump_reg);

	return kit_sensor->sensor_ops->dump_reg(kit_sensor);
}

uint32 hwsensor_get_mipi_pixel_rate(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *mipi_pixel_rate)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_mipi_pixel_rate);
	return_err_if_null(mipi_pixel_rate);

	return kit_sensor->sensor_ops->get_mipi_pixel_rate(
		kit_sensor, scenario_id, mipi_pixel_rate);
}

uint32 hwsensor_get_mipi_trail_val(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *mipi_trail_val)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_mipi_trail_val);
	return_err_if_null(mipi_trail_val);
	return kit_sensor->sensor_ops->get_mipi_trail_val(
		kit_sensor, scenario_id, mipi_trail_val);
}

uint32 hwsensor_get_mipi_mask_val(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *mipi_mask_val)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_mipi_mask_val);
	return_err_if_null(mipi_mask_val);
	return kit_sensor->sensor_ops->get_mipi_mask_val(
		kit_sensor, scenario_id, mipi_mask_val);
}

uint32 hwsensor_get_awb_req_by_scenario(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *need_set_awb_gain)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_mipi_mask_val);
	return_err_if_null(need_set_awb_gain);
	return kit_sensor->sensor_ops->get_awb_req_by_scenario(
		kit_sensor, scenario_id, need_set_awb_gain);
}

uint32 hwsensor_get_sensor_pixel_rate(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *pixel_rate)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_sensor_pixel_rate);
	return_err_if_null(pixel_rate);

	return kit_sensor->sensor_ops->get_sensor_pixel_rate(
		kit_sensor, scenario_id, pixel_rate);
}

uint32 hwsensor_set_pdaf_mode(const uint32 sensor_idx,
	const uint16 pdaf_mode)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_pdaf_mode);

	log_info("PDAF mode :%d", pdaf_mode);

	return kit_sensor->sensor_ops->set_pdaf_mode(kit_sensor, pdaf_mode);
}

uint32 hwsensor_set_pdaf_setting(const uint32 sensor_idx,
	uint16 *setting, const uint32 reg_num)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_pdaf_setting);
	return_err_if_null(setting);

	log_info("set pdaf setting size %d\n", reg_num);

	return kit_sensor->sensor_ops->set_pdaf_setting(
		kit_sensor, setting, reg_num);
}

uint32 hwsensor_get_pdaf_regs_data(const uint32 sensor_idx,
	uint16 *reg_pairs, const uint32 reg_num)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_pdaf_regs_data);
	return_err_if_null(reg_pairs);

	log_info("get pdaf setting size %d", reg_num);
	return kit_sensor->sensor_ops->get_pdaf_regs_data(
		kit_sensor, reg_pairs, reg_num);
}

uint32 hwsensor_get_vc_info(const uint32 sensor_idx,
	const uint32 scenario_id, struct camkit_sensor_vc_info_t *vc_info)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_vc_info);
	return_err_if_null(vc_info);

	return kit_sensor->sensor_ops->get_vc_info(
		kit_sensor, scenario_id, vc_info);
}

uint32 hwsensor_get_binning_ratio(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *ratio)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_binning_ratio);
	return_err_if_null(ratio);

	return kit_sensor->sensor_ops->get_binning_ratio(
		kit_sensor, scenario_id, ratio);
}

uint32 hwsensor_set_shutter_frame_length(const uint32 sensor_idx,
	uint32 shutter, uint32 frame_length)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_shutter_frame_length);

	return kit_sensor->sensor_ops->set_shutter_frame_length(
		kit_sensor, shutter, frame_length);
}

uint32 hwsensor_get_default_framerate(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *framerate)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_default_framerate);

	return kit_sensor->sensor_ops->get_default_framerate(
		kit_sensor, scenario_id, framerate);
}

uint32 hwsensor_get_pdaf_capacity(const uint32 sensor_idx,
	const uint32 scenario_id, uint32 *pdaf_support, uint32 para_len)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->get_pdaf_capacity);

	return kit_sensor->sensor_ops->get_pdaf_capacity(
		kit_sensor, scenario_id, pdaf_support, para_len);
}

uint32 hwsensor_set_scenario_framerate(const uint32 sensor_idx,
	const uint32 scenario_id, const uint32 framerate)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->sensor_ops);
	return_err_if_null(kit_sensor->sensor_ops->set_scenario_framerate);

	return kit_sensor->sensor_ops->set_scenario_framerate(
		kit_sensor, scenario_id, framerate);
}

uint32 hwsensor_get_mclk_drv_current(const uint32 sensor_idx, uint32 *mclk_current)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->kit_params);
	return_err_if_null(kit_sensor->kit_params->sensor_params);
	return_err_if_null(mclk_current);

	sensor_params = kit_sensor->kit_params->sensor_params;
	*mclk_current = sensor_params->sensor_info.isp_driving_current + 1;

	return ERR_NONE;
}

uint32 hwsensor_get_cam_tcpm_ctrl(const uint32 sensor_idx,
	uint32 *cam_tcpm_ctrl)
{
	struct camkit_sensor *kit_sensor = get_camkit_sensor(sensor_idx);
	struct camkit_sensor_params *sensor_params = NULL;

	return_err_if_null(kit_sensor);
	return_err_if_null(kit_sensor->kit_params);
	return_err_if_null(kit_sensor->kit_params->sensor_params);
	return_err_if_null(cam_tcpm_ctrl);

	sensor_params = kit_sensor->kit_params->sensor_params;
	*cam_tcpm_ctrl = (uint32)sensor_params->sensor_info.cam_tcpm_ctrl;

	return ERR_NONE;
}
