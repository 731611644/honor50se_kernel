/*
 * SX932x Driver
 * Copyright (c) 2021 Semtech Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define DRIVER_NAME "sx932x"
#define DRIVER_NAME1 "sx932x1"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
//#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/sort.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <securec.h>

#include "hf_manager.h"
#include "sx932x.h" 	/* main struct, interrupt,init,pointers */



#define IDLE			0
#define ACTIVE			1

/* Failer Index */
#define SX932x_ID_ERROR 	1
#define SX932x_NIRQ_ERROR	2
#define SX932x_CONN_ERROR	3
#define SX932x_I2C_ERROR	4

#define FAR_CALIBRATE 0
#define NEAR_CALIBRATE 1
#define FAC_CALI_FAILED -1
#define FAC_CALI_SUCCESS 0


/*! \struct sx932x
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct sx932x
{
	struct hf_device hf_dev;
	psx932x_platform_data_t hw;		/* specific platform data settings */
	//pbuttonInformation_t pbuttonInformation;
} sx932x_t, *psx932x_t;

static struct sensor_info support_sensors[] = {
	{
		.sensor_type = SENSOR_TYPE_CAP_PROX_PRIVATE,
		.gain = 1,
		.name = {'c','a','p','_','p','r','o','x'},
		.vendor = {'s','e','m','t','e','c','h'},
	},
	{
		.sensor_type = SENSOR_TYPE_CAP_PROX1_PRIVATE,
		.gain = 1,
		.name = {'c','a','p','_','p','r','o','x','1'},
		.vendor = {'s','e','m','t','e','c','h'},
	},
};

static struct class capsense_class[] = {
	{
		.name  = "capsensor",
		.owner = THIS_MODULE,
	},
	{
		.name  = "capsensor1",
		.owner = THIS_MODULE,
	},
};

static  psx93XX_t psx9323_ptr = NULL;
static  psx93XX_t psx9323_ptr1 = NULL;


/*! \fn static int write_register(psx93XX_t this, u8 address, u8 value)
 * \brief Sends a write register to the device
 * \param this Pointer to main parent struct
 * \param address 8-bit register address
 * \param value   8-bit register value to write to address
 * \return Value from i2c_master_send
 */
static int write_register(psx93XX_t this, u8 address, u8 value)
{
	struct i2c_client *i2c = 0;
	char buffer[2];
	int returnValue = 0;

	buffer[0] = address;
	buffer[1] = value;
	returnValue = -ENOMEM;

	if (this && this->bus) {
		i2c = this->bus;
		returnValue = i2c_master_send(i2c,buffer,2);
		#ifdef DEBUG
		dev_info(&i2c->dev,"write_register Address: 0x%x Value: 0x%x Return: %d\n",
														address,value,returnValue);
		#endif
	}
	return returnValue;
}

/*! \fn static int read_register(psx93XX_t this, u8 address, u8 *value)
* \brief Reads a register's value from the device
* \param this Pointer to main parent struct
* \param address 8-Bit address to read from
* \param value Pointer to 8-bit value to save register value to
* \return Value from i2c_smbus_read_byte_data if < 0. else 0
*/
static int read_register(psx93XX_t this, u8 address, u8 *value)
{
	struct i2c_client *i2c = 0;
	s32 returnValue = 0;

	if (this && value && this->bus) {
		i2c = this->bus;
		returnValue = i2c_smbus_read_byte_data(i2c,address);
		#ifdef DEBUG
		dev_info(&i2c->dev, "read_register Address: 0x%x Return: 0x%x\n",
														address,returnValue);
		#endif

		if (returnValue >= 0) {
			*value = returnValue;
			return 0;
		}
		else {
			return returnValue;
		}
	}
	return -ENOMEM;
}

//static int sx932x_set_mode(psx93XX_t this, unsigned char mode);

/*! \fn static int read_regStat(psx93XX_t this)
 * \brief Shortcut to read what caused interrupt.
 * \details This is to keep the drivers a unified
 * function that will read whatever register(s)
 * provide information on why the interrupt was caused.
 * \param this Pointer to main parent struct
 * \return If successful, Value of bit(s) that cause interrupt, else 0
 */
static int read_regStat(psx93XX_t this)
{
	u8 data = 0;
	if (this) {
		if (read_register(this,SX932x_IRQSTAT_REG,&data) == 0)
		return (data & 0x00FF);
	}
	return 0;
}

/*********************************************************************/
/*! \brief Perform a manual offset calibration
* \param this Pointer to main parent struct
* \return Value return value from the write register
 */
static int manual_offset_calibration(psx93XX_t this)
{
	s32 returnValue = 0;
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	returnValue = write_register(this,SX932x_STAT2_REG,0x0F);
	return returnValue;
}
/*! \brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static ssize_t manual_offset_calibration_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	u8 reg_value = 0;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	dev_info(this->pdev, "Reading IRQSTAT_REG\n");
	read_register(this,SX932x_IRQSTAT_REG,&reg_value);
	return sprintf(buf, "%d\n", reg_value);
}

/*! \brief sysfs store function for manual calibration
 */
static ssize_t manual_offset_calibration_store(struct device *dev,
			struct device_attribute *attr,const char *buf, size_t count)
{
	unsigned long val;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;
	if (val) {
		dev_info( this->pdev, "Performing manual_offset_calibration()\n");
		manual_offset_calibration(this);
	}
	return count;
}

static int sx932x_Hardware_Check(psx93XX_t this)
{
	int ret;
	u8 failcode = 0;
	u8 loop = 0;
	this->failStatusCode = 0;

	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	//Check th IRQ Status
	//while(this->get_nirq_low && this->get_nirq_low()){
	//	read_regStat(this);
	//	msleep(100);
	//	if(++loop >10){
	//		this->failStatusCode = SX932x_NIRQ_ERROR;
	//		break;
	//	}
	//}

	//Check I2C Connection
	ret = read_register(this, SX932x_WHOAMI_REG, &failcode);
	if(ret < 0){
		this->failStatusCode = SX932x_I2C_ERROR;
	}

	if(failcode != SX932x_WHOAMI_VALUE){
		this->failStatusCode = SX932x_ID_ERROR;
	}

	dev_info(this->pdev, "sx932x failcode = 0x%x 0x%x\n",this->failStatusCode, failcode);
	return (int)this->failStatusCode;
}

