/*
 * cps4035.c
 *
 * cps4035 driver
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
#include <securec.h>
#include "cps4035.h"

#define HWLOG_TAG wireless_cps4035
HWLOG_REGIST();

#define CPS_PRINTF_BUF_SIZE 32

static char g_cps_printf_buf[CPS_PRINTF_BUF_SIZE];
static struct cps4035_dev_info *g_cps4035_di;
static struct wakeup_source g_cps4035_wakelock;

void cps4035_get_dev_info(struct cps4035_dev_info **di)
{
	if (!g_cps4035_di || !di)
		return;

	*di = g_cps4035_di;
}

bool cps4035_is_pwr_good(void)
{
	int gpio_val;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di)
		return false;

	if (!di->g_val.mtp_chk_complete)
		return true;

	return true;
}

int cps4035_iic_ram_unlock(void)
{
	int ret = 0;
	ret = cps4035_aux_write_word(CPS4035_CMD_UNLOCK_I2C, CPS4035_I2C_CODE);
	ret += cps4035_aux_write_word(CPS4035_CMD_INC_MODE, CPS4035_BYTE_INC);
	ret += cps4035_aux_write_word(CPS4035_CMD_SET_HI_ADDR, CPS4035_SRAM_HI_ADDR);
	if(ret) {
		hwlog_err("cps4035_iic_ram_unlock: fail\n");
	}
	return ret;
}

static int cps4035_i2c_read(struct i2c_client *client,
	u8 *cmd, int cmd_len, u8 *dat, int dat_len)
{
	int i;
	int ret;

	if (!client || !cmd || !dat) {
		hwlog_err("i2c_read: para null\n");
		return -WLC_ERR_PARA_NULL;
	}

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		if (!cps4035_is_pwr_good())
			return -WLC_ERR_I2C_R;
		ret = power_i2c_read_block(client, cmd, cmd_len, dat, dat_len);
		if (!ret)
			return 0;
		power_usleep(DT_USLEEP_10MS);
	}

	return -WLC_ERR_I2C_R;
}

static int cps4035_i2c_write(struct i2c_client *client, u8 *cmd, int cmd_len)
{
	int i;
	int ret;

	if (!client || !cmd) {
		hwlog_err("i2c_write: para null\n");
		return -WLC_ERR_PARA_NULL;
	}

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		if (!cps4035_is_pwr_good())
			return -WLC_ERR_I2C_W;
		ret = power_i2c_write_block(client, cmd, cmd_len);
		if (!ret)
			return 0;
		power_usleep(DT_USLEEP_10MS);
	}

	return -WLC_ERR_I2C_W;
}

int cps4035_read_block(u16 reg, u8 *data, u8 len)
{
	int ret;
	u8 cmd[CPS4035_ADDR_LEN];
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di || !data) {
		hwlog_err("read_block: para null\n");
		return -WLC_ERR_PARA_NULL;
	}

	di->client->addr = CPS4035_SW_I2C_ADDR;
	cmd[0] = reg >> BITS_PER_BYTE;
	cmd[1] = reg & BYTE_MASK;

	ret = cps4035_i2c_read(di->client, cmd, CPS4035_ADDR_LEN, data, len);
	if (ret)
		return ret;

	return 0;
}

int cps4035_write_block(u16 reg, u8 *data, u8 len)
{
	int ret;
	u8 cmd[CPS4035_WRITE_LEN];
	struct cps4035_dev_info *di = NULL;
	errno_t rc = EOK;

	cps4035_get_dev_info(&di);
	if (!di || !di->client || !data) {
		hwlog_err("write_block: para null\n");
		return -WLC_ERR_PARA_NULL;
	}

	di->client->addr = CPS4035_SW_I2C_ADDR;
	cmd[0] = reg >> BITS_PER_BYTE;
	cmd[1] = reg & BYTE_MASK;
	rc = memcpy_s(&cmd[CPS4035_ADDR_LEN], (CPS4035_WRITE_LEN - CPS4035_ADDR_LEN),
		data, len);
	if (rc != EOK) {
		hwlog_info("%s : memcpy_s is failed, rc = %d\n", __FUNCTION__, rc);
	}
	ret = cps4035_i2c_write(di->client, cmd, CPS4035_ADDR_LEN + len);
	if (ret)
		return ret;

	return 0;
}

int cps4035_read_byte(u16 reg, u8 *data)
{
	return cps4035_read_block(reg, data, BYTE_LEN);
}

int cps4035_read_word(u16 reg, u16 *data)
{
	int ret;
	u8 buff[WORD_LEN] = {0};

	ret = cps4035_read_block(reg, buff, WORD_LEN);
	if (ret)
		return -WLC_ERR_I2C_R;

	*data = buff[0] | (buff[1] << BITS_PER_BYTE);
	return 0;
}

int cps4035_write_byte(u16 reg, u8 data)
{
	return cps4035_write_block(reg, &data, BYTE_LEN);
}

int cps4035_write_word(u16 reg, u16 data)
{
	u8 buff[WORD_LEN] = {0};

	buff[0] = data & BYTE_MASK;
	buff[1] = data >> BITS_PER_BYTE;

	return cps4035_write_block(reg, buff, WORD_LEN);
}

int cps4035_write_byte_mask(u16 reg, u8 mask, u8 shift, u8 data)
{
	int ret;
	u8 val = 0;

	ret = cps4035_read_byte(reg, &val);
	if (ret)
		return ret;

	val &= ~mask;
	val |= ((data << shift) & mask);

	return cps4035_write_byte(reg, val);
}

int cps4035_write_word_mask(u16 reg, u16 mask, u16 shift, u16 data)
{
	int ret;
	u16 val = 0;

	ret = cps4035_read_word(reg, &val);
	if (ret)
		return ret;

	val &= ~mask;
	val |= ((data << shift) & mask);

	return cps4035_write_word(reg, val);
}

static int cps4035_aux_write_block(u16 reg, u8 *data, u8 data_len)
{
	u8 cmd[CPS4035_WRITE_LEN];
	struct cps4035_dev_info *di = NULL;
	errno_t rc = EOK;

	cps4035_get_dev_info(&di);
	if (!di || !data) {
		hwlog_err("write_block: para null\n");
		return -WLC_ERR_PARA_NULL;
	}

	di->client->addr = CPS4035_HW_I2C_ADDR;
	cmd[0] = reg >> BITS_PER_BYTE;
	cmd[1] = reg & BYTE_MASK;
	rc = memcpy_s(&cmd[CPS4035_ADDR_LEN], (CPS4035_WRITE_LEN - CPS4035_ADDR_LEN),
		data, data_len);
	if (rc != EOK) {
		hwlog_info("%s : memcpy_s is failed, rc = %d\n", __FUNCTION__, rc);
	}

	return cps4035_i2c_write(di->client, cmd, CPS4035_ADDR_LEN + data_len);
}

int cps4035_aux_write_word(u16 reg, u16 data)
{
	u8 buff[WORD_LEN];

	buff[0] = data & BYTE_MASK;
	buff[1] = data >> BITS_PER_BYTE;

	return cps4035_aux_write_block(reg, buff, WORD_LEN);
}

/*
 * cps4035 chip_info
 */
