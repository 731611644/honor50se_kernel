/*
 * cps4035_fw.c
 *
 * cps4035 mtp, sram driver
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
#include "cps4035_mtp.h"

#define HWLOG_TAG wireless_cps4035_fw
HWLOG_REGIST();

static bool g_cps4035_mtp_latest;

static int cps4035_fw_unlock_i2c(void)
{
	return cps4035_aux_write_word(CPS4035_CMD_UNLOCK_I2C, CPS4035_I2C_CODE);
}

static int cps4035_fw_set_hi_addr(u32 hi_addr)
{
	return cps4035_aux_write_word(CPS4035_CMD_SET_HI_ADDR, hi_addr);
}

static int cps4035_fw_set_increase_mode(u16 mode)
{
	return cps4035_aux_write_word(CPS4035_CMD_INC_MODE, mode);
}

static int cps4035_fw_write_block(u32 addr, u8 *cmd, int len)
{
	int ret;
	u16 low_addr = addr & 0xFFFF; /* low 16bit addr */
	u16 hi_addr = addr >> 16; /* high 16bit addr */

	ret = cps4035_fw_set_hi_addr(hi_addr);
	if (ret)
		return ret;

	ret = cps4035_write_block(low_addr, cmd, len);
	if (ret)
		return ret;

	return 0;
}

static int cps4035_fw_read_block(u32 addr, u8 *data, int len)
{
	int ret;
	u16 low_addr = addr & 0xFFFF; /* low 16bit addr */
	u16 hi_addr = addr >> 16; /* high 16bit addr */

	ret = cps4035_fw_set_hi_addr(hi_addr);
	if (ret)
		return ret;

	ret = cps4035_read_block(low_addr, data, len);
	if (ret)
		return ret;

	return 0;
}

static int cps4035_fw_hold_mcu(void)
{
	return cps4035_aux_write_word(CPS4035_CMD_HOLD_MCU, CPS4035_HOLD_MCU);
}

static void cps4035_fw_soft_reset_system(void)
{
	u32 cmd = CPS4035_SYS_SOFT_REST;

	/* soft_reset: ignore i2c failure */
	(void)cps4035_fw_write_block(CPS4035_SYS_SOFT_REST_ADDR,
		(u8 *)&cmd, CPS4035_SYS_CMD_LEN);
	hwlog_info("reset_system: ignore i2c failure\n");
	msleep(100); /* sleep for system reset */
}

static int cps4035_enter_program_mode(void)
{
	int ret;
	u32 cmd;

	cmd = CPS4035_PGM_EN_TEST;
	/* soft_reset: ignore i2c failure */
	ret = cps4035_fw_write_block(CPS4035_PGM_EN_TEST_ADDR,
		(u8 *)&cmd, CPS4035_SYS_CMD_LEN);
	if (ret) {
		hwlog_err("cps4035_enter_program_mode: CPS4035_PGM_EN_TEST_ADDR failed\n");
		return ret;
	}

	cmd = CPS4035_PGM_EN_CARRY;
	/* soft_reset: ignore i2c failure */
	ret = cps4035_fw_write_block(CPS4035_PGM_EN_CARRY_ADDR,
		(u8 *)&cmd, CPS4035_SYS_CMD_LEN);
	if (ret) {
		hwlog_err("cps4035_enter_program_mode: CPS4035_PGM_EN_CARRY_ADDR failed\n");
		return ret;
	}

	hwlog_info("cps_enter_program_mode\n");
	msleep(100);
	return 0;
}

static int cps4035_exit_program_mode(void)
{
	int ret;
	u32 cmd;

	cmd = CPS4035_PGM_DIS_TEST;
	/* soft_reset: ignore i2c failure */
	ret = cps4035_fw_write_block(CPS4035_PGM_EN_TEST_ADDR,
		(u8 *)&cmd, CPS4035_SYS_CMD_LEN);
	if (ret) {
		hwlog_err("cps4035_exit_program_mode: CPS4035_PGM_EN_TEST_ADDR failed\n");
		return ret;
	}

	cmd = CPS4035_PGM_DIS_CARRY;
	/* soft_reset: ignore i2c failure */
	ret = cps4035_fw_write_block(CPS4035_PGM_EN_CARRY_ADDR,
		(u8 *)&cmd, CPS4035_SYS_CMD_LEN);
	if (ret) {
		hwlog_err("cps4035_exit_program_mode: CPS4035_PGM_EN_CARRY_ADDR failed\n");
		return ret;
	}
	hwlog_info("cps4035_exit_program_mode\n");
	msleep(100);
	return 0;
}