/*********************************************************************/
static int sx932x_global_variable_init(psx93XX_t this)
{
	this->irq_disabled = 0;
	this->failStatusCode = 0;
	this->reg_in_dts = true;
	/* factory calibration initialization */
	this->fac_diff_far = 0;
	this->fac_offset_far = 0;
	this->fac_diff_near = 0;
	this->fac_offset_near = 0;
	this->fac_diff_far1 = 0;
	this->fac_offset_far1 = 0;
	this->fac_diff_near1 = 0;
	this->fac_offset_near1 = 0;
	this->fac_cali_result = FAC_CALI_FAILED;
	return 0;
}

static ssize_t sx932x_register_write_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int reg_address = 0, val = 0;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
		pr_err("[SX932x]: %s - The number of data are wrong\n",__func__);
		return -EINVAL;
	}

	write_register(this, (unsigned char)reg_address, (unsigned char)val);
	pr_info("[SX932x]: %s - Register(0x%x) data(0x%x)\n",__func__, reg_address, val);

	return count;
}

//read registers not include the advanced one
static ssize_t sx932x_register_read_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	return sprintf(buf, "0x%02x \n", this->register_read_store_val);
}

static ssize_t sx932x_register_read_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val=0;
	int regist = 0;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	dev_info(this->pdev, "Reading register\n");

	if (sscanf(buf, "%x", &regist) != 1) {
		pr_err("[SX932x]: %s - The number of data are wrong\n",__func__);
		return -EINVAL;
	}

	read_register(this, regist, &val);
	pr_info("[SX932x]: %s - Register(0x%2x) data(0x%2x)\n",__func__, regist, val);
	this->register_read_store_val = (unsigned int)val;
	return count;
}

static void read_rawData(psx93XX_t this)
{
	u8 msb=0, lsb=0;
	u8 csx;

	if(this){
		for(csx =0; csx<4; csx++){
			write_register(this,SX932x_CPSRD,csx);//here to check the CS1, also can read other channel
			read_register(this,SX932x_USEMSB,&msb);
			read_register(this,SX932x_USELSB,&lsb);
			this->rawdata_phase[csx].useful = (s32)((msb << 8) | lsb);

			read_register(this,SX932x_AVGMSB,&msb);
			read_register(this,SX932x_AVGLSB,&lsb);
			this->rawdata_phase[csx].average = (s32)((msb << 8) | lsb);

			read_register(this,SX932x_DIFFMSB,&msb);
			read_register(this,SX932x_DIFFLSB,&lsb);
			this->rawdata_phase[csx].diff = (s32)((msb << 8) | lsb);

			read_register(this,SX932x_OFFSETMSB,&msb);
			read_register(this,SX932x_OFFSETLSB,&lsb);
			this->rawdata_phase[csx].offset = (u16)((msb << 8) | lsb);
			if (this->rawdata_phase[csx].useful > 32767)
				this->rawdata_phase[csx].useful -= 65536;
			if (this->rawdata_phase[csx].average > 32767)
				this->rawdata_phase[csx].average -= 65536;
			if (this->rawdata_phase[csx].diff > 32767)
				this->rawdata_phase[csx].diff -= 65536;
			dev_info(this->pdev, " [CS: %d] Useful = %d Average = %d, DIFF = %d Offset = %d \n",
				csx,this->rawdata_phase[csx].useful,this->rawdata_phase[csx].average,this->rawdata_phase[csx].diff,this->rawdata_phase[csx].offset);
		}
	}

}

static ssize_t sx932x_raw_data_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	int ret;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	read_rawData(this);
	ret = sprintf(buf, "0,%d,%d,%d,%d,1,%d,%d,%d,%d,2,%d,%d,%d,%d,3,%d,%d,%d,%d \n",
                        //"1,%d,%d,%d,%d,"
                        //"2,%d,%d,%d,%d,"
                        //"3,%d,%d,%d,%d \n",
                        this->rawdata_phase[0].useful,
                        this->rawdata_phase[0].average,
                        this->rawdata_phase[0].diff,
                        this->rawdata_phase[0].offset,

                        this->rawdata_phase[1].useful,
                        this->rawdata_phase[1].average,
                        this->rawdata_phase[1].diff,
                        this->rawdata_phase[1].offset,

                        this->rawdata_phase[2].useful,
                        this->rawdata_phase[2].average,
                        this->rawdata_phase[2].diff,
                        this->rawdata_phase[2].offset,

                        this->rawdata_phase[3].useful,
                        this->rawdata_phase[3].average,
                        this->rawdata_phase[3].diff,
                        this->rawdata_phase[3].offset
                   );
	return ret;
}

/* show far near status reg data */
static ssize_t sx932x_status_regdata_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val0,val1,val2,val3;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	read_register(this,SX932x_STAT0_REG,&val0);
	read_register(this,SX932x_STAT1_REG,&val1);
	read_register(this,SX932x_STAT2_REG,&val2);
	read_register(this,SX932x_STAT3_REG,&val3);

	pr_info("[SX932x]: %s - Status0=0x%2x, Status1=0x%2x,Status2=0x%2x,Status3=0x%2x \n",__func__, val0,val1,val2,val3);

	return sprintf(buf, "%d,%d,%d,%d \n", val0,val1,val2,val3);
}