int cps4035_get_chip_id(u16 *chip_id)
{
	int ret;

	ret = cps4035_read_word(CPS4035_CHIP_ID_ADDR, chip_id);
	hwlog_info("[chip_id]:0x%x\n",*chip_id);
	if (ret)
		return ret;
	*chip_id = CPS4035_CHIP_ID;
	return 0;
}

static int cps4035_get_mtp_version(u16 *mtp_ver)
{
	return cps4035_read_word(CPS4035_MTP_VER_ADDR, mtp_ver);
}

int cps4035_get_chip_info(struct cps4035_chip_info *info)
{
	int ret;

	if (!info)
		return -WLC_ERR_PARA_NULL;

	ret = cps4035_get_chip_id(&info->chip_id);
	ret += cps4035_get_mtp_version(&info->mtp_ver);
	if (ret) {
		hwlog_err("get_chip_info: failed\n");
		return ret;
	}

	return 0;
}

int cps4035_get_chip_info_str(char *info_str, int len)
{
	int ret;
	struct cps4035_chip_info chip_info;

	info_str = &g_cps_printf_buf[0];
	if (!info_str || (len != WL_CHIP_INFO_STR_LEN))
		return -WLC_ERR_PARA_NULL;

	ret = cps4035_get_chip_info(&chip_info);
	if (ret)
		return ret;

	memset_s(info_str, CPS4035_CHIP_INFO_STR_LEN, 0, CPS4035_CHIP_INFO_STR_LEN);

	snprintf_s(info_str, sizeof(g_cps_printf_buf), len,
		"chip_id:0x%04x mtp_ver:0x%04x", chip_info.chip_id,
		chip_info.mtp_ver);

	return 0;
}

