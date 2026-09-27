/*
 * multi_ic_check.c
 *
 * multi ic check interface for power module
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
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

#include <linux/types.h>
#include <mt-plat/charger_type.h>
#include <huawei_platform/log/hw_log.h>
#include <linux/power/huawei_charger.h>
#include <chipset_common/hwpower/battery_temp.h>
#include <chipset_common/hwpower/power_dsm.h>
#include <chipset_common/hwpower/power_dts.h>
#include <chipset_common/hwpower/btb_check.h>
#include <huawei_platform/power/direct_charger/multi_ic_check.h>

#define MULTI_ERR_STRING_LEN  256
#define HIGH_POWER_CUR_TH     800
#define SINGLE_IBUS_MAX       4000

#define BASP_FIRST_CC_CYCLE_MIN  30
#define BASP_FIRST_CC_CYCLE_MAX  100
#define BASP_FIRST_CC_VTER_DEC   30
#define BASP_FIRST_CC_VTER_ASC   20

static struct multi_ic_unbalanced_current_sc_para g_sc_para;

#define HWLOG_TAG multi_ic_check
HWLOG_REGIST();

extern bool gauge_get_current(int *bat_current);
extern int gauge_get_average_current(bool *valid);

static char *multi_ic_get_current_ratio_name(struct multi_ic_check_para *info)
{
	const char *main_ic_name = NULL;
	const char *aux_ic_name = NULL;
	int i;

	main_ic_name = dc_get_ic_name(SC_MODE, CHARGE_IC_MAIN);
	if (main_ic_name == NULL)
		main_ic_name = dc_get_ic_name(LVC_MODE, CHARGE_IC_MAIN);

	aux_ic_name = dc_get_ic_name(SC_MODE, CHARGE_IC_AUX);
	if (aux_ic_name == NULL)
		aux_ic_name == dc_get_ic_name(LVC_MODE, CHARGE_IC_AUX);

	hwlog_info("main_ic_name:%s aux_ic_name:%s\n", main_ic_name, aux_ic_name);
	for (i = 0; i < MULTI_IC_COMB_NAME_PARA_LEVEL; i++) {
		if (strcmp(main_ic_name, info->comb_name[i].main_ic_name) == 0 &&
			strcmp(aux_ic_name, info->comb_name[i].aux_ic_name) == 0)
			return info->comb_name[i].current_ratio_name;
	}
	return "current_ratio";
}

static void multi_ic_parse_comb_name_para(struct device_node *np,
	struct multi_ic_check_para *info)
{
	int i, row, col, array_len;
	const char *tmp_string = NULL;

	array_len = power_dts_read_count_strings(power_dts_tag(HWLOG_TAG), np,
		"muti_ic_comb_ratio_para", MULTI_IC_COMB_NAME_PARA_LEVEL,
		MULTI_IC_COMB_NAME_MAX);

	for (i = 0; i < array_len; i++) {
		if (power_dts_read_string_index(power_dts_tag(HWLOG_TAG),
			np, "muti_ic_comb_ratio_para", i, &tmp_string))
			return;

		row = i / MULTI_IC_COMB_NAME_MAX;
		col = i % MULTI_IC_COMB_NAME_MAX;

		switch (col) {
			case MULTI_IC_COMB_MAIN_IC_NAME:
				strncpy(info->comb_name[row].main_ic_name,
					tmp_string, MULTI_IC_COMB_NAME_LEN - 1);
				break;
			case MULTI_IC_COMB_AUX_IC_NAME:
				strncpy(info->comb_name[row].aux_ic_name,
					tmp_string, MULTI_IC_COMB_NAME_LEN - 1);
				break;
			case MULTI_IC_COMB_CURRENT_RATION_NAME:
				strncpy(info->comb_name[row].current_ratio_name,
					tmp_string, MULTI_IC_COMB_NAME_LEN - 1);
				break;
		}
	}
}

static void multi_ic_parse_current_ratio_para(struct device_node *np,
	struct multi_ic_check_para *info)
{
	int row, col, len;
	char *current_ratio_name = NULL;
	int data[MULTI_IC_CURR_RATIO_PARA_LEVEL * MULTI_IC_CURR_RATIO_ERR_MAX] = { 0 };

	current_ratio_name = multi_ic_get_current_ratio_name(info);
	hwlog_info("current_ratio_name:%s\n", current_ratio_name);
	len = power_dts_read_string_array(power_dts_tag(HWLOG_TAG), np,
		current_ratio_name, data, MULTI_IC_CURR_RATIO_PARA_LEVEL,
		MULTI_IC_CURR_RATIO_ERR_MAX);
	if (len < 0)
		return;

	for (row = 0; row < len / MULTI_IC_CURR_RATIO_ERR_MAX; row++) {
		col = row * MULTI_IC_CURR_RATIO_ERR_MAX + MULTI_IC_CURR_RATIO_ERR_CHECK_CNT;
		info->curr_ratio[row].error_cnt = data[col];
		col = row * MULTI_IC_CURR_RATIO_ERR_MAX + MULTI_IC_CURR_RATIO_MIN;
		info->curr_ratio[row].current_ratio_min = data[col];
		col = row * MULTI_IC_CURR_RATIO_ERR_MAX + MULTI_IC_CURR_RATIO_MAX;
		info->curr_ratio[row].current_ratio_max = data[col];
		col = row * MULTI_IC_CURR_RATIO_ERR_MAX + MULTI_IC_CURR_RATIO_DMD_LEVEL;
		info->curr_ratio[row].dmd_level = data[col];
		col = row * MULTI_IC_CURR_RATIO_ERR_MAX + MULTI_IC_CURR_RATIO_LIMIT_CURRENT;
		info->curr_ratio[row].limit_current = data[col];
	}
}

static void multi_ic_parse_vbat_error_para(struct device_node *np,
	struct multi_ic_check_para *info)
{
	int row, col, len;
	int data[MULTI_IC_VBAT_ERROR_PARA_LEVEL * MULTI_IC_VBAT_ERROR_MAX] = { 0 };

	len = power_dts_read_string_array(power_dts_tag(HWLOG_TAG), np,
		"vbat_error", data, MULTI_IC_VBAT_ERROR_PARA_LEVEL,
		MULTI_IC_VBAT_ERROR_MAX);
	if (len < 0)
		return;

	for (row = 0; row < len / MULTI_IC_VBAT_ERROR_MAX; row++) {
		col = row * MULTI_IC_VBAT_ERROR_MAX + MULTI_IC_VBAT_ERROR_CHECK_CNT;
		info->vbat_error[row].error_cnt = data[col];
		col = row * MULTI_IC_VBAT_ERROR_MAX + MULTI_IC_VBAT_ERROR_DELTA;
		info->vbat_error[row].vbat_error = data[col];
		col = row * MULTI_IC_VBAT_ERROR_MAX + MULTI_IC_VBAT_ERROR_DMD_LEVEL;
		info->vbat_error[row].dmd_level = data[col];
		col = row * MULTI_IC_VBAT_ERROR_MAX + MULTI_IC_VBAT_ERROR_LIMIT_CURRENT;
		info->vbat_error[row].limit_current = data[col];
	}
}

static void multi_ic_parse_tbat_error_para(struct device_node *np,
	struct multi_ic_check_para *info)
{
	int row, col, len;
	int data[MULTI_IC_TBAT_ERROR_PARA_LEVEL * MULTI_IC_TBAT_ERROR_MAX] = { 0 };

	len = power_dts_read_string_array(power_dts_tag(HWLOG_TAG), np,
		"tbat_error", data, MULTI_IC_TBAT_ERROR_PARA_LEVEL,
		MULTI_IC_TBAT_ERROR_MAX);
	if (len < 0)
		return;

	for (row = 0; row < len / MULTI_IC_TBAT_ERROR_MAX; row++) {
		col = row * MULTI_IC_TBAT_ERROR_MAX + MULTI_IC_TBAT_ERROR_CHECK_CNT;
		info->tbat_error[row].error_cnt = data[col];
		col = row * MULTI_IC_TBAT_ERROR_MAX + MULTI_IC_TBAT_ERROR_DELTA;
		info->tbat_error[row].tbat_error = data[col];
		col = row * MULTI_IC_TBAT_ERROR_MAX + MULTI_IC_TBAT_ERROR_DMD_LEVEL;
		info->tbat_error[row].dmd_level = data[col];
		col = row * MULTI_IC_TBAT_ERROR_MAX + MULTI_IC_TBAT_ERROR_LIMIT_CURRENT;
		info->tbat_error[row].limit_current = data[col];
	}
}

static void multi_ic_parse_unbalanced_sc_para(struct device_node *np,
	struct multi_ic_check_para *info)
{
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"main_ic_ibus_limit_current_max",
		&info->main_ibus_current_limit_max, MAIN_IC_IBUS_CURRENT_LIMIT_TH);

	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"aux_ic_ibus_limit_current_max",
		&info->aux_ibus_current_limit_max, AUX_IC_IBUS_CURRENT_LIMIT_TH);

	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"ibus_limit_current_delta",
		&info->ibus_limit_current_delta, IBUS_CURRENT_LIMIT_DELTA_TH);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"dc_use_fg_get_battery_current",
		(u32 *)&info->dc_use_fg_get_battery_current, 0);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"support_unbalanced_current_sc",
		(u32 *)&info->support_unbalanced_current_sc, 0);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"single_main_ic_ibat_th", (u32 *)&info->single_main_ic_ibat_th,
		DC_SINGLEIC_MAIN_CURRENT_LIMIT);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"single_aux_ic_ibat_th", (u32 *)&info->single_aux_ic_ibat_th,
		DC_SINGLEIC_AUX_CURRENT_LIMIT);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"multi_ic_info_ibat_th", (u32 *)&info->multi_ic_info_ibat_th,
		MULTI_IC_INFO_IBAT_TH_DEFAULT);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"support_basp_stage_volt_para",
		(u32 *)&info->support_basp_stage_volt_para, 0);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"basp_first_cc_cycle_min",
		(u32 *)&info->basp_first_cc_cycle_min, BASP_FIRST_CC_CYCLE_MIN);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"basp_first_cc_cycle_max",
		(u32 *)&info->basp_first_cc_cycle_max, BASP_FIRST_CC_CYCLE_MAX);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"basp_first_cc_vterm_dec",
		(u32 *)&info->basp_first_cc_vterm_dec, BASP_FIRST_CC_VTER_DEC);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"basp_first_cc_vterm_asc",
		(u32 *)&info->basp_first_cc_vterm_asc, BASP_FIRST_CC_VTER_ASC);
}

static void multi_ic_parse_ibat_comb_para(struct device_node *np,
	struct multi_ic_check_para *info)
{
	int ret;

	ret = power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"ibat_comb", (u32 *)&info->ibat_comb, 0);
	if (ret < 0)
		hwlog_info("use default ibat comb para\n");
}

void multi_ic_parse_check_para(struct device_node *np,
	struct multi_ic_check_para *info)
{
	if (!np || !info)
		return;

	multi_ic_parse_comb_name_para(np, info);
	multi_ic_parse_current_ratio_para(np, info);
	multi_ic_parse_vbat_error_para(np, info);
	multi_ic_parse_tbat_error_para(np, info);
	multi_ic_parse_ibat_comb_para(np, info);
	multi_ic_parse_unbalanced_sc_para(np, info);
}

static int multi_ic_check_get_dmd_num(int dmd_level)
{
	switch (dmd_level) {
	case MULTI_IC_DMD_LEVEL_INFO:
		return DSM_MULTI_CHARGE_CURRENT_RATIO_INFO;
	case MULTI_IC_DMD_LEVEL_WARNING:
		return DSM_MULTI_CHARGE_CURRENT_RATIO_WARNING;
	case MULTI_IC_DMD_LEVEL_ERROR:
		return DSM_MULTI_CHARGE_CURRENT_RATIO_ERROR;
	default:
		return -1;
	}
}

static void multi_ic_dsm_report_dmd(int working_mode, int type, int dmd_level,
	struct multi_ic_check_para *info, const char *buf, int buf_size)
{
	char tmp_buf[MULTI_ERR_STRING_LEN] = { 0 };
	int ibus_main = 0;
	int ibus_aux = 0;
	int tbat_main = 0;
	int tbat_aux = 0;
	int ibus_ratio;
	int dmd_num;
	int bat_curr = 0;
	bool valid = 0;

	if (!buf || (buf_size >= MULTI_ERR_STRING_LEN))
		return;

	if (dmd_level < MULTI_IC_DMD_LEVEL_BEGIN ||
		dmd_level >= MULTI_IC_DMD_LEVEL_END)
		return;

	if (info->report_info[dmd_level])
		return;

	dmd_num = multi_ic_check_get_dmd_num(dmd_level);
	if (dmd_num < 0)
		return;

	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_MAIN, &ibus_main);
	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_AUX, &ibus_aux);

	if (!ibus_aux)
		return;

	ibus_ratio = ibus_main * 100 / ibus_aux; /* multiplied by 100 to calc ratio */
	(void)bat_temp_get_temperature(BTB_TEMP_0, &tbat_main);
	(void)bat_temp_get_temperature(BTB_TEMP_1, &tbat_aux);
	(void)gauge_get_current(&bat_curr);

	snprintf(tmp_buf, sizeof(tmp_buf),
		"%serror_type = %d, Ibus_ratio = %d, capacity = %d, curr = %dmA,"
		"avg_curr = %dmA, Ibus1 = %dmA, Ibus2 = %dmA, batt_volt1 = %dmV, "
		"batt_volt2 = %dmV,temp1 = %d, temp2 = %d, charger_type = %d\n",
		buf, type, ibus_ratio, huawei_battery_capacity(), bat_curr,
		gauge_get_average_current(&valid), ibus_main, ibus_aux,
		dc_get_bat_btb_voltage_with_comp(working_mode, CHARGE_IC_MAIN, info->vbat_comp),
		dc_get_bat_btb_voltage_with_comp(working_mode, CHARGE_IC_AUX, info->vbat_comp),
		tbat_main, tbat_aux, mt_get_charger_type());

	hwlog_info("%s\n", tmp_buf);
	power_dsm_dmd_report(POWER_DSM_BATTERY, dmd_num, tmp_buf);
	info->report_info[dmd_level] = 1; /* report flag */
}

