/*
 * Copyright (c) Honor Technologies Co., Ltd. 2021-2029. All rights reserved.
 * Team:    Sensor
 * Date:    2021.06.05
 * Description: sensor dmd module
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <log/hwlog_kernel.h>
#include <log/imonitor.h>
#include "sensors_dmd.h"

static int isspace(int c)
{
	return c == ' ' || (unsigned)c-'\t' < 5;
}

static  int isdigit(int c)
{
	return (unsigned)c-'0' < 10;
}

static big_data_param_detail_t event_ps_sound_param[] = {
	{ "Num_Call1", 0 },
	{ "Num_Call2", 0 },
	{ "Num_Call3", 0 },
	{ "Num_Call4", 0 },
	{ "Num_Entering1", 0 },
	{ "Num_Entering2", 0 },
	{ "Num_Entering3", 0 },
	{ "Num_Entering4", 0 },
	{ "Num_Leaving1", 0 },
	{ "Num_Leaving2", 0 },
	{ "Num_Leaving3", 0 },
	{ "Num_Leaving4", 0 },
	{ "Num_Within1", 0 },
	{ "Num_Within2", 0 },
	{ "Num_Within3", 0 },
	{ "Num_Within4", 0 }
};

static int atoi(char *s)
 {
 	int n=0, neg=0;
 	while (isspace(*s)) s++;
 	switch (*s) {
 	case '-': neg=1;
 	case '+': s++;
 	}
 	/* Compute n as a negative number to avoid overflow on INT_MIN */
 	while (isdigit(*s))
 		n = 10*n - (*s++ - '0');
 	return neg ? n : -n;
 }

//1:1:0:0:
int sensor_big_data_parse(char *context)
{
	char detail[32] = { 0 };
	int report_cnt[4] = { 0 };
	int i = 0;
	int j = 0;
	int k = 0;
	int event_index = 0;

	if (context == NULL) {
		pr_err("%s: para error\n", __func__);
		return -1;
	}

	pr_info("%s: context %s, len %d\n", __func__, context, strlen(context));
	for (i; i < strlen(context) - 6; i++) {
		if (context[i] != ':') {
			detail[j++] = context[i];
		} else if (context[i] == ':') {
			report_cnt[k] = atoi(detail);
			memset(detail, 0, sizeof(detail));
			j = 0;
			k++;
		}
	}

	event_ps_sound_param[6].event_cnt = report_cnt[0];
	event_ps_sound_param[7].event_cnt = report_cnt[1];
	event_ps_sound_param[10].event_cnt = report_cnt[2];
	event_ps_sound_param[11].event_cnt = report_cnt[3];

	return 0;
}

int sensors_big_data_report(uint32_t event_id, uint8_t *detail)
{
	struct imonitor_eventobj *obj = NULL;
	int ret = 0;
	int i;

	pr_info("%s enter\n", __func__);
	if (detail == NULL) {
		pr_err("%s para error\n", __func__);
		return -1;
	}

	sensor_big_data_parse(detail);

	obj = imonitor_create_eventobj(event_id);
	if (!obj) {
		pr_err("%s imonitor_create_eventobj failed\n", __func__);
		return -1;
	}

	for (i = 0; i < sizeof(event_ps_sound_param) / sizeof(event_ps_sound_param[0]); i++) {
		ret += imonitor_set_param_integer_v2(obj, event_ps_sound_param[i].param_name, 
			event_ps_sound_param[i].event_cnt);
		pr_info("%s: param_name %s, event count %d\n",	__func__, 
			event_ps_sound_param[i].param_name, event_ps_sound_param[i].event_cnt);
	}
	if (ret) {
		imonitor_destroy_eventobj(obj);
		pr_err("%s imonitor_set_para fail, ret %d\n", __func__, ret);
		return ret;
	}

	ret = imonitor_send_event(obj);
	if (ret < 0)
		pr_err("%s imonitor_send_event fail, ret %d\n", __func__, ret);

	imonitor_destroy_eventobj(obj);
	pr_info("%s event id: %d\n", __func__, event_id);
	return ret;
}

EXPORT_SYMBOL(sensors_big_data_report);