/* read far near status */
static ssize_t sx932x_status_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	int status = 0;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	read_register(this,SX932x_STAT0_REG,&val);
	pr_info("[SX932x]: %s - Register(0x01) data(0x%2x)\n",__func__, val);
	if(val == 0)
	{
		status = 0;
	}else{

		status = 1;
	}
	return sprintf(buf, "%d\n", status);
}

/* check if manual calibrate success or not */
static ssize_t sx932x_cal_state_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	int status = 0;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	read_register(this,SX932x_CTRL1_REG,&val);
	pr_info("[SX932x]: %s - Register(SX932x_CTRL1_REG 0x11) data(0x%2x)\n",__func__, val);
	if(val == 0x23)
	{
		status = 1;
	}else{

		status = 0;
	}
	return sprintf(buf, "%d\n", status);
}

static ssize_t sx932x_enable_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	int status = 0;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	read_register(this,SX932x_CTRL1_REG,&val);
	pr_info("[SX932x]: %s - Register(SX932x_CTRL1_REG 0x11) data(0x%2x)\n",__func__, val);
	if(val == 0x27)
	{
		status = 1;
	}else{

		status = 0;
	}
	return sprintf(buf, "%d\n", status);
}

static ssize_t sx932x_enable_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val=0;
	int enable = 1;
	psx93XX_t this = NULL;
	if(!dev || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	dev_info(this->pdev, "Reading register\n");

	if (sscanf(buf, "%x", &enable) != 1) {
		pr_err("[SX932x]: %s - The number of data are wrong\n",__func__);
		return -EINVAL;
	}

	if(enable == 0)
	{
		pr_info("[SX932x]: disabled!\n");
		write_register(this,SX932x_CTRL1_REG,0x20);
	}else{
		pr_info("[SX932x]: enable!\n");
		write_register(this,SX932x_CTRL1_REG,0x27);
	}
	read_register(this, SX932x_CTRL1_REG, &val);
	pr_info("[SX932x]: %s - Register(0x%2x) data(0x%2x)\n",__func__, SX932x_CTRL1_REG, val);

	return count;
}

static DEVICE_ATTR(manual_calibrate, 0664, manual_offset_calibration_show,manual_offset_calibration_store);
static DEVICE_ATTR(register_write,  0664, NULL,sx932x_register_write_store);
static DEVICE_ATTR(register_read,0664, sx932x_register_read_show,sx932x_register_read_store);
static DEVICE_ATTR(raw_data,0664,sx932x_raw_data_show,NULL);
static DEVICE_ATTR(regproxdata,0664,sx932x_status_regdata_show,NULL);
static DEVICE_ATTR(proxstatus,0664,sx932x_status_show,NULL);
static DEVICE_ATTR(cal_state,0664,sx932x_cal_state_show,NULL);
static DEVICE_ATTR(enable,0664,sx932x_enable_show,sx932x_enable_store);
static struct attribute *sx932x_attributes[] = {
	&dev_attr_manual_calibrate.attr,
	&dev_attr_register_write.attr,
	&dev_attr_register_read.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_regproxdata.attr,
	&dev_attr_proxstatus.attr,
	&dev_attr_cal_state.attr,
	&dev_attr_enable.attr,
	NULL,
};
static struct attribute_group sx932x_attr_group = {
	.attrs = sx932x_attributes,
};

static ssize_t calibration_info_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int len = 0;
	psx93XX_t this= NULL;
	char *pbuf = buf;

	if(!class || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(class->name, capsense_class[0].name)) {
			this = psx9323_ptr;
	} else {
			this = psx9323_ptr1;
	}
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	len = snprintf_s(pbuf, PAGE_SIZE, PAGE_SIZE - 1, "ch0 far:%d,%d near:%d,%d\n",
			this->fac_diff_far, this->fac_offset_far,
			this->fac_diff_near, this->fac_offset_near);

	len += snprintf_s(pbuf + len, PAGE_SIZE - len, PAGE_SIZE - len -  1, "ch1 far:%d,%d near:%d,%d\n",
			this->fac_diff_far1, this->fac_offset_far1,
			this->fac_diff_near1, this->fac_offset_near1);

	pr_info("%s, ch0 far:%d,%d near:%d,%d\n", __func__,
			this->fac_diff_far, this->fac_offset_far,
			this->fac_diff_near, this->fac_offset_near);

	pr_info("%s, ch1 far:%d,%d near:%d,%d\n", __func__,
			this->fac_diff_far1, this->fac_offset_far1,
			this->fac_diff_near1, this->fac_offset_near1);
	return len;

}
static CLASS_ATTR_RO(calibration_info);

static ssize_t calibrate_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int len = 0;
	psx93XX_t this= NULL;

	if(!class || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(class->name, capsense_class[0].name)) {
			this = psx9323_ptr;
	} else {
			this = psx9323_ptr1;
	}
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	len = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%d",
			this->fac_cali_result);
	pr_info("%s,fac calibrate result:%d\n", __func__,
			this->fac_cali_result);

	return len;
}

static ssize_t calibrate_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	psx93XX_t this= NULL;
	unsigned long val = 0;
	int ret = 0;

	if(!class || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(class->name, capsense_class[0].name)) {
			this = psx9323_ptr;
	} else {
			this = psx9323_ptr1;
	}
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	if (kstrtoul(buf, 0, &val))
		return -1;

	if (val == FAR_CALIBRATE) {
		ret = manual_offset_calibration(this);
		read_rawData(this);
		this->fac_diff_far = this->rawdata_phase[0].diff;
		this->fac_offset_far = this->rawdata_phase[0].offset;
		this->fac_diff_far1 = this->rawdata_phase[1].diff;
		this->fac_offset_far1 = this->rawdata_phase[1].offset;
		this->fac_cali_result = FAC_CALI_SUCCESS;
		pr_info("%s,Far calibrate result:%d\n", __func__,
				this->fac_cali_result);
	} else if (val == NEAR_CALIBRATE) {
		read_rawData(this);
		this->fac_diff_near = this->rawdata_phase[0].diff;
		this->fac_offset_near = this->rawdata_phase[0].offset;
		this->fac_diff_near1 = this->rawdata_phase[1].diff;
		this->fac_offset_near1 = this->rawdata_phase[1].offset;
		this->fac_cali_result = FAC_CALI_SUCCESS;
		pr_info("%s,Near calibrate result:%d\n", __func__,
				this->fac_cali_result);
	} else {
		this->fac_cali_result = FAC_CALI_FAILED;
		pr_err("%s: Invalid operatio,calibrate result:%d\n", __func__,
				this->fac_cali_result);
		return count;
	}

	pr_info("%s,Exit fac calibrate\n", __func__);

	return count;
}
static CLASS_ATTR_RW(calibrate);

