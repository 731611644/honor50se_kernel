/*
 * c9922qcq_car.c
 *
 * Copyright (c) 2021-2021 Honor Technologies Co., Ltd.
 *
 * customized sensor driver
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

#include <linux/spinlock.h>
#include <securec.h>

#include "camkit_driver_impl.h"
#include "camkit_sensor_i2c.h"

extern spinlock_t camkit_lock;

/*
 * for long shutter mode which has different setting for NormalToLong and LongToNormal, such as ov13b10
*/
static enum sensor_shutter_mode get_shutter_mode_no_delay(uint32 in_shutter,
		struct camkit_aec_info_t *aec_info)
{
	static uint32 pre_shutter; /* shutter be set in previous((N-1)th) frame. */
	enum sensor_shutter_mode shutter_mode = SHUTTER_DEFAULT;
	uint32 normal_max_shutter = aec_info->max_frame_length - aec_info->vts_offset;

	if (pre_shutter == 0)
		pre_shutter = in_shutter;

	aaa_dbg("pre shutter is %u", pre_shutter);
	aaa_dbg("cur shutter is %u", in_shutter);
	/* update shutter_mode base on in_shutter and previous shutter */
	if (in_shutter > normal_max_shutter &&
		pre_shutter <= normal_max_shutter) {
		/*
		 * the shutter to be set this time is long,
		 * but last is normal, the mode is NORMAL2LONG
		 */
		log_info("NORMAL2LONG");
		shutter_mode = SHUTTER_NORMAL2LONG;
	} else if (in_shutter <= normal_max_shutter &&
		   pre_shutter > normal_max_shutter) {
		/*
		 * the shutter to be set this time is normal,
		 * but last is long, the mode is LONG2NORMAL
		 */
		log_info("LONG2NORMAL");
		shutter_mode = SHUTTER_LONG2NORMAL;
	} else {
		/*
		 * the shutter to be set this time is normal,
		 * and last is normal too, the mode is NORMAL
		 */
		aaa_dbg("NORMAL");
		shutter_mode = SHUTTER_DEFAULT;
	}
	pre_shutter = in_shutter;

	return shutter_mode;
}

/*
 * for long shutter mode, such as ov13b10
*/
static void select_expo_map_by_mode(struct camkit_aec_info_t *aec_info,
	enum sensor_shutter_mode shutter_mode,
	struct aec_ops_map **expo_ops_map)
{
	/* default expo ops map */
	*expo_ops_map = &(aec_info->expo_ops_map);

	switch (shutter_mode) {
	case SHUTTER_NORMAL2LONG:
		*expo_ops_map = &(aec_info->normal2long_ops_map);
		log_info("SHUTTER_NORMAL2LONG\n");
		break;
	case SHUTTER_LONG2NORMAL:
		*expo_ops_map = &(aec_info->long2normal_ops_map);
		log_info("SHUTTER_LONG2NORMAL\n");
		break;
	default:
		log_info("Default Mode\n");
		break;
	}

	return;
}

static void c9922qcp_car_get_expo_info(struct camkit_aec_info_t *aec_info,
	uint32 in_shutter,
	struct aec_ops_map **out_expo_map)
{
	enum sensor_shutter_mode shutter_mode;
	if (aec_info == NULL || out_expo_map == NULL) {
		log_err("input parameters is unavailable, return default\n");
		return;
	}

	shutter_mode = get_shutter_mode_no_delay(in_shutter, aec_info);
	select_expo_map_by_mode(aec_info, shutter_mode, out_expo_map);

	aaa_dbg("shutter_mode: %d, in_shutter: %d\n", shutter_mode, in_shutter);
	return;
}

