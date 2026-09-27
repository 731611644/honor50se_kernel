/*
 * cps4035_dts.c
 *
 * cps4035 dts driver
 *
 * Copyright (c) 2021-2021 Hihonor Technologies Co., Ltd.
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

#include "cps4035.h"

#define HWLOG_TAG wireless_cps4035_dts
HWLOG_REGIST();

static void cps4035_parse_tx_fod(struct device_node *np,
	struct cps4035_dev_info *di)
{
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"tx_ploss_th0", (u32 *)&di->tx_fod.ploss_th0,
		CPS4035_TX_PLOSS_TH0_VAL);
	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"tx_ploss_cnt", (u32 *)&di->tx_fod.ploss_cnt,
		CPS4035_TX_PLOSS_CNT_VAL);
}

static int cps4035_parse_rx_fod(struct device_node *np,
	struct cps4035_dev_info *di)
{
	return 0;
}

static int cps4035_parse_ldo_cfg(struct device_node *np,
	struct cps4035_dev_info *di)
{
	return 0;
}

int cps4035_parse_dts(struct device_node *np, struct cps4035_dev_info *di)
{
	int ret;

	(void)power_dts_read_u32(power_dts_tag(HWLOG_TAG), np,
		"gpio_en_valid_val", (u32 *)&di->gpio_en_valid_val, 0);

	cps4035_parse_tx_fod(np, di);
	ret = cps4035_parse_rx_fod(np, di);
	if (ret) {
		hwlog_err("parse_dts: parse rx_fod para failed\n");
		return ret;
	}
	ret = cps4035_parse_ldo_cfg(np, di);
	if (ret) {
		hwlog_err("parse_dts: parse ldo cfg failed\n");
		return ret;
	}

	return 0;
}