static ssize_t sar_sensor_detect_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int len = 0;
	int ret = 0;
	psx93XX_t this= NULL;

	if(!class || !attr || !buf){
		pr_err("%s: invalid pointer to parameter\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(class->name, capsense_class[0].name)) {
			this = psx9323_ptr;
	} else {
			this = psx9323_ptr1;
	}
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	ret = sx932x_Hardware_Check(this);
	if (ret) {
		pr_info("%s: sar detect fail\n", __func__);
		len = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%d\n", 0);
	} else {
		pr_info("%s: sar detect succ\n", __func__);
		len = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%d\n", 1);
	}
	return len;
}
static CLASS_ATTR_RO(sar_sensor_detect);

static struct attribute *sx9323_attributes[] = {
	&class_attr_calibration_info.attr,
	&class_attr_calibrate.attr,
	&class_attr_sar_sensor_detect.attr,
	NULL,
};
static struct attribute_group sx9323_attr_group = {
	.attrs = sx9323_attributes,
};

static void sx932x_class_create_attr(psx93XX_t this)
{
	int ret;
	psx932x_t pDevice = NULL;
	struct class *cap_prox_class;
	pr_info("Enter %s\n", __func__);

	cap_prox_class = kzalloc(sizeof(*cap_prox_class), GFP_KERNEL);
	if (cap_prox_class == NULL) {
		pr_err("create cap prox class failed!\n");
		return;
	}

	if(this && (pDevice = this->pDevice))
	{
		/* for accessing items in user data (e.g. calibrate) */
		ret = sysfs_create_group(&this->pdev->kobj, &sx932x_attr_group);
		if (ret < 0) {
			pr_err("%s: Create device attr group failed %d\n", __func__, ret);
			return;
		}
		ret = sysfs_create_group(&this->pdev->kobj, &sx9323_attr_group);
		if (ret < 0) {
			pr_err("%s: Create class attr group failed %d\n", __func__, ret);
			return;
		}
		if (pDevice->hw->sensor_type == SENSOR_TYPE_CAP_PROX_PRIVATE) {
			cap_prox_class = &capsense_class[0];
		} else if(pDevice->hw->sensor_type == SENSOR_TYPE_CAP_PROX1_PRIVATE){
			cap_prox_class = &capsense_class[1];
		}
		ret = class_register(cap_prox_class);
		if (ret < 0) {
			pr_err("%s: Create fsys class failed %d\n", __func__, ret);
			goto err_remove_attr_group;
		}
		ret = class_create_file(cap_prox_class, &class_attr_calibrate);
		if (ret < 0) {
			pr_err("%s: Create  fac calibrate failed %d\n", __func__, ret);
			goto err_remove_calibrate;
		}

		ret = class_create_file(cap_prox_class, &class_attr_calibration_info);
		if (ret < 0) {
			pr_err("%s: Create fac calibration info failed %d\n", __func__, ret);
			goto err_remove_calibration_info;
		}

		ret = class_create_file(cap_prox_class, &class_attr_sar_sensor_detect);
		if (ret < 0) {
			pr_err("%s: Create fac sar detect failed %d\n", __func__, ret);
			goto err_remove_sar_sensor_detect;
		}
	}

	return;

err_remove_sar_sensor_detect:
	class_remove_file(cap_prox_class, &class_attr_sar_sensor_detect);
err_remove_calibration_info:
	class_remove_file(cap_prox_class, &class_attr_calibration_info);
err_remove_calibrate:
	class_remove_file(cap_prox_class, &class_attr_calibrate);
err_remove_cap_class:
	class_unregister(cap_prox_class);
err_remove_attr_group:
	sysfs_remove_group(&this->pdev->kobj, &sx9323_attr_group);
}

/****************************************************/
/*! \brief  Initialize I2C config from platform data
 * \param this Pointer to main parent struct
 */
