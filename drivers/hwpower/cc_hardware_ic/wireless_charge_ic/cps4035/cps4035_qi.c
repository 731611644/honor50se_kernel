/*
 * cps4035_qi.c
 *
 * cps4035 qi_protocol driver; ask: rx->tx; fsk: tx->rx
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

#define HWLOG_TAG wireless_cps4035_qi
HWLOG_REGIST();

static u8 cps4035_get_ask_header(int data_len)
{
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di || !di->g_val.qi_hdl || !di->g_val.qi_hdl->get_ask_hdr) {
		hwlog_err("get_ask_header: para null\n");
		return 0;
	}

	return di->g_val.qi_hdl->get_ask_hdr(data_len);
}

static u8 cps4035_get_fsk_header(int data_len)
{
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di || !di->g_val.qi_hdl || !di->g_val.qi_hdl->get_fsk_hdr) {
		hwlog_err("get_fsk_header: para null\n");
		return 0;
	}

	return di->g_val.qi_hdl->get_fsk_hdr(data_len);
}

/*
 * rx function interface, rx send ask
 */
static int cps4035_qi_send_ask_msg(u8 cmd, u8 *data, int data_len,
	void *dev_data)
{
	hwlog_info("[send_ask_msg] succ, cmd=0x%x\n", cmd);
	return 0;
}

/*
 * rx function interface, rx send ask
 */
static int cps4035_qi_send_ask_msg_ack(u8 cmd, u8 *data, int data_len,
	void *dev_data)
{
	return 0;
}

/*
 * rx function interface, rx receive fsk
 */
static int cps4035_qi_receive_fsk_msg(u8 *data, int data_len, void *dev_data)
{
	hwlog_info("[receive_msg] get tx2rx data(cmd:0x%x) succ\n", data[0]);
	return 0;
}

static int cps4035_qi_send_fsk_msg(u8 cmd, u8 *data, int data_len,
	void *dev_data)
{
	int ret;
	u8 header;
	u8 write_data[CPS4035_SEND_MSG_DATA_LEN] = { 0 };

	if ((data_len > CPS4035_SEND_MSG_DATA_LEN) || (data_len < 0)) {
		hwlog_err("send_fsk_msg: data number out of range\n");
		return -WLC_ERR_PARA_WRONG;
	}

	if (cmd == QI_CMD_ACK)
		header = QI_CMD_ACK_HEAD;
	else
		header = cps4035_get_fsk_header(data_len + 1);
	if (header <= 0) {
		hwlog_err("send_fsk_msg: header wrong\n");
		return -WLC_ERR_PARA_WRONG;
	}
	ret = cps4035_write_byte(CPS4035_SEND_MSG_HEADER_ADDR, header);
	if (ret) {
		hwlog_err("send_fsk_msg: write header failed\n");
		return ret;
	}
	ret = cps4035_write_byte(CPS4035_SEND_MSG_CMD_ADDR, cmd);
	if (ret) {
		hwlog_err("send_fsk_msg: write cmd failed\n");
		return ret;
	}

	if (data && data_len > 0) {
		memcpy_s(write_data, CPS4035_SEND_MSG_DATA_LEN, data, data_len);
		ret = cps4035_write_block(CPS4035_SEND_MSG_DATA_ADDR,
			write_data, data_len);
		if (ret) {
			hwlog_err("send_fsk_msg: write fsk reg failed\n");
			return ret;
		}
	}
	ret = cps4035_write_byte_mask(CPS4035_TX_CMD_ADDR,
		CPS4035_TX_CMD_SEND_MSG, CPS4035_TX_CMD_SEND_MSG_SHIFT,
		CPS4035_TX_CMD_VAL);
	if (ret) {
		hwlog_err("send_fsk_msg: send fsk failed\n");
		return ret;
	}

	hwlog_info("[send_fsk_msg] succ\n");
	return 0;
}

