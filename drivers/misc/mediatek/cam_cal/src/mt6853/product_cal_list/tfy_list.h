/*
 * tfy_cal_list.h
 *
 * Copyright (c) 2021-2021 Honor Technologies Co., Ltd.
 *
 * tfy_cal_list
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

#ifndef _TFY_CAL_LIST_H_
#define _TFY_CAL_LIST_H_

// total: 9418(0x24CA)
static struct eeprom_hw_i2c_reg tfy_main_eeprom_map_845[] = {
	{ 0x00, EEPROM_I2C_WORD_ADDR, 77,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
	{ 0x0CAA, EEPROM_I2C_WORD_ADDR, 5355,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
	{ 0x306D, EEPROM_I2C_WORD_ADDR, 3986,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map tfy_main_eeprom_map_table_845 = {
	.map = tfy_main_eeprom_map_845,
	.map_size = ARRAY_SIZE(tfy_main_eeprom_map_845),
};

// total: 4988(0x137c)
static struct eeprom_hw_i2c_reg tfy_sub_eeprom_map_651[] = {
	{ 0x00, EEPROM_I2C_WORD_ADDR, 4988,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map tfy_sub_eeprom_map_table_651 = {
	.map = tfy_sub_eeprom_map_651,
	.map_size = ARRAY_SIZE(tfy_sub_eeprom_map_651),
};

// total: 1939(0x0793)
static struct eeprom_hw_i2c_reg tfy_micro_eeprom_map_827[] = {
	{ 0x00, EEPROM_I2C_WORD_ADDR, 39,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
	{ 0xfa0, EEPROM_I2C_WORD_ADDR, 1900,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map tfy_micro_eeprom_map_table_827 = {
	.map = tfy_micro_eeprom_map_827,
	.map_size = ARRAY_SIZE(tfy_micro_eeprom_map_827),
};

struct stCAM_CAL_LIST_STRUCT tfy_camCalList[] = {
	{ // main-48m
		.sensorID = C845DBC_M010_TFY_SENSOR_ID,
		.slaveID = 0xA0,
		.maxEepromSize = 0x24CA,
		.eeprom_hw_map = &tfy_main_eeprom_map_table_845,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C845DBC_M060_TFY_SENSOR_ID,
		.slaveID = 0xA0,
		.maxEepromSize = 0x24CA,
		.eeprom_hw_map = &tfy_main_eeprom_map_table_845,
		.getCamCalData = eeprom_hw_get_data,
	}, { // sub
		.sensorID = C651JDI_M030_TFY_SENSOR_ID,
		.slaveID = 0xA0,
		.eeprom_hw_map = &tfy_sub_eeprom_map_table_651,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C651FUV_M090_TFY_SENSOR_ID,
		.slaveID = 0xA0,
		.eeprom_hw_map = &tfy_sub_eeprom_map_table_651,
		.getCamCalData = eeprom_hw_get_data,
	}, { // macro
		.sensorID = C827YGA_M010_TFY_SENSOR_ID,
		.slaveID = 0xAC,
		.eeprom_hw_map = &tfy_micro_eeprom_map_table_827,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C827RWP_M020_TFY_SENSOR_ID,
		.slaveID = 0xAC,
		.eeprom_hw_map = &tfy_micro_eeprom_map_table_827,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C827RWP_M0B0_TFY_SENSOR_ID,
		.slaveID = 0xAC,
		.eeprom_hw_map = &tfy_micro_eeprom_map_table_827,
		.getCamCalData = eeprom_hw_get_data,
	},
	// ADD before this line
	{ 0, 0, 0, 0 } // end of list
};

#endif // _TFY_CAL_LIST_H_