static void sx932x_reg_init(psx93XX_t this)
{
	psx932x_t pDevice = 0;
	psx932x_platform_data_t pdata = 0;
	int i = 0;
	/* configure device */
	dev_info(this->pdev, "Going to Setup I2C Registers\n");
	if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
	{
		/*******************************************************************************/
		// try to initialize from device tree!
		/*******************************************************************************/
		if (this->reg_in_dts == true) {
			while ( i < pdata->i2c_reg_num) {
				/* Write all registers/values contained in i2c_reg */
				dev_info(this->pdev, "Going to Write Reg from dts: 0x%x Value: 0x%x\n",
				pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
				write_register(this, pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
				i++;
			}
		} else { // use static ones!!
			while ( i < ARRAY_SIZE(sx932x_i2c_reg_setup)) {
				/* Write all registers/values contained in i2c_reg */
				dev_info(this->pdev, "Going to Write Reg: 0x%x Value: 0x%x\n",
				sx932x_i2c_reg_setup[i].reg,sx932x_i2c_reg_setup[i].val);
				write_register(this, sx932x_i2c_reg_setup[i].reg,sx932x_i2c_reg_setup[i].val);
				i++;
			}
		}
	/*******************************************************************************/
	} else {
		dev_err(this->pdev, "ERROR! platform data 0x%p\n",pDevice->hw);
	}

}


/*! \fn static int initialize(psx93XX_t this)
 * \brief Performs all initialization needed to configure the device
 * \param this Pointer to main parent struct
 * \return Last used command's return value (negative if error)
 */
static int initialize(psx93XX_t this)
{
	int ret;
	if (this) {
		pr_info("SX932x income initialize\n");
		/* prepare reset by disabling any irq handling */
		this->irq_disabled = 1;
		disable_irq(this->irq);
		/* perform a reset */
		write_register(this,SX932x_SOFTRESET_REG,SX932x_SOFTRESET);
		/* wait until the reset has finished by monitoring NIRQ */
		dev_info(this->pdev, "Sent Software Reset. Waiting until device is back from reset to continue.\n");
		/* just sleep for awhile instead of using a loop with reading irq status */
		msleep(100);
		ret = sx932x_global_variable_init(this);

		sx932x_reg_init(this);
		msleep(100); /* make sure everything is running */
		manual_offset_calibration(this);

		/* re-enable interrupt handling */
		enable_irq(this->irq);

		/* make sure no interrupts are pending since enabling irq will only
		* work on next falling edge */
		read_regStat(this);
		return 0;
	}
	return -ENOMEM;
}

static int sx932x_parse_dt(struct sx932x_platform_data *pdata, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;
	int ret;
	if (dNode == NULL)
		return -ENODEV;

	pdata->irq_gpio= of_get_named_gpio_flags(dNode,
											"Semtech,nirq-gpio", 0, &flags);
	if (pdata->irq_gpio < 0) {
		pr_err("[SENSOR]: %s - get irq_gpio error\n", __func__);
		return -ENODEV;
	}

	/***********************************************************************/
	// load in registers from device tree
	of_property_read_u32(dNode,"Semtech,reg-num",&pdata->i2c_reg_num);
	// layout is register, value, register, value....
	// if an extra item is after just ignore it. reading the array in will cause it to fail anyway
	pr_info("[SX932x]:%s -  size of elements %d \n", __func__,pdata->i2c_reg_num);
	if (pdata->i2c_reg_num > 0) {
		 // initialize platform reg data array
		 pdata->pi2c_reg = devm_kzalloc(dev,sizeof(struct smtc_reg_data)*pdata->i2c_reg_num, GFP_KERNEL);
		 if (unlikely(pdata->pi2c_reg == NULL)) {
			return -ENOMEM;
		}

	 // initialize the array
		if (of_property_read_u8_array(dNode,"Semtech,reg-init",(u8*)&(pdata->pi2c_reg[0]),sizeof(struct smtc_reg_data)*pdata->i2c_reg_num))
		return -ENOMEM;
	}
	/***********************************************************************/
	if(of_property_read_u32(dNode, "sensor_type", &pdata->sensor_type))
		pr_err("read sensor_type fail\n");
	pr_info("%s sensor_type = %d\n", __func__, pdata->sensor_type);

	pdata->cap_vdd = regulator_get(dev, "cap_vdd");
	if (IS_ERR(pdata->cap_vdd)) {
		if (PTR_ERR(pdata->cap_vdd) == -EPROBE_DEFER) {
			ret = PTR_ERR(pdata->cap_vdd);
			return ret;
		}
		pr_err("%s: Failed to get regulator\n", __func__);
	} else {
		ret = regulator_enable(pdata->cap_vdd);

		if (ret) {
			regulator_put(pdata->cap_vdd);
			pr_err("%s: Error %d enable regulator\n",
				__func__, ret);
			return ret;
		}
		pdata->cap_vdd_en = true;
		pr_info("cap_vdd regulator is %s\n",
		regulator_is_enabled(pdata->cap_vdd) ? "on" : "off");
	}

	pr_info("[SX932x]: %s -[%d] parse_dt complete\n", __func__,pdata->irq_gpio);
	return 0;
}

/* get the NIRQ state (1->NIRQ-low, 0->NIRQ-high) */
static int sx932x_init_platform_hw(struct i2c_client *client)
{
	psx93XX_t this = i2c_get_clientdata(client);
	struct sx932x *pDevice = NULL;
	struct sx932x_platform_data *pdata = NULL;

	int rc;

	pr_info("[SX932x] : %s init_platform_hw start!",__func__);

	if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw)) {
		if (gpio_is_valid(pdata->irq_gpio)) {
			rc = gpio_request(pdata->irq_gpio, "sx932x_irq_gpio");
			if (rc < 0) {
				dev_err(this->pdev, "SX932x Request gpio. Fail![%d]\n", rc);
				return rc;
			}
			rc = gpio_direction_input(pdata->irq_gpio);
			if (rc < 0) {
				dev_err(this->pdev, "SX932x Set gpio direction. Fail![%d]\n", rc);
				return rc;
			}
			this->irq = client->irq = gpio_to_irq(pdata->irq_gpio);
		}
		else {
			dev_err(this->pdev, "SX932x Invalid irq gpio num.(init)\n");
		}
	}
	else {
		pr_err("[SX932x] : %s - Do not init platform HW", __func__);
	}

	pr_err("[SX932x]: %s - sx932x_irq_debug\n",__func__);
	return rc;
}

static void sx932x_exit_platform_hw(struct i2c_client *client)
{
	psx93XX_t this = i2c_get_clientdata(client);
	struct sx932x *pDevice = NULL;
	struct sx932x_platform_data *pdata = NULL;

	if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw)) {
		if (gpio_is_valid(pdata->irq_gpio)) {
			gpio_free(pdata->irq_gpio);
		}
		else {
			dev_err(this->pdev, "Invalid irq gpio num.(exit)\n");
		}
	}
	return;
}

static int sx932x_get_nirq_state(int gpio_num)
{
	return  !gpio_get_value(gpio_num);
}

/*! \fn static int sx932x_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * \brief Probe function
 * \param client pointer to i2c_client
 * \param id pointer to i2c_device_id
 * \return Whether probe was successful
 */