int cps4035_get_mode(u8 *mode)
{
	int ret;

	if (!mode)
		return -WLC_ERR_PARA_NULL;

	ret = cps4035_iic_ram_unlock();
	if (ret) {
		hwlog_err("cps4035_iic_ram_unlock: failed\n");
		return -WLC_ERR_I2C_R;
	}

	ret = cps4035_read_byte(CPS4035_OP_MODE_ADDR, mode);
	if (ret) {
		hwlog_err("get_mode: failed\n");
		return -WLC_ERR_I2C_R;
	}

	return 0;
}

static void cps4035_wake_lock(void)
{
	if (g_cps4035_wakelock.active) {
		hwlog_info("[wake_lock] already locked\n");
		return;
	}

	__pm_stay_awake(&g_cps4035_wakelock);
	hwlog_info("wake_lock\n");
}

static void cps4035_wake_unlock(void)
{
	if (!g_cps4035_wakelock.active) {
		hwlog_info("[wake_unlock] already unlocked\n");
		return;
	}

	__pm_relax(&g_cps4035_wakelock);
	hwlog_info("wake_unlock\n");
}

void cps4035_ps_control(enum wlps_ctrl_scene scene, int ctrl_flag)
{
	static int ref_cnt;

	hwlog_info("[ps_control] ref_cnt=%d, flag=%d\n", ref_cnt, ctrl_flag);
	if (ctrl_flag == WLPS_CTRL_ON)
		++ref_cnt;
	else if (--ref_cnt > 0)
		return;

	wlps_control(scene, ctrl_flag);
	if (ctrl_flag == WLPS_CTRL_ON) {
		cps4035_iic_ram_unlock();
	}
}

void cps4035_enable_irq(void)
{
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di)
		return;

	mutex_lock(&di->mutex_irq);
	if (!di->irq_active) {
		hwlog_info("[enable_irq] ++\n");
		enable_irq(di->irq_int);
		di->irq_active = true;
	}
	hwlog_info("[enable_irq] --\n");
	mutex_unlock(&di->mutex_irq);
}

void cps4035_disable_irq_nosync(void)
{
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di)
		return;

	mutex_lock(&di->mutex_irq);
	if (di->irq_active) {
		hwlog_info("[disable_irq_nosync] ++\n");
		disable_irq_nosync(di->irq_int);
		di->irq_active = false;
	}
	hwlog_info("[disable_irq_nosync] --\n");
	mutex_unlock(&di->mutex_irq);
}

void cps4035_chip_enable(int enable)
{
	int gpio_val;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di)
		return;

	if (enable == RX_EN_ENABLE)
		gpio_set_value(di->gpio_en, di->gpio_en_valid_val);
	else
		gpio_set_value(di->gpio_en, !di->gpio_en_valid_val);

	gpio_val = gpio_get_value(di->gpio_en);
	hwlog_info("[chip_enable] gpio %s now\n", gpio_val ? "high" : "low");
}

void cps4035_sleep_enable(int enable)
{
	int gpio_val;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di || di->g_val.irq_abnormal_flag)
		return;

	hwlog_info("[sleep_enable] gpio %s now\n", gpio_val ? "high" : "low");
}