static void multi_ic_check_update_limit_curr(struct multi_ic_check_para *info,
	int limit_current)
{
	if (limit_current != 0 && info->limit_current > limit_current)
		info->limit_current = limit_current;
}

static void multi_ic_check_info(int working_mode, struct multi_ic_check_para *info, int volt_ratio)
{
	int i;
	int ibus_main = 0;
	int ibus_aux = 0;
	int ibus_ratio;
	char tmp_buf[MULTI_ERR_STRING_LEN] = { 0 };
	struct multi_ic_curr_ratio_para *info_para = NULL;

	if (info->report_info[MULTI_IC_DMD_LEVEL_INFO])
		return;

	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_MAIN, &ibus_main);
	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_AUX, &ibus_aux);

	if (!ibus_aux)
		return;

	hwlog_info("ibus_main:%d ibus_aux:%d ibat_th:%d volt_ratio:%d\n",
		ibus_main, ibus_aux, info->ibat_th, volt_ratio);

	if (ibus_main + ibus_aux < info->ibat_th / volt_ratio) {
		info->ratio_result = 0;
		return;
	}

	for (i = 0; i < MULTI_IC_CURR_RATIO_PARA_LEVEL; i++) {
		if (info->curr_ratio[i].dmd_level == MULTI_IC_DMD_LEVEL_INFO &&
			info->curr_ratio[i].current_ratio_max != 0) {
			info_para = &info->curr_ratio[i];
			break;
		}
	}

	if (!info_para)
		return;
	info->ratio_result = 1;
	ibus_ratio = ibus_main * 100 / ibus_aux; /* multiplied by 100 to calc ratio */

	hwlog_info("ibus_ratio:%d current_ratio_max:%d current_ratio_min:%d\n",
		ibus_ratio, info_para->current_ratio_max, info_para->current_ratio_min);
	if (ibus_ratio <= info_para->current_ratio_max * 10 &&
		ibus_ratio >= info_para->current_ratio_min * 10) {
		snprintf(tmp_buf, sizeof(tmp_buf),
			"Ibus_ratio = %d, Ibus1 = %dmA, Ibus2 = %dmA\n",
			ibus_ratio, ibus_main, ibus_aux);
		multi_ic_dsm_report_dmd(working_mode, MULTI_IC_ERROR_TYPE_INFO,
			MULTI_IC_DMD_LEVEL_INFO, info, tmp_buf, strlen(tmp_buf));
	}
}