static int sx932x_enable(struct hf_device *hfdev, int sensor_type, int en)
{
	int err = 0;
	psx93XX_t this = NULL;

	if(!hfdev){
		pr_err("%s: hfdev is NULL\n", __func__);
		return -EINVAL;
	}

	this = hf_device_get_private_data(hfdev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}

	if(en == 1){
		write_register(this,SX932x_CTRL1_REG,0x27);
	}else{
		write_register(this,SX932x_CTRL1_REG,0x20);
	}

	pr_debug("%s id:%d en:%d\n", __func__, sensor_type, en);

	return err;
}

static int sx932x_batch(struct hf_device *hfdev, int sensor_type,
		int64_t delay, int64_t latency)
{
	pr_debug("%s id:%d delay:%lld latency:%lld\n", __func__, sensor_type,
			delay, latency);
	return 0;
}

static int sx932x_rawdata(struct hf_device *hfdev, int sensor_type, int en)
{
	psx93XX_t this = NULL;
	if(!hfdev){
		pr_err("%s: hfdev is NULL\n", __func__);
		return -EINVAL;
	}

	this = hf_device_get_private_data(hfdev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	if(en == 1)
	{
		read_rawData(this);
	}
	return 0;
}

static void sx932x_sample(psx93XX_t this)//struct hf_device *hfdev)
{
	u8 data[3];
	psx932x_t pDevice = this->pDevice;

	struct hf_manager *manager = pDevice->hf_dev.manager;
	struct hf_manager_event event;

	if (this == NULL || pDevice == NULL) {
		pr_err("[SX932x]: %s - sx932x_sample\n",__func__);
		return;
	}

	read_register(this,SX932x_IRQSTAT_REG,&data[0]);
	read_register(this,SX932x_STAT0_REG,&data[1]);
	read_register(this,SX932x_STAT1_REG,&data[2]);

	memset(&event, 0, sizeof(struct hf_manager_event));

	event.timestamp = ktime_get_boot_ns();
	event.sensor_type = pDevice->hw->sensor_type;//SENSOR_TYPE_SAR;
	event.accurancy = SENSOR_ACCURANCY_HIGH;
	event.action = DATA_ACTION;
	if (pDevice->hw->sensor_type == SENSOR_TYPE_CAP_PROX_PRIVATE) {
		event.word[0] = data[0];
		event.word[1] = data[1];
		event.word[2] = data[2];
	} else if (pDevice->hw->sensor_type == SENSOR_TYPE_CAP_PROX1_PRIVATE){
		event.word[0] = data[0];
		event.word[1] = data[1];
		event.word[2] = data[2];
	}
	manager->report(manager, &event);
	//manager->complete(manager);

	pr_info("[SX932x]: %s type:%d, IRQ:%d, STAT0:%d, STAT1:%d\n",
		__func__, event.sensor_type, data[0], data[1], data[2]);
	return;
}

static int sx932x_calibration(struct hf_device *hfdev, int sensor_type)
{
	psx93XX_t this = NULL;
	if(!hfdev){
		pr_err("%s: hfdev is NULL\n", __func__);
		return -EINVAL;
	}

	this = hf_device_get_private_data(hfdev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return -EINVAL;
	}
	manual_offset_calibration(this);
	return 0;
}

static int sx932x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	int ret = 0;

	psx93XX_t this = 0;
	psx932x_t pDevice = 0;
	psx932x_platform_data_t pplatData = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	dev_info(&client->dev, "sx932x_probe()\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)) {
		dev_err(&client->dev, "Check i2c functionality.Fail!\n");
		err = -EIO;
		return err;
	}

	this = devm_kzalloc(&client->dev,sizeof(sx93XX_t), GFP_KERNEL); /* create memory for main struct */
	dev_info(&client->dev, "\t Initialized Main Memory: 0x%p\n",this);

	pplatData = devm_kzalloc(&client->dev,sizeof(struct sx932x_platform_data), GFP_KERNEL);
	if (!pplatData) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}
	pplatData->get_is_nirq_low = sx932x_get_nirq_state;

	err = sx932x_parse_dt(pplatData, &client->dev);
	if (err) {
		dev_err(&client->dev, "could not setup pin\n");
		goto parse_dt_fail;
	}
	pplatData->init_platform_hw = sx932x_init_platform_hw;
	client->dev.platform_data = pplatData;

	dev_err(&client->dev, "SX932x init_platform_hw done!\n");

	if (this){
		dev_info(&client->dev, "SX932x initialize start!!");
		/* In case we need to reinitialize data
		* (e.q. if suspend reset device) */
		this->init = initialize;
		/* shortcut to read status of interrupt */
		this->refreshStatus = read_regStat;
		/* pointer to function from platform data to get pendown
		* (1->NIRQ=0, 0->NIRQ=1) */
		this->get_nirq_low = pplatData->get_is_nirq_low;
		/* save irq in case we need to reference it */
		this->irq = client->irq;
		/* do we need to create an irq timer after interrupt ? */
		this->useIrqTimer = 0;

		/* Setup function to call on corresponding reg irq source bit */
		if (MAX_NUM_STATUS_BITS>= 8)
		{
			this->statusFunc[0] = 0; /* TXEN_STAT */
			this->statusFunc[1] = 0; /* UNUSED */
			this->statusFunc[2] = 0; /* UNUSED */
			this->statusFunc[3] = 0; //read_rawData; /* CONV_STAT */
			this->statusFunc[4] = 0; /* COMP_STAT */
			this->statusFunc[5] = sx932x_sample; /* RELEASE_STAT */
			this->statusFunc[6] = sx932x_sample; /* TOUCH_STAT  */
			this->statusFunc[7] = 0; /* RESET_STAT */
		}
		/* setup i2c communication */
		this->bus = client;
		i2c_set_clientdata(client, this);

		/* record device struct */
		this->pdev = &client->dev;

		ret = sx932x_Hardware_Check(this);
		if (ret) {
			dev_err(this->pdev,"sx932x hardware check failed!\n");
			goto parse_dt_fail;
		}
		/* create memory for device specific struct */
		pDevice = devm_kzalloc(&client->dev,sizeof(sx932x_t), GFP_KERNEL);
		dev_info(&client->dev, "\t Initialized Device Specific Memory: 0x%p\n",pDevice);
		if (pDevice){
			this->pDevice = pDevice;

			/* Add Pointer to main platform data struct */
			pDevice->hw = pplatData;

			sx932x_class_create_attr(this);

			/* Check if we hava a platform initialization function to call*/
			if (pplatData->init_platform_hw)
			pplatData->init_platform_hw(client);

			if(pDevice->hw->sensor_type == SENSOR_TYPE_CAP_PROX_PRIVATE){
				psx9323_ptr = this;
				pDevice->hf_dev.dev_name = DRIVER_NAME;
				pDevice->hf_dev.support_list = &support_sensors[0];
			}else if(pDevice->hw->sensor_type == SENSOR_TYPE_CAP_PROX1_PRIVATE){
				psx9323_ptr1 = this;
				pDevice->hf_dev.dev_name = DRIVER_NAME1;
				pDevice->hf_dev.support_list = &support_sensors[1];
			}

			//pDevice->hf_dev.support_size = ARRAY_SIZE(support_sensors);
			pDevice->hf_dev.support_size = 1;
			pDevice->hf_dev.device_poll = HF_DEVICE_IO_POLLING;
			pDevice->hf_dev.device_bus = HF_DEVICE_IO_ASYNC;
			pDevice->hf_dev.enable = sx932x_enable;
			pDevice->hf_dev.batch = sx932x_batch;
			//pDevice->hf_dev.sample = sx932x_sample;
			pDevice->hf_dev.rawdata = sx932x_rawdata;
			pDevice->hf_dev.calibration = sx932x_calibration;

			/* transfer i2c_dev to hf_dev */
			hf_device_set_private_data(&pDevice->hf_dev, this);

			err = hf_manager_create(&pDevice->hf_dev);
			if (err < 0) {
				pr_err("%s hf_manager_create fail\n", __func__);
				goto create_manager_fail;
			}
		}else{
			dev_err(this->pdev,"pDevice malloc failed\n");
			return -ENOMEM;
		}

		sx93XX_IRQ_init(this);
		/* call init function pointer (this should initialize all registers */
		if (this->init){
			this->init(this);
		}else{
			dev_err(this->pdev,"No init function!!!!\n");
			return -ENOMEM;
		}
	}else{
		return -1;
	}

	pplatData->exit_platform_hw = sx932x_exit_platform_hw;

	dev_info(&client->dev, "sx932x_probe() Done\n");

	return 0;