static int cps4035_fw_sram_i2c_init(u8 inc_mode)
{
	int ret;
	u32 wr_data = CPS4035_I2C_TEST_VAL;
	u32 rd_data = 0;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di)
		return -WLC_ERR_PARA_NULL;

	ret = cps4035_fw_unlock_i2c();
	if (ret)
		return ret;
	ret = cps4035_fw_set_increase_mode(inc_mode);
	if (ret)
		return ret;
	ret = cps4035_fw_set_hi_addr(CPS4035_SRAM_HI_ADDR);
	if (ret)
		return ret;

	ret = cps4035_write_block(CPS4035_I2C_TEST_ADDR,
		(u8 *)&wr_data, CPS4035_I2C_TEST_LEN);
	if (ret)
		return ret;
	ret = cps4035_read_block(CPS4035_I2C_TEST_ADDR,
		(u8 *)&rd_data, CPS4035_I2C_TEST_LEN);
	if (ret)
		return ret;

	if (rd_data != wr_data) {
		hwlog_err("sram_i2c_init: failed, rd_data=0x%x\n", rd_data);
		return -WLC_ERR_MISMATCH;
	}

	return 0;
}

static int cps4035_fw_enable_remap(void)
{
	u32 cmd = CPS4035_SYS_REMAP_EN;

	return cps4035_fw_write_block(CPS4035_SYS_REMAP_EN_ADDR,
		(u8 *)&cmd, CPS4035_SYS_CMD_LEN);
}

static int cps4035_fw_disable_trim(void)
{
	u32 cmd = CPS4035_SYS_TRIM_DIS;

	return cps4035_fw_write_block(CPS4035_SYS_TRIM_DIS_ADDR,
		(u8 *)&cmd, CPS4035_SYS_CMD_LEN);
}

static int cps4035_fw_check_mtp_crc(void)
{
	int i;
	int ret;
	u16 crc = 0;

	ret = cps4035_fw_sram_i2c_init(CPS4035_BYTE_INC);
	if (ret) {
		hwlog_err("check_mtp_crc: i2c init failed\n");
		return ret;
	}
	ret = cps4035_write_byte_mask(CPS4035_TX_CMD_ADDR,
		CPS4035_TX_CMD_CRC_CHK, CPS4035_TX_CMD_CRC_CHK_SHIFT,
		CPS4035_TX_CMD_VAL);
	if (ret) {
		hwlog_err("check_mtp_crc: write cmd failed\n");
		return ret;
	}

	/* 100ms*5=500ms timeout for status check */
	for (i = 0; i < 5; i++) {
		msleep(100);
		ret = cps4035_read_word(CPS4035_CRC_ADDR, &crc);
		if (ret) {
			hwlog_err("check_mtp_crc: get crc failed\n");
			return ret;
		}
		if (crc == CPS4035_MTP_CRC)
			return 0;
	}

	hwlog_err("check_mtp_crc: timeout, crc=0x%x\n", crc);
	return -WLC_ERR_CHECK_FAIL;
}

static int cps4035_fw_check_mtp_version(void)
{
	int ret;
	struct cps4035_chip_info info;

	ret = cps4035_fw_sram_i2c_init(CPS4035_BYTE_INC);
	if (ret) {
		hwlog_err("check_mtp_match: i2c init failed\n");
		return ret;
	}
	ret = cps4035_get_chip_info(&info);
	if (ret) {
		hwlog_err("check_mtp_match: get chip_info failed\n");
		return ret;
	}

	hwlog_info("[check_mtp_match] mtp_ver=0x%04x\n", info.mtp_ver);
	if (info.mtp_ver != CPS4035_MTP_VER)
		return -WLC_ERR_MISMATCH;

	return 0;
}

