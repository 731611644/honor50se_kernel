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


#ifndef __SENSORS_DMD_H__
#define __SENSORS_DMD_H__

#define PS_BIG_DATA_EVENT_ID    936005006

typedef struct {
	const char *param_name;
	int event_cnt;
} big_data_param_detail_t;

int sensors_big_data_report(uint32_t event_id, uint8_t *detail);

#endif
