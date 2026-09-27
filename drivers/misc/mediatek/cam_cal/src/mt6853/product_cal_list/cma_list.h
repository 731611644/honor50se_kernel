/*
 * cma_cal_list.h
 *
 * Copyright (c) 2020-2021 Huawei Technologies Co., Ltd.
 *
 * cma_cal_list
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

#ifndef _CMA_CAL_LIST_H_
#define _CMA_CAL_LIST_H_

static struct eeprom_hw_i2c_reg cma_main_eeprom_map[] = {
	{ 0x0d76, EEPROM_I2C_WORD_ADDR, 3343,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map cma_main_eeprom_map_table = {
	.map = cma_main_eeprom_map,
	.map_size = ARRAY_SIZE(cma_main_eeprom_map),
};
static struct eeprom_hw_i2c_reg cma_main_eeprom_map_JslAndHlt[] = {
	{ 0x0d76, EEPROM_I2C_WORD_ADDR, 3311,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map cma_main_eeprom_map_table_JslAndHlt = {
	.map = cma_main_eeprom_map_JslAndHlt,
	.map_size = ARRAY_SIZE(cma_main_eeprom_map),
};
static struct eeprom_hw_i2c_reg cma_sub_eeprom_map[] = {
	{ 0x0ac0, EEPROM_I2C_WORD_ADDR, 1919,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map cma_sub_eeprom_map_table = {
	.map = cma_sub_eeprom_map,
	.map_size = ARRAY_SIZE(cma_sub_eeprom_map),
};

struct stCAM_CAL_LIST_STRUCT cma_camCalList[] = {
	{ // main
		.sensorID = C9922QCP_M100_CAR_SENSOR_ID,
		.slaveID = 0xA0,
		.maxEepromSize = 0xCEF,
		.eeprom_hw_map = &cma_main_eeprom_map_table_JslAndHlt,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C9922UAI_M330_CAR_SENSOR_ID,
		.slaveID = 0xA0,
		.maxEepromSize = 0xCEF,
		.eeprom_hw_map = &cma_main_eeprom_map_table_JslAndHlt,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C9922XBA_M340_CAR_SENSOR_ID,
		.slaveID = 0xA0,
		.maxEepromSize = 0xD0F,
		.eeprom_hw_map = &cma_main_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	}, { // sub
		.sensorID = C9920HQF_M330_CAR_SENSOR_ID,
		.slaveID = 0xA0,
		.eeprom_hw_map = &cma_sub_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C9920KEH_M190_CAR_SENSOR_ID,
		.slaveID = 0xA0,
		.eeprom_hw_map = &cma_sub_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C9920KEH_M180_CAR_SENSOR_ID,
		.slaveID = 0xA0,
		.eeprom_hw_map = &cma_sub_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	},
	// ADD before this line
	{ 0, 0, 0, 0 } // end of list
};

#endif // _CMA_CAL_LIST_H_