static void multi_ic_check_ibus(int working_mode, struct multi_ic_check_para *info)
{
	int ibus_main = 0;
	int ibus_aux = 0;
	int ibus_ratio;
	int i;
	char tmp_buf[MULTI_ERR_STRING_LEN] = { 0 };
	u32 cur_time = current_kernel_time().tv_sec;

	if (cur_time - info->multi_ic_start_time < MULTI_IC_CHECK_TIMEOUT)
		return;

	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_MAIN, &ibus_main);
	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_AUX, &ibus_aux);

	if (!ibus_aux)
		return;

	ibus_ratio = ibus_main * 100 / ibus_aux; /* multiplied by 100 to calc ratio */

	for (i = 0; i < MULTI_IC_CURR_RATIO_PARA_LEVEL; i++) {
		if (info->ibus_error_num[i] >= info->curr_ratio[i].error_cnt)
			continue;

		if (ibus_ratio <= info->curr_ratio[i].current_ratio_max * 10 &&
			ibus_ratio >= info->curr_ratio[i].current_ratio_min * 10)
			continue;

		info->ibus_error_num[i]++;
		hwlog_info("check ibus error, ibus_main=%d, ibus_aux=%d,"
			"ibus_ratio=%d, cnt=%d\n", ibus_main, ibus_aux, ibus_ratio,
			info->ibus_error_num[i]);
		if (info->ibus_error_num[i] == info->curr_ratio[i].error_cnt) {
			snprintf(tmp_buf, sizeof(tmp_buf),
				"Ibus_ratio = %d, Ibus1 = %dmA, Ibus2 = %dmA\n",
				ibus_ratio, ibus_main, ibus_aux);
			multi_ic_dsm_report_dmd(working_mode, MULTI_IC_ERROR_TYPE_IBUS,
				info->curr_ratio[i].dmd_level, info, tmp_buf, strlen(tmp_buf));
			multi_ic_check_update_limit_curr(info, info->curr_ratio[i].limit_current);
		}
		return;
	}
}