static int cps4035_fw_status_check(u32 cmd)
{
	int i;
	int ret;
	u8 status = 0;

	ret = cps4035_fw_write_block(CPS4035_SRAM_STRAT_CMD_ADDR,
		(u8 *)&cmd, CPS4035_SRAM_CMD_LEN);
	if (ret) {
		hwlog_err("status_check: set check cmd failed\n");
		return ret;
	}

	/* wait for 50ms*10=500ms for status check */
	for (i = 0; i < 10; i++) {
		msleep(500);
		ret = cps4035_fw_read_block(CPS4035_SRAM_CHK_CMD_ADDR,
			&status, BYTE_LEN);
		if (ret) {
			hwlog_err("status_check: get status failed\n");
			return ret;
		}
		if (status == CPS4035_CHK_SUCC)
			return 0;
		if (status == CPS4035_CHK_FAIL) {
			hwlog_err("status_check: failed, stat=0x%x\n", status);
			return -WLC_ERR_CHECK_FAIL;
		}
	}

	hwlog_err("status_check: status=0x%x, program timeout\n", status);
	return -WLC_ERR_CHECK_FAIL;
}

static int cps4035_fw_write_sram_data(u32 cur_addr,
	const u8 *data, int len)
{
	int ret = 0;
	int size_to_wr;
	u32 wr_already = 0;
	u32 addr = cur_addr;
	int remaining = len;
	u8 wr_buff[CPS4035_FW_PAGE_SIZE] = { 0 };
	int i;
	int remainder;
	int quotient;
	errno_t rc = EOK;

	while (remaining > 0) {
		hwlog_info("%s : remaining = %d\n", __func__, remaining);
		size_to_wr = remaining > CPS4035_FW_PAGE_SIZE ?
			CPS4035_FW_PAGE_SIZE : remaining;
		rc = memcpy_s(wr_buff, CPS4035_FW_PAGE_SIZE, data + wr_already, size_to_wr);
		if (rc != EOK) {
			hwlog_info("%s : memcpy_s is failed, rc = %d\n", __FUNCTION__, rc);
		}

		quotient = size_to_wr / CPS4035_FW_PAGE_SEGMENT_SIZE;
		remainder = size_to_wr % CPS4035_FW_PAGE_SEGMENT_SIZE;
		for (i = 0; i < quotient; i++)
			ret |= cps4035_write_block(addr + (i * CPS4035_FW_PAGE_SEGMENT_SIZE),
				wr_buff + (i * CPS4035_FW_PAGE_SEGMENT_SIZE),
				CPS4035_FW_PAGE_SEGMENT_SIZE);
		if (remainder != 0)
			ret |= cps4035_write_block(addr + (quotient * CPS4035_FW_PAGE_SEGMENT_SIZE),
				wr_buff + (quotient * CPS4035_FW_PAGE_SEGMENT_SIZE), remainder);
		if (ret) {
			hwlog_err("write_sram_data: fail, addr=0x%x\n", addr);
			return ret;
		}
		addr += size_to_wr;
		wr_already += size_to_wr;
		remaining -= size_to_wr;
	}

	return 0;
}

static int cps4035_fw_write_mtp_data(void)
{
	int ret;
	int offset = 0;
	int remaining = CPS4035_FW_MTP_SIZE;
	int wr_size;

	while (remaining > 0) {
		wr_size = remaining > CPS4035_SRAM_MTP_BUFF_SIZE ?
			CPS4035_SRAM_MTP_BUFF_SIZE : remaining;
		ret = cps4035_fw_write_sram_data(CPS4035_SRAM_MTP_BUFF0,
			g_cps4035_mtp + offset, wr_size);
		if (ret) {
			hwlog_err("write_mtp_data: write mtp failed\n");
			return ret;
		}
		ret = cps4035_fw_status_check(CPS4035_STRAT_CARRY_BUF0);
		if (ret) {
			hwlog_err("write_mtp_data: check crc failed\n");
			return ret;
		}
		offset += wr_size;
		remaining -= wr_size;
	}

	return 0;
}

