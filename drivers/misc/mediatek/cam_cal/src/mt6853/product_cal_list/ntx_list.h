/*
 * ntx_list.h
 *
 * Copyright (c) 2021-2021 Honor Technologies Co., Ltd.
 *
 * ntx_list
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

#ifndef _NTX_LIST_H_
#define _NTX_LIST_H_

// total: 9684(0x25D4)
static struct eeprom_hw_i2c_reg ntx_main_eeprom_map[] = {
	{ 0x00, EEPROM_I2C_WORD_ADDR, 38,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
	{ 0x0AEE, EEPROM_I2C_WORD_ADDR, 42,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
	{ 0x17E7, EEPROM_I2C_WORD_ADDR, 4007,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
	{ 0x282E, EEPROM_I2C_WORD_ADDR, 5597,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map ntx_main_eeprom_map_table = {
	.map = ntx_main_eeprom_map,
	.map_size = ARRAY_SIZE(ntx_main_eeprom_map),
};


// total: 4439(0x1157)
static struct eeprom_hw_i2c_reg ntx_front_eeprom_map[] = {
	{ 0x00, EEPROM_I2C_WORD_ADDR, 39,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
	{ 0x258a, EEPROM_I2C_WORD_ADDR, 4400,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map ntx_front_eeprom_map_table = {
	.map = ntx_front_eeprom_map,
	.map_size = ARRAY_SIZE(ntx_front_eeprom_map),
};


// total: 1939(0x0793)
static struct eeprom_hw_i2c_reg ntx_micro_eeprom_map_698[] = {
	{ 0x00, EEPROM_I2C_WORD_ADDR, 39,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
	{ 0xfa0, EEPROM_I2C_WORD_ADDR, 1900,
		EEPROM_I2C_BYTE_DATA, EEPROM_I2C_READ, 0x00 },
};

static struct eeprom_hw_map ntx_micro_eeprom_map_table_698 = {
	.map = ntx_micro_eeprom_map_698,
	.map_size = ARRAY_SIZE(ntx_micro_eeprom_map_698),
};

struct stCAM_CAL_LIST_STRUCT ntx_camCalList[] = {
	{ // main-64m
		.sensorID = C633KES_M060_NTX_SENSOR_ID,
		.slaveID = 0xA0,
		.maxEepromSize = 0x25D4,
		.eeprom_hw_map = &ntx_main_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C633KES_M020_NTX_SENSOR_ID,
		.slaveID = 0xA0,
		.maxEepromSize = 0x25D4,
		.eeprom_hw_map = &ntx_main_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	}, { // front
		.sensorID = C658OGU_M010_NTX_SENSOR_ID,
		.slaveID = 0xA0,
		.eeprom_hw_map = &ntx_front_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C658FUV_M060_NTX_SENSOR_ID,
		.slaveID = 0xA0,
		.eeprom_hw_map = &ntx_front_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C658JDI_M020_NTX_SENSOR_ID,
		.slaveID = 0xA0,
		.eeprom_hw_map = &ntx_front_eeprom_map_table,
		.getCamCalData = eeprom_hw_get_data,
	}, { // macro
		.sensorID = C698YGA_M010_NTX_SENSOR_ID,
		.slaveID = 0xAC,
		.eeprom_hw_map = &ntx_micro_eeprom_map_table_698,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C698EOY_M0B0_NTX_SENSOR_ID,
		.slaveID = 0xAC,
		.eeprom_hw_map = &ntx_micro_eeprom_map_table_698,
		.getCamCalData = eeprom_hw_get_data,
	}, {
		.sensorID = C698EOY_M020_NTX_SENSOR_ID,
		.slaveID = 0xAC,
		.eeprom_hw_map = &ntx_micro_eeprom_map_table_698,
		.getCamCalData = eeprom_hw_get_data,
	},
	// ADD before this line
	{ 0, 0, 0, 0 } // end of list
};

#endif // _NTX_LIST_H_