static void multi_ic_check_vbat(int working_mode, struct multi_ic_check_para *info)
{
	int vbat_main;
	int vbat_aux;
	int delta_volt;
	int i;
	char tmp_buf[MULTI_ERR_STRING_LEN] = { 0 };

	vbat_main = dc_get_bat_btb_voltage_with_comp(working_mode, CHARGE_IC_MAIN, info->vbat_comp);
	vbat_aux = dc_get_bat_btb_voltage_with_comp(working_mode, CHARGE_IC_AUX, info->vbat_comp);
	delta_volt = vbat_main - vbat_aux;
	if (delta_volt < 0)
		delta_volt = -delta_volt;

	for (i = 0; i < MULTI_IC_VBAT_ERROR_PARA_LEVEL; i++) {
		if (info->vbat_error_num[i] >= info->vbat_error[i].error_cnt)
			continue;

		if (delta_volt <= info->vbat_error[i].vbat_error)
			continue;

		info->vbat_error_num[i]++;
		hwlog_info("check vbat_delta error, main_vbat=%d, aux_vbat=%d,"
			"cnt=%d\n", vbat_main, vbat_aux, info->vbat_error_num[i]);
		if (info->vbat_error_num[i] == info->vbat_error[i].error_cnt) {
			snprintf(tmp_buf, sizeof(tmp_buf),
				"vbat_delta = %d, main_vbat = %d, aux_vbat = %d\n",
				delta_volt, vbat_main, vbat_aux);
			multi_ic_dsm_report_dmd(working_mode, MULTI_IC_ERROR_TYPE_VBAT,
				info->vbat_error[i].dmd_level, info, tmp_buf, strlen(tmp_buf));
			multi_ic_check_update_limit_curr(info,
				info->vbat_error[i].limit_current);
		}
		return;
	}
}