create_manager_fail:
	if(pDevice){
		pr_err("create_manager_fail\n");
		devm_kfree(&client->dev, pDevice);
		pDevice = NULL;
	}
parse_dt_fail:
	if(this || pplatData){
		pr_err("parse_dt_fail\n");
		devm_kfree(&client->dev, pplatData);
		devm_kfree(&client->dev, this);
		pplatData = NULL;
		this = NULL;
		i2c_set_clientdata(client, this);
	}
	sysfs_remove_group(&client->dev.kobj, &sx932x_attr_group);

	return -ENOMEM;
}

static int sx932x_remove(struct i2c_client *client)
{
	int ret = 0;
	psx932x_platform_data_t pplatData =0;
	psx932x_t pDevice = 0;
	psx93XX_t this = i2c_get_clientdata(client);
	if (this && (pDevice = this->pDevice))
	{
		sysfs_remove_group(&client->dev.kobj, &sx932x_attr_group);
		pplatData = client->dev.platform_data;
		if (pplatData && pplatData->exit_platform_hw)
			pplatData->exit_platform_hw(client);
		devm_kfree(&client->dev, this->pDevice);
	}

	if (pplatData->cap_vdd_en) {
		ret = regulator_disable(pplatData->cap_vdd);
		if (ret) {
			regulator_put(pplatData->cap_vdd);
			pr_err("%s: Error %d disable regulator\n",
				__func__, ret);
		}
	}
	hf_manager_destroy(pDevice->hf_dev.manager);

	//devm_kfree(&client->dev, driver_dev);
	return sx93XX_remove(this);
}

static int sx932x_suspend(struct device *dev)
{
	psx93XX_t this = NULL;
	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return 0;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return 0;
	}
	pr_info("%s\n", __func__);
	write_register(this,SX932x_CTRL1_REG,0x20);//make sx932x in Sleep mode
	return 0;
}

static int sx932x_resume(struct device *dev)
{
	psx93XX_t this = NULL;
	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return 0;
	}
	this = dev_get_drvdata(dev);
	if(!this){
		pr_err("%s: psx93XX_t is NULL\n", __func__);
		return 0;
	}
	pr_info("%s\n", __func__);
	write_register(this,SX932x_CTRL1_REG,0x27);//resume from sleep
	return 0;
}


static struct i2c_device_id sx932x_idtable[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sx932x_idtable);

