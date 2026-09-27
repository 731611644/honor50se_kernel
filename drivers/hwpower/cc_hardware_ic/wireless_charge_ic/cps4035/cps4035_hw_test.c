/*
 * cps4035_hw_test.c
 *
 * cps4035 hardware test driver
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

#define HWLOG_TAG wireless_cps4035_hw_test
HWLOG_REGIST();

static int cps4035_hw_test_pwr_good_gpio(void)
{
	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_ON);
	power_usleep(DT_USLEEP_10MS);
	if (!cps4035_is_pwr_good()) {
		hwlog_err("pwr_good_gpio: failed\n");
		wlps_control(WLPS_TX_SW, WLPS_CTRL_OFF);
		return -EINVAL;
	}
	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_OFF);

	hwlog_info("[pwr_good_gpio] succ\n");
	return 0;
};

static int cps4035_hw_test_reverse_cp(int type)
{
	return 0;
}

static struct wireless_hw_test_ops g_cps4035_hw_test_ops = {
	.chk_pwr_good_gpio = cps4035_hw_test_pwr_good_gpio,
	.chk_reverse_cp_prepare = cps4035_hw_test_reverse_cp,
};

int cps4035_hw_test_ops_register(void)
{
	return wireless_hw_test_ops_register(&g_cps4035_hw_test_ops);
};