static int cps4035_qi_send_fsk_with_ack(u8 cmd, u8 *data, int data_len,
	void *dev_data)
{
	int i;
	int ret;
	struct cps4035_dev_info *di = NULL;

	cps4035_get_dev_info(&di);
	if (!di) {
		hwlog_err("send_fsk_with_ack: para null\n");
		return -WLC_ERR_PARA_NULL;
	}

	di->irq_val &= ~CPS4035_TX_IRQ_FSK_ACK;
	ret = cps4035_qi_send_fsk_msg(cmd, data, data_len, dev_data);
	if (ret)
		return ret;

	for (i = 0; i < CPS4035_WAIT_FOR_ACK_RETRY_CNT; i++) {
		msleep(CPS4035_WAIT_FOR_ACK_SLEEP_TIME);
		if (di->irq_val & CPS4035_TX_IRQ_FSK_ACK) {
			di->irq_val &= ~CPS4035_TX_IRQ_FSK_ACK;
			hwlog_info("[send_fsk_with_ack] succ\n");
			return 0;
		}
		if (di->g_val.tx_stop_chrg_flag)
			return -WLC_ERR_STOP_CHRG;
	}

	hwlog_err("send_fsk_with_ack: failed\n");
	return -WLC_ERR_ACK_TIMEOUT;
}

static int cps4035_qi_receive_ask_pkt(u8 *pkt_data, int pkt_data_len,
	void *dev_data)
{
	int ret;
	int i;
	char buff[CPS4035_RCVD_PKT_BUFF_LEN] = { 0 };
	char pkt_str[CPS4035_RCVD_PKT_STR_LEN] = { 0 };

	if (!pkt_data || (pkt_data_len <= 0) ||
		(pkt_data_len > CPS4035_RCVD_MSG_PKT_LEN)) {
		hwlog_err("get_ask_pkt: para err\n");
		return -1;
	}
	ret = cps4035_read_block(CPS4035_RCVD_MSG_HEADER_ADDR,
		pkt_data, pkt_data_len);
	if (ret) {
		hwlog_err("get_ask_pkt: read failed\n");
		return -1;
	}
	for (i = 0; i < pkt_data_len; i++) {
		snprintf_s(buff, sizeof(buff)/sizeof(buff[0]),
			CPS4035_RCVD_PKT_BUFF_LEN, "0x%02x ", pkt_data[i]);
		strncat_s(pkt_str, CPS4035_RCVD_PKT_STR_LEN, buff, strlen(buff));
	}

	hwlog_info("[get_ask_pkt] RX back packet: %s\n", pkt_str);
	return 0;
}

/*
 * rx function interface
 */
static int cps4035_qi_set_rx_rpp_format(u8 pmax)
{
	int ret;
	return 0;
}

/*
 * Function function is not clear at present, reserved for now
 */
static int cps4035_qi_set_tx_rpp_format(u8 pmax)
{
	int ret;
	return 0;
}

static int cps4035_qi_set_rpp_format(u8 pmax, int mode, void *dev_data)
{
	if (mode == WIRELESS_RX)
		return cps4035_qi_set_rx_rpp_format(pmax);

	return cps4035_qi_set_tx_rpp_format(pmax);
}

int cps4035_qi_get_fw_version(u8 *data, int len, void *dev_data)
{
	struct cps4035_chip_info chip_info;

	/* fw version length must be 4 */
	if (!data || (len != 4)) {
		hwlog_err("get_fw_version: para err");
		return -WLC_ERR_PARA_WRONG;
	}

	if (cps4035_get_chip_info(&chip_info)) {
		hwlog_err("get_fw_version: get chip_info failed\n");
		return -WLC_ERR_I2C_R;
	}

	/* byte[0:1]=chip_id, byte[2:3]=mtp_ver */
	data[0] = (u8)((chip_info.chip_id >> 0) & BYTE_MASK);
	data[1] = (u8)((chip_info.chip_id >> BITS_PER_BYTE) & BYTE_MASK);
	data[2] = (u8)((chip_info.mtp_ver >> 0) & BYTE_MASK);
	data[3] = (u8)((chip_info.mtp_ver >> BITS_PER_BYTE) & BYTE_MASK);

	return 0;
}

static struct qi_protocol_ops g_cps4035_qi_ops = {
	.chip_name = "cps4057",
	.send_msg = cps4035_qi_send_ask_msg,
	.send_msg_with_ack = cps4035_qi_send_ask_msg_ack,
	.receive_msg = cps4035_qi_receive_fsk_msg,
	.send_fsk_msg = cps4035_qi_send_fsk_msg,
	.auto_send_fsk_with_ack = cps4035_qi_send_fsk_with_ack,
	.get_ask_packet = cps4035_qi_receive_ask_pkt,
	.get_chip_fw_version = cps4035_qi_get_fw_version,
	.get_tx_id_pre = NULL,
	.set_rpp_format_post = cps4035_qi_set_rpp_format,
};

int cps4035_qi_ops_register(void)
{
	return qi_protocol_ops_register(&g_cps4035_qi_ops);
}