static void cps4035_irq_work(struct work_struct *work)
{
	int ret;
	int gpio_val;
	u8 mode = 0;
	u16 chip_id = 0;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di) {
		hwlog_err("irq_work: di null\n");
		goto exit;
	}

	gpio_val = gpio_get_value(di->gpio_en);
	if (gpio_val != di->gpio_en_valid_val) {
		hwlog_err("[irq_work] gpio %s\n", gpio_val ? "high" : "low");
		goto exit;
	}
	msleep(10);
	/* get System Operating Mode */
	ret = cps4035_get_mode(&mode);
	if (!ret)
		hwlog_info("[irq_work] mode=0x%x\n", mode);
	ret = cps4035_get_chip_id(&chip_id);
	if (!ret)
		hwlog_info("[irq_work] chip_id=0x%x\n", chip_id);

	/* handler irq */
	if ((mode == CPS4035_OP_MODE_TX) || (mode == CPS4035_OP_MODE_BP))
		cps4035_tx_mode_irq_handler(di);

exit:
	if (di && !di->g_val.irq_abnormal_flag)
		cps4035_enable_irq();

	cps4035_wake_unlock();
}

static irqreturn_t cps4035_interrupt(int irq, void *_di)
{
	struct cps4035_dev_info *di = _di;

	if (!di) {
		hwlog_err("interrupt: di null\n");
		return IRQ_HANDLED;
	}

	cps4035_wake_lock();
	hwlog_info("[interrupt] ++\n");
	if (di->irq_active) {
		disable_irq_nosync(di->irq_int);
		di->irq_active = false;
		schedule_work(&di->irq_work);
	} else {
		hwlog_info("[interrupt] irq is not enable\n");
		cps4035_wake_unlock();
	}
	hwlog_info("[interrupt] --\n");

	return IRQ_HANDLED;
}

static int cps4035_dev_check(struct cps4035_dev_info *di)
{
	int ret;
	u16 chip_id = 0;

	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_ON);

	power_usleep(DT_USLEEP_10MS);
	ret = cps4035_get_chip_id(&chip_id);
	if (ret) {
		hwlog_err("dev_check: failed\n");
		cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_OFF);
		return ret;
	}
	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_OFF);

	hwlog_info("[dev_check] chip_id=0x%04x\n", chip_id);
	if (chip_id != CPS4035_CHIP_ID)
		hwlog_err("dev_check: rx_chip not match\n");

	return 0;
}

struct device_node *cps4035_dts_dev_node(void *dev_data)
{
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di || !di->dev)
		return NULL;

	return di->dev->of_node;
}

static int cps4035_gpio_init(struct cps4035_dev_info *di,
	struct device_node *np)
{
	if (power_gpio_config_output(np, "gpio_en", "cps4035_en",
		&di->gpio_en, di->gpio_en_valid_val))
		goto gpio_en_fail;

	return 0;

gpio_en_fail:
	return -EINVAL;
}

static int cps4035_irq_init(struct cps4035_dev_info *di,
	struct device_node *np)
{
	if (power_gpio_config_interrupt(np, "gpio_int", "cps4035_int",
		&di->gpio_int, &di->irq_int))
		return -EINVAL;

	if (request_irq(di->irq_int, cps4035_interrupt,
		IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND, "cps4035_irq", di)) {
		hwlog_err("irq_init: request cps4035_irq failed\n");
		gpio_free(di->gpio_int);
		return -EINVAL;
	}

	enable_irq_wake(di->irq_int);
	di->irq_active = true;
	INIT_WORK(&di->irq_work, cps4035_irq_work);

	return 0;
}

static void cps4035_register_pwr_dev_info(struct cps4035_dev_info *di)
{
	int ret;
	u16 chip_id = 0;
	struct power_devices_info_data *pwr_dev_info = NULL;

	ret = cps4035_get_chip_id(&chip_id);
	if (ret)
		return;

	pwr_dev_info = power_devices_info_register();
	if (pwr_dev_info) {
		pwr_dev_info->dev_name = di->dev->driver->name;
		pwr_dev_info->dev_id = chip_id;
		pwr_dev_info->ver_id = 0;
	}
}

