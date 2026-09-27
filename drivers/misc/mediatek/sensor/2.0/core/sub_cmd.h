/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: als channel header file
 * Author: wangsiwen
 * Create: 2020-12-15
 */
#ifndef __SUB_CMD_H__
#define __SUB_CMD_H__

enum sub_cmd_t {
	/* als */
	SUB_CMD_SET_ALS_PA = 0x20,
	SUB_CMD_UPDATE_BL_LEVEL,
	SUB_CMD_UPDATE_RGB_DATA,
	SUB_CMD_GET_FACTORY_PARA,
	SUB_CMD_CHANGE_DC_STATUS,
	SUB_CMD_CHANGE_ALWAYS_ON_STATUS,
	SUB_CMD_UPDATE_NOISE_DATA,
	SUB_CMD_ALS_LCD_FREQ_REQ,
	SUB_CMD_SET_ALS_UD_CALIB_DATA,
	SUB_CMD_UPDATE_AOD_STATUS,
};

#endif