static struct i2c_device_id sx932x_idtable1[] = {
	{ DRIVER_NAME1, 1 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sx932x_idtable1);

static struct of_device_id sx932x_match_table[] = {
	{ .compatible = "Semtech,sx932x",},
	{ .compatible = "Semtech,sx932x1",},
	{ },
};

MODULE_DEVICE_TABLE(of, sx932x_match_table);

static const struct dev_pm_ops sx932x_pm_ops = {
	.suspend = sx932x_suspend,
	.resume = sx932x_resume,
};
static struct i2c_driver sx932x_driver = {
	.driver = {
		.owner			= THIS_MODULE,
		.name			= DRIVER_NAME,
		//.of_match_table	= sx932x_match_table,
		.pm				= &sx932x_pm_ops,
	},
	.id_table		= sx932x_idtable,
	.probe			= sx932x_probe,
	.remove			= sx932x_remove,
};

static struct i2c_driver sx932x_driver1 = {
	.driver = {
		.owner			= THIS_MODULE,
		.name			= DRIVER_NAME1,
		//.of_match_table	= sx932x_match_table,
		.pm				= &sx932x_pm_ops,
	},
	.id_table		= sx932x_idtable1,
	.probe			= sx932x_probe,
	.remove			= sx932x_remove,
};
static int __init sx932x_I2C_init(void)
{
	pr_err("%s\n",__func__);
	return i2c_add_driver(&sx932x_driver);
}
static void __exit sx932x_I2C_exit(void)
{
	i2c_del_driver(&sx932x_driver);
}


static int __init sx932x1_I2C_init(void)
{
	pr_err("%s\n",__func__);
	return i2c_add_driver(&sx932x_driver1);
}
static void __exit sx932x1_I2C_exit(void)
{
	i2c_del_driver(&sx932x_driver1);
}

module_init(sx932x_I2C_init);
module_init(sx932x1_I2C_init);
module_exit(sx932x_I2C_exit);
module_exit(sx932x1_I2C_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX932x Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

static void sx93XX_schedule_work(psx93XX_t this, unsigned long delay)
{
	unsigned long flags;
	if (this) {
		dev_info(this->pdev, "sx93XX_schedule_work()\n");
		spin_lock_irqsave(&this->lock,flags);
		/* Stop any pending penup queues */
		cancel_delayed_work(&this->dworker);
		//after waiting for a delay, this put the job in the kernel-global workqueue. so no need to create new thread in work queue.
		schedule_delayed_work(&this->dworker,delay);
		spin_unlock_irqrestore(&this->lock,flags);
	}
	else
		printk(KERN_ERR "sx93XX_schedule_work, NULL psx93XX_t\n");
}

static irqreturn_t sx93XX_irq(int irq, void *pvoid)
{
	psx93XX_t this = 0;
	psx932x_t pDevice = 0;
	int irq_num = 0;

	if (pvoid) {
		this = (psx93XX_t)pvoid;
		pDevice = this->pDevice;
		irq_num = pDevice->hw->irq_gpio;

		if ((!this->get_nirq_low) || this->get_nirq_low(irq_num)) {
			sx93XX_schedule_work(this,0);
		}
		else{
			dev_err(this->pdev, "sx93XX_irq - nirq read high\n");
		}
		pr_info("%s, type:%d, irq_num:%d\n",
			__func__, pDevice->hw->sensor_type, irq_num);
	}
	else{
		printk(KERN_ERR "sx93XX_irq, NULL pvoid\n");
	}
	return IRQ_HANDLED;
}

static void sx93XX_worker_func(struct work_struct *work)
{
	psx93XX_t this = 0;
	psx932x_t pDevice = 0;
	int status = 0;
	int counter = 0;
	int irq_num = 0;
	u8 nirqLow = 0;
	if (work) {
		this = container_of(work,sx93XX_t,dworker.work);
		pDevice = this->pDevice;
		irq_num = pDevice->hw->irq_gpio;

		if (!this) {
			printk(KERN_ERR "sx93XX_worker_func, NULL sx93XX_t\n");
			return;
		}
		if (unlikely(this->useIrqTimer)) {
			if ((!this->get_nirq_low) || this->get_nirq_low(irq_num)) {
				nirqLow = 1;
			}
		}
		/* since we are not in an interrupt don't need to disable irq. */
		status = this->refreshStatus(this);
		counter = -1;
		dev_dbg(this->pdev, "Worker - Refresh Status %d\n",status);

		while((++counter) < MAX_NUM_STATUS_BITS) { /* counter start from MSB */
			if (((status>>counter) & 0x01) && (this->statusFunc[counter])) {
				dev_info(this->pdev, "SX932x Function Pointer Found. Calling\n");
				this->statusFunc[counter](this);
			}
		}
		if (unlikely(this->useIrqTimer && nirqLow))
		{	/* Early models and if RATE=0 for newer models require a penup timer */
			/* Queue up the function again for checking on penup */
			sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
		}
	}else{
		printk(KERN_ERR "sx93XX_worker_func, NULL work_struct\n");
	}
}

int sx93XX_remove(psx93XX_t this)
{
	struct i2c_client *client = 0;
	if (this) {
		client = this->bus;
		cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
		/*destroy_workqueue(this->workq); */
		free_irq(this->irq, this);
		devm_kfree(&client->dev, this);
		return 0;
	}
	return -ENOMEM;
}
/*void sx93XX_suspend(psx93XX_t this)
{
	if (this)
		disable_irq(this->irq);

	write_register(this,SX932x_CTRL1_REG,0x20);//make sx932x in Sleep mode
}
void sx93XX_resume(psx93XX_t this)
{
	if (this) {
		sx93XX_schedule_work(this,0);
		//if (this->init)
			//this->init(this);
	enable_irq(this->irq);
	}
	write_register(this,SX932x_CTRL1_REG,0x27);//resume from sleep, need to modify based on number of channel.
}*/

int sx93XX_IRQ_init(psx93XX_t this)
{
	int err = 0;
	if (this && this->pDevice)
	{
		/* initialize spin lock */
		spin_lock_init(&this->lock);
		/* initialize worker function */
		INIT_DELAYED_WORK(&this->dworker, sx93XX_worker_func);
		/* initailize interrupt reporting */
		this->irq_disabled = 0;
		err = request_irq(this->irq, sx93XX_irq, IRQF_TRIGGER_FALLING,
							this->pdev->driver->name, this);
		if (err) {
			dev_err(this->pdev, "irq %d busy?\n", this->irq);
			return err;
		}
		dev_info(this->pdev, "registered with irq (%d)\n", this->irq);
	}
	return -ENOMEM;
}