static int cps4035_fw_load_mtp(void)
{
	int ret;

	cps4035_enter_program_mode();

	ret = cps4035_fw_set_hi_addr(CPS4035_SRAM_HI_ADDR);
	if (ret) {
		hwlog_err("load_mtp: switch to sram addr failed\n");
		return ret;
	}
	ret = cps4035_fw_write_mtp_data();
	if (ret) {
		hwlog_err("load_mtp: write mtp data failed\n");
		return ret;
	}

	cps4035_exit_program_mode();

	ret = cps4035_fw_status_check(CPS4035_START_CHK_MTP);
	if (ret) {
		hwlog_err("load_mtp: check mtp crc failed\n");
		return ret;
	}

	hwlog_info("[load_mtp] succ\n");
	return 0;
}

static int cps4035_fw_check_bootloader_version(void)
{
	int ret;
	u16 btl_ver = 0;

	ret = cps4035_read_word(CPS4035_SRAM_BTL_VER_ADDR, &btl_ver);
	if (ret) {
		hwlog_err("check_bootloader: get bootloader version failed\n");
		return ret;
	}
	hwlog_info("[check_bootloader] ver=0x%04x\n", btl_ver);
	if (btl_ver != CPS4035_BTL_VER) {
		hwlog_err("check_bootloader: exp_ver=0x%x\n", CPS4035_BTL_VER);
		return -WLC_ERR_CHECK_FAIL;
	}

	return 0;
}

static int cps4035_fw_check_bootloader(void)
{
	int ret;

	ret = cps4035_fw_sram_i2c_init(CPS4035_WORD_INC);
	if (ret) {
		hwlog_err("check_bootloader: i2c init failed\n");
		return ret;
	}
	ret = cps4035_fw_check_bootloader_version();
	if (ret) {
		hwlog_err("check_bootloader: check version failed\n");
		return ret;
	}
	ret = cps4035_fw_status_check(CPS4035_START_CHK_BTL);
	if (ret) {
		hwlog_err("check_bootloader: check crc failed\n");
		return ret;
	}

	return 0;
}

static int cps4035_fw_load_bootloader(void)
{
	int ret;

	ret = cps4035_fw_sram_i2c_init(CPS4035_WORD_INC);
	if (ret) {
		hwlog_err("load_bootloader: i2c init failed\n");
		return ret;
	}
	ret = cps4035_fw_write_sram_data(CPS4035_SRAM_BTL_BUFF,
		g_cps4035_bootloader, CPS4035_FW_BTL_SIZE);
	if (ret) {
		hwlog_err("load_bootloader: load bootloader data failed\n");
		return ret;
	}
	ret = cps4035_fw_enable_remap();
	if (ret) {
		hwlog_err("load_bootloader: enble remap failed\n");
		return ret;
	}
	ret = cps4035_fw_disable_trim();
	if (ret) {
		hwlog_err("load_bootloader: disable trim failed\n");
		return ret;
	}
	cps4035_fw_soft_reset_system();
	ret = cps4035_fw_check_bootloader();
	if (ret) {
		hwlog_err("load_bootloader: check bootloader failed\n");
		return ret;
	}

	hwlog_info("[load_bootloader] succ\n");
	return 0;
}

static int cps4035_fw_progam_post_process(void)
{
	int ret;

	cps4035_enter_program_mode();

	ret = cps4035_fw_status_check(CPS4035_START_CHK_PGM);
	if (ret) {
		hwlog_err("progam_post_process: get program result failed\n");
		return ret;
	}

	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_OFF);
	power_usleep(DT_USLEEP_10MS);
	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_ON);
	msleep(100); /* delay 100ms for mcu restart */

	ret = cps4035_fw_check_mtp_version();
	if (ret) {
		hwlog_err("progam_post_process: mtp_version mismatch\n");
		return ret;
	}

	cps4035_exit_program_mode();
	return 0;
}

static int cps4035_fw_program_prev_process(void)
{
	int ret;

	ret = cps4035_fw_unlock_i2c();
	if (ret) {
		hwlog_err("program_prev_process: unlock i2c failed\n");
		return ret;
	}
	ret = cps4035_fw_hold_mcu();
	if (ret) {
		hwlog_err("program_prev_process: hold mcu failed\n");
		return ret;
	}

	return 0;
}