static int cps4035_ops_register(struct cps4035_dev_info *di)
{
	int ret;

	ret = cps4035_fw_ops_register();
	if (ret) {
		hwlog_err("ops_register: register fw_ops failed\n");
		return ret;
	}

	ret = cps4035_tx_ps_ops_register();
	if (ret) {
		hwlog_err("ops_register: register tx_ps_ops failed\n");
		return ret;
	}

	ret = cps4035_tx_ops_register();
	if (ret) {
		hwlog_err("ops_register: register tx_ops failed\n");
		return ret;
	}

	ret = cps4035_qi_ops_register();
	if (ret) {
		hwlog_err("ops_register: register qi_ops failed\n");
		return ret;
	}
	di->g_val.qi_hdl = qi_protocol_get_handle();

	return 0;
}

static void cps4035_fw_mtp_check(struct cps4035_dev_info *di)
{
	if (power_cmdline_is_powerdown_charging_mode())  {
		di->g_val.mtp_chk_complete = true;
		return;
	}

	INIT_DELAYED_WORK(&di->mtp_check_work, cps4035_fw_mtp_check_work);
	schedule_delayed_work(&di->mtp_check_work,
		msecs_to_jiffies(WIRELESS_FW_WORK_DELAYED_TIME));
}

static int cps4035_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct cps4035_dev_info *di = NULL;
	struct device_node *np = NULL;

	if (!client || !client->dev.of_node)
		return -ENODEV;

	di = devm_kzalloc(&client->dev, sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	g_cps4035_di = di;
	di->dev = &client->dev;
	np = client->dev.of_node;
	di->client = client;
	i2c_set_clientdata(client, di);

	ret = cps4035_dev_check(di);
	if (ret)
		goto dev_ck_fail;

	ret = cps4035_parse_dts(np, di);
	if (ret)
		goto parse_dts_fail;

	ret = cps4035_gpio_init(di, np);
	if (ret)
		goto gpio_init_fail;

	ret = cps4035_irq_init(di, np);
	if (ret)
		goto irq_init_fail;

	wakeup_source_init(&g_cps4035_wakelock, "cps4035_wakelock");
	mutex_init(&di->mutex_irq);

	ret = cps4035_ops_register(di);
	if (ret)
		goto ops_regist_fail;

	cps4035_fw_mtp_check(di);
	cps4035_register_pwr_dev_info(di);

	hwlog_info("wireless_chip probe ok\n");
	return 0;

ops_regist_fail:
	gpio_free(di->gpio_int);
	free_irq(di->irq_int, di);
irq_init_fail:
	gpio_free(di->gpio_en);

gpio_init_fail:
parse_dts_fail:
dev_ck_fail:
	devm_kfree(&client->dev, di);
	g_cps4035_di = NULL;
	return ret;
}

static void cps4035_shutdown(struct i2c_client *client)
{
	int wired_channel_state;

	hwlog_info("[shutdown] ++\n");
	hwlog_info("[shutdown] --\n");
}

MODULE_DEVICE_TABLE(i2c, wireless_cps4035);
static const struct of_device_id cps4035_of_match[] = {
	{
		.compatible = "cps, wls-charger-cps4035-L-pen",
		.data = NULL,
	},
	{},
};

static const struct i2c_device_id cps4035_i2c_id[] = {
	{ "wls-charger-cps4035-L-pen", 0 }, {}
};

static struct i2c_driver cps4035_driver = {
	.probe = cps4035_probe,
	.shutdown = cps4035_shutdown,
	.id_table = cps4035_i2c_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "cps-wls-charger-l",
		.of_match_table = of_match_ptr(cps4035_of_match),
	},
};

static int __init cps4035_init(void)
{
	return i2c_add_driver(&cps4035_driver);
}

static void __exit cps4035_exit(void)
{
	i2c_del_driver(&cps4035_driver);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
late_initcall(cps4035_init);
#else
device_initcall(cps4035_init);
#endif
module_exit(cps4035_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("cps4035 module driver");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