static bool multi_ic_check_disable_status(int working_mode, int volt_ratio, struct multi_ic_check_para *info)
{
	int ibus_main = 0;
	int ibus_aux = 0;
	int isys;
	int ibat = 0;

	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_MAIN, &ibus_main);
	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_AUX, &ibus_aux);
	if (info->dc_use_fg_get_battery_current == 1) {
		/* 10: unit convert to 1mA */
		ibat = power_platform_get_battery_current() / 10;
	} else {
	(void)dc_get_bat_current_with_calibration(
		working_mode, CHARGE_IC_MAIN,&ibat, info->ibat_comb);
	}

	isys = (ibus_main + ibus_aux) * volt_ratio - ibat;
	if ((isys > HIGH_POWER_CUR_TH) &&
		(ibus_main < SINGLE_IBUS_MAX) && (ibus_aux < SINGLE_IBUS_MAX)) {
		hwlog_info("high power scenario, skip dmd report\n");
		return true;
	}
	return false;
}

static void multi_get_ic_ibus_avg(int working_mode,
	int *main_ibus, int *aux_ibus)
{
	int ibus_main_temp = 0;
	int ibus_aux_temp = 0;
	int ibus_main_min;
	int ibus_aux_min;
	int ibus_main_max;
	int ibus_aux_max;
	int ibus_main_sum;
	int ibus_aux_sum;
	int i;

	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_MAIN, &ibus_main_temp);
	(void)dc_get_ic_ibus(working_mode, CHARGE_IC_AUX, &ibus_aux_temp);

	ibus_main_min = ibus_main_temp;
	ibus_main_max = ibus_main_temp;
	ibus_main_sum = ibus_main_temp;
	ibus_aux_min = ibus_aux_temp;
	ibus_aux_max = ibus_aux_temp;
	ibus_aux_sum = ibus_aux_temp;

	for (i = 1; i < GET_IBUS_COUNT; i++) {
		usleep_range(1000, 1100); /* delay 1ms */
		(void)dc_get_ic_ibus(working_mode, CHARGE_IC_MAIN, &ibus_main_temp);
		(void)dc_get_ic_ibus(working_mode, CHARGE_IC_AUX, &ibus_aux_temp);

		ibus_main_sum += ibus_main_temp;
		if (ibus_main_min > ibus_main_temp)
			ibus_main_min = ibus_main_temp;
		if (ibus_main_max < ibus_main_temp)
			ibus_main_max = ibus_main_temp;

		ibus_aux_sum += ibus_aux_temp;
		if (ibus_aux_min > ibus_aux_temp)
			ibus_aux_min = ibus_aux_temp;
		if (ibus_aux_max < ibus_aux_temp)
			ibus_aux_max = ibus_aux_temp;
	}
	/* Subtract the maximum and minimum only left with 8 values to average */
	*main_ibus = (ibus_main_sum - ibus_main_min - ibus_main_max) /
		(GET_IBUS_COUNT - 2);
	*aux_ibus = (ibus_aux_sum - ibus_aux_min - ibus_aux_max) /
		(GET_IBUS_COUNT - 2);
}