uint32 c9922qcp_car_set_shutter_frame_length(struct camkit_sensor *sensor,
	uint32 shutter, uint32 frame_length)
{
	int32 ret;

	struct camkit_aec_info_t *aec_info = NULL;
	struct aec_ops_map *expo_ops_map = NULL;
	struct sensor_setting_cfg setting_cfg;
	struct aec_ctrl_cfg aec_ctrl;
	struct camkit_sensor_ctrl_t *sensor_ctrl = NULL;
	unsigned long flags;
	uint16 shift = 0;
	struct camkit_sensor_params *params = NULL;

	return_err_if_null(sensor);
	return_err_if_null(sensor->sensor_ops);
	return_err_if_null(sensor->kit_params);
	return_err_if_null(sensor->kit_params->sensor_params);
	params = sensor->kit_params->sensor_params;

	log_info("input shutter is %u", shutter);

	sensor_ctrl = &(params->sensor_ctrl);
	aec_info = &(params->aec_info);
	expo_ops_map = &(aec_info->expo_ops_map);

	(void)memset_s(&setting_cfg, sizeof(setting_cfg), 0, sizeof(setting_cfg));
	(void)memset_s(&aec_ctrl, sizeof(aec_ctrl), 0, sizeof(aec_ctrl));

	/*
	 * 1. confirm the shutter whether it is
	 * in the min and max line count threshold
	 */
	if (shutter < aec_info->min_linecount)
		shutter = aec_info->min_linecount;

	c9922qcp_car_get_expo_info(aec_info, shutter, &expo_ops_map);

	aec_ctrl.line_count = shutter;
	aec_ctrl.lc_valid = true;

	aec_ctrl.shift = shift;
	aec_ctrl.shift_valid = true;

	/* 2. calc frame length depend on shutter */
	aaa_dbg("[%s] lock", params->sensor_name);
	spin_lock(&camkit_lock);
	aaa_dbg("[%s] lock in", params->sensor_name);
	if (frame_length < sensor_ctrl->min_frame_length)
		frame_length = sensor_ctrl->min_frame_length;
	if (shutter > frame_length - aec_info->vts_offset || shift > 0)
		sensor_ctrl->frame_length = shutter + aec_info->vts_offset;
	else
		sensor_ctrl->frame_length = frame_length;

	if ((params->sensor_info.need_extra_vts_offset == 1) &&
		(sensor_ctrl->frame_length >= sensor_ctrl->last_shutter) &&
		(sensor_ctrl->frame_length - sensor_ctrl->last_shutter < aec_info->extra_vts_offset)) {
		log_info("[%s] frame_length(%u) < last_shutter(%u) + extra_vts_offset(%u), extend frame_length\n",
			params->sensor_name, sensor_ctrl->frame_length,
			sensor_ctrl->last_shutter, aec_info->extra_vts_offset);
		sensor_ctrl->frame_length += aec_info->extra_vts_offset;
	}

	if (sensor_ctrl->frame_length > aec_info->max_frame_length)
		sensor_ctrl->frame_length = aec_info->max_frame_length;

	/* 3. calc and update framelength to abandon the flicker issue */
	(void)camkit_abandon_flicker(sensor_ctrl);

	aec_ctrl.frame_length = sensor_ctrl->frame_length;
	aec_ctrl.fl_valid = true;

	aaa_dbg("[%s] lock down", params->sensor_name);
	spin_unlock(&camkit_lock);
	aaa_dbg("[%s] unlock", params->sensor_name);

	aaa_dbg("[%s] shutter = 0x%x, fl = 0x%x", params->sensor_name,
		shutter, aec_ctrl.frame_length);

	camkit_fill_aec_array(expo_ops_map, &aec_ctrl, &setting_cfg);

	// write registers to sensor
	ret = camkit_write_setting_cfg(sensor_ctrl, &setting_cfg);
	if (ret < 0) {
		log_err("write shutter failed, shutter: %u\n", shutter);
		return ret;
	}

	spin_lock_irqsave(&camkit_lock, flags);
	sensor_ctrl->shutter = shutter;
	sensor_ctrl->last_shutter = shutter;
	spin_unlock_irqrestore(&camkit_lock, flags);

	return ERR_NONE;
}

static struct sensor_kit_ops c9922qcp_car_ops = {
	.sensor_open = camkit_open,
	.sensor_close = camkit_close,
	.match_id = camkit_match_id,
	.sensor_init = camkit_sensor_init,
	.get_sensor_info = camkit_get_sensor_info,
	.control = camkit_control,
	.get_scenario_pclk = camkit_get_scenario_pclk,
	.get_scenario_period = camkit_get_scenario_period,
	.set_test_pattern = camkit_set_test_pattern,
	.dump_reg = camkit_dump_reg,
	.set_auto_flicker = camkit_set_auto_flicker,
	.get_default_framerate = camkit_get_default_framerate,
	.get_crop_info = camkit_get_crop_info,
	.streaming_control = camkit_streaming_control,
	.get_mipi_pixel_rate = camkit_get_mipi_pixel_rate,
	.get_mipi_trail_val = camkit_get_mipi_trail_val,
	.get_sensor_pixel_rate = camkit_get_sensor_pixel_rate,
	.get_pdaf_capacity = camkit_get_pdaf_capacity,
	.get_binning_ratio = camkit_get_binning_ratio,
	.get_pdaf_info = camkit_get_pdaf_info,
	.get_vc_info = camkit_get_vc_info,
	.get_pdaf_regs_data = camkit_get_pdaf_regs_data,
	.set_pdaf_setting = camkit_set_pdaf_setting,
	.set_video_mode = camkit_set_video_mode,
	.set_shutter = camkit_set_shutter,
	.set_gain = camkit_set_gain,
	.set_dummy = camkit_set_dummy,
	.set_max_framerate = camkit_set_max_framerate,
	.set_scenario_framerate = camkit_set_scenario_framerate,
	.set_shutter_frame_length = c9922qcp_car_set_shutter_frame_length,
	.set_current_fps = camkit_set_current_fps,
	.set_pdaf_mode = camkit_set_pdaf_mode,
};

uint32 get_c9922qcp_car_ops(struct sensor_kit_ops **ops)
{
	if (ops != NULL) {
		*ops = &c9922qcp_car_ops;
	} else {
		log_err("get c9922qcp_car_ops operators fail");
		return ERR_INVAL;
	}

	log_info("get c9922qcp_car operators OK");
	return ERR_NONE;
}

register_customized_driver(
	c9922qcp_car,
	CAMKIT_SENSOR_IDX_MAIN,
	C9922QCP_M100_CAR_SENSOR_ID,
	get_c9922qcp_car_ops);