static int cps4035_fw_program_mtp(void)
{
	int ret;

	if (g_cps4035_mtp_latest)
		return 0;

	cps4035_disable_irq_nosync();
	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_ON);
	cps4035_chip_enable(RX_EN_ENABLE);
	msleep(100); /* delay for power on */

	ret = cps4035_fw_program_prev_process();
	if (ret)
		goto exit;
	ret = cps4035_fw_load_bootloader();
	if (ret)
		goto exit;
	ret = cps4035_fw_load_mtp();
	if (ret)
		goto exit;
	ret = cps4035_fw_progam_post_process();
	if (ret)
		goto exit;

	g_cps4035_mtp_latest = true;
	hwlog_info("[program_mtp] succ\n");

exit:
	cps4035_enable_irq();
	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_OFF);
	return ret;
}

static int cps4035_fw_rx_program_mtp(int proc_type)
{
	int ret;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di)
		return -WLC_ERR_PARA_NULL;

	hwlog_info("[rx_program_mtp] type=%d\n", proc_type);

	if (!di->g_val.mtp_chk_complete)
		return -WLC_ERR_I2C_WR;

	di->g_val.mtp_chk_complete = false;
	ret = cps4035_fw_program_mtp();
	if (!ret)
		hwlog_info("[rx_program_mtp] succ\n");
	di->g_val.mtp_chk_complete = true;

	return ret;
}

static int cps4035_fw_check_mtp(void)
{
	if (g_cps4035_mtp_latest)
		return 0;

	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_ON);
	msleep(100); /* delay for power on */

	if (cps4035_fw_check_mtp_version()) {
		hwlog_err("check_mtp: mtp_ver mismatch\n");
		goto check_fail;
	}

	if (cps4035_fw_check_mtp_crc()) {
		hwlog_err("check_mtp: mtp_crc mismatch\n");
		goto check_fail;
	}

	g_cps4035_mtp_latest = true;
	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_OFF);
	hwlog_info("[check_mtp] mtp latest\n");
	return 0;

check_fail:
	cps4035_ps_control(WLPS_TX_SW, WLPS_CTRL_OFF);
	return -WLC_ERR_CHECK_FAIL;
}

int cps4035_fw_sram_update(enum wireless_mode sram_mode)
{
	int ret;
	struct cps4035_chip_info info = { 0 };

	ret = cps4035_get_chip_info(&info);
	if (!ret)
		hwlog_info("[chip_info] chip_id=0x%x, mtp_version=0x%x\n",
			info.chip_id, info.mtp_ver);

	return 0;
}

static int cps4035_fw_is_mtp_exist(void)
{
	int ret;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di) {
		hwlog_err("is_mtp_exist: di null\n");
		return WIRELESS_FW_NON_PROGRAMED;
	}

	di->g_val.mtp_chk_complete = false;
	ret = cps4035_fw_check_mtp();
	if (!ret) {
		di->g_val.mtp_chk_complete = true;
		return WIRELESS_FW_PROGRAMED;
	}
	di->g_val.mtp_chk_complete = true;

	return WIRELESS_FW_NON_PROGRAMED;
}

void cps4035_fw_mtp_check_work(struct work_struct *work)
{
	int i;
	int ret;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di) {
		hwlog_err("mtp_check_work: di null\n");
		return;
	}

	di->g_val.mtp_chk_complete = false;
	ret = cps4035_fw_check_mtp();
	if (!ret) {
		hwlog_info("[mtp_check_work] succ\n");
		goto exit;
	}

	/* program for 3 times until it's ok */
	for (i = 0; i < 3; i++) {
		ret = cps4035_fw_program_mtp();
		if (ret)
			continue;
		hwlog_info("[mtp_check_work] update mtp succ, cnt=%d\n", i + 1);
		goto exit;
	}
	hwlog_err("mtp_check_work: update mtp failed\n");

exit:
	di->g_val.mtp_chk_complete = true;
}

static struct wireless_fw_ops g_cps4035_fw_ops = {
	.program_fw = cps4035_fw_rx_program_mtp,
	.is_fw_exist = cps4035_fw_is_mtp_exist,
	.check_fw = cps4035_fw_check_mtp,
};

int cps4035_fw_ops_register(void)
{
	return wireless_fw_ops_register(&g_cps4035_fw_ops);
}