void multi_ic_reset_get_ibus_fifo(void)
{
	g_sc_para.count = 0;
	g_sc_para.first = false;
	g_sc_para.limit_current_flag = false;
	g_sc_para.ibus_main_sum = 0;
	g_sc_para.ibus_aux_sum = 0;

	memset(g_sc_para.ibus_main_buff, 0,
		sizeof(g_sc_para.ibus_main_buff));
	memset(g_sc_para.ibus_aux_buff, 0,
		sizeof(g_sc_para.ibus_aux_buff));
	hwlog_info("multi_ic_reset_get_ibus_fifo successful\n");
}

static void multi_ic_check_ibus_abs(int working_mode,
	struct multi_ic_check_para *info)
{
	int ibus_main;
	int ibus_main_avg;
	int ibus_aux;
	int ibus_aux_avg;
	int i;

	multi_get_ic_ibus_avg(working_mode, &ibus_main, &ibus_aux);

	if (g_sc_para.first == false) {
		multi_ic_reset_get_ibus_fifo();
		g_sc_para.first = true;
	}

	g_sc_para.count = g_sc_para.count % GET_IBUS_FIFO_LEN;

	g_sc_para.ibus_main_sum -= g_sc_para.ibus_main_buff[g_sc_para.count];
	g_sc_para.ibus_main_buff[g_sc_para.count] = ibus_main;
	g_sc_para.ibus_main_sum += g_sc_para.ibus_main_buff[g_sc_para.count];
	ibus_main_avg = g_sc_para.ibus_main_sum / GET_IBUS_FIFO_LEN;

	g_sc_para.ibus_aux_sum -= g_sc_para.ibus_aux_buff[g_sc_para.count];
	g_sc_para.ibus_aux_buff[g_sc_para.count] = ibus_aux;
	g_sc_para.ibus_aux_sum += g_sc_para.ibus_aux_buff[g_sc_para.count];
	ibus_aux_avg = g_sc_para.ibus_aux_sum / GET_IBUS_FIFO_LEN;

	g_sc_para.count++;

	if ((g_sc_para.limit_current_flag == false) &&
		((ibus_main_avg > info->main_ibus_current_limit_max) ||
		(ibus_aux_avg > info->aux_ibus_current_limit_max))) {
		g_sc_para.limit_current_flag = true;
		info->limit_current = (ibus_main_avg + ibus_aux_avg -
			info->ibus_limit_current_delta) * 2; /* ratio is 2:1 */

		hwlog_info("ibus_main_avg:%d ibus_aux_avg:%d set limit_current:%d\n",
			ibus_main_avg, ibus_aux_avg, info->limit_current);
	}
}

int mulit_ic_check(int working_mode, int mode, struct multi_ic_check_para *info, int volt_ratio)
{
	u32 cur_time = current_kernel_time().tv_sec;
	int vbat_main;
	int vbat_aux;

	if (!info)
		return -1;

	if (mode != CHARGE_MULTI_IC)
		return 0;

	if (multi_ic_check_disable_status(working_mode, volt_ratio, info))
		return 0;
	vbat_main = dc_get_bat_btb_voltage_with_comp(working_mode, CHARGE_IC_MAIN, info->vbat_comp);
	vbat_aux = dc_get_bat_btb_voltage_with_comp(working_mode, CHARGE_IC_AUX, info->vbat_comp);

	/* 1st check vbat is in normal stage */
	if (vbat_main < MULTI_IC_CHECK_VBAT_LOW_TH ||
		vbat_main > MULTI_IC_CHEKC_VBAT_HIGH_TH ||
		vbat_aux < MULTI_IC_CHECK_VBAT_LOW_TH ||
		vbat_aux > MULTI_IC_CHEKC_VBAT_HIGH_TH) {
		hwlog_err("vbat is over limit, vbat_main=%d, vbat_aux=%d\n",
			vbat_main, vbat_aux);
		return -1;
	}

	if (info->support_unbalanced_current_sc == 1)
		multi_ic_check_ibus_abs(working_mode, info);

	if (cur_time - info->multi_ic_start_time < MULTI_IC_CHECK_TIMEOUT)
		return 0;

	multi_ic_check_info(working_mode, info, volt_ratio);
	multi_ic_check_ibus(working_mode, info);
	multi_ic_check_vbat(working_mode, info);

	if (info->limit_current < 0)
		return -1;

	return 0;
}

static void multi_check_btb_result(int type, struct multi_ic_check_mode_para *para)
{
	u32 result = 0;

	btb_ck_get_result_now(type, &result);
	hwlog_info("btb check begin, type is %d\n", type);
	if (result == 0)
		return;
	if (result & MAIN_BAT_BTB_ERR) {
		para->ic_error_cnt[CHARGE_IC_TYPE_MAIN] = MULTI_IC_CHECK_ERR_CNT_MAX;
		hwlog_info("main btb check fail, can not direct charge\n");
	}
	if (result & AUX_BAT_BTB_ERR) {
		para->ic_error_cnt[CHARGE_IC_TYPE_AUX] = MULTI_IC_CHECK_ERR_CNT_MAX;
		hwlog_info("aux btb check fail\n");
	}
	if (result & BTB_BAT_DIFF_ERR) {
		para->ic_error_cnt[CHARGE_IC_TYPE_AUX] = MULTI_IC_CHECK_ERR_CNT_MAX;
		hwlog_info("btb diff check fail\n");
	}
}

int multi_ic_check_select_tbat_id(struct multi_ic_check_mode_para *para)
{
	int temp_result = 0;

	if (!para || !para->support_multi_ic || !para->support_select_temp)
		return BAT_TEMP_MIXED;

	btb_ck_get_result_now(BTB_TEMP_CHECK, &temp_result);
	switch (temp_result) {
	case MAIN_BAT_BTB_ERR:
		return BTB_TEMP_1;
	case AUX_BAT_BTB_ERR:
		return BTB_TEMP_0;
	default:
		break;
	}

	return BAT_TEMP_MIXED;
}

int multi_ic_check_select_init_mode(struct multi_ic_check_mode_para *para, int *mode)
{
	u32 i;

	if (!para || !mode)
		return -1;

	if (!para->support_multi_ic) {
		*mode = CHARGE_IC_MAIN;
		return 0;
	}

	/* 1st: check btb result */
	multi_check_btb_result(BTB_VOLT_CHECK, para);
	/* 2nd: check ic error cnt */
	for (i = 0; i < CHARGE_IC_TYPE_MAX; i++) {
		if (para->ic_error_cnt[i] < MULTI_IC_CHECK_ERR_CNT_MAX) {
			*mode = BIT(i);
			return 0;
		}
	}

	hwlog_info("all ic is error, can not enter direct charge\n");
	return -1;
}

void multi_ic_check_set_ic_error_flag(int flag, struct multi_ic_check_mode_para *para)
{
	if (!para)
		return;

	switch (flag) {
	case CHARGE_IC_MAIN:
		if (para->ic_error_cnt[CHARGE_IC_TYPE_MAIN] < MULTI_IC_CHECK_ERR_CNT_MAX)
			para->ic_error_cnt[CHARGE_IC_TYPE_MAIN]++;
		break;
	case CHARGE_IC_AUX:
		if (para->ic_error_cnt[CHARGE_IC_TYPE_AUX] < MULTI_IC_CHECK_ERR_CNT_MAX)
			para->ic_error_cnt[CHARGE_IC_TYPE_AUX]++;
		break;
	default:
		break;
	}

	hwlog_info("[err_flag] main=%d, aux=%d\n", para->ic_error_cnt[CHARGE_IC_TYPE_MAIN],
		para->ic_error_cnt[CHARGE_IC_TYPE_AUX]);
}

int multi_ic_check_ic_status(struct multi_ic_check_mode_para *para)
{
	int i;

	if (!para)
		return -1;

	for (i = 0; i < CHARGE_IC_TYPE_MAX; i++) {
		if (para->ic_error_cnt[i] >= MULTI_IC_CHECK_ERR_CNT_MAX)
			return -1;
	}

	return 0;
}
