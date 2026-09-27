/*
 * cps4035_chip.h
 *
 * cps4035 registers, chip info, etc.
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

#ifndef _CPS4035_CHIP_H_
#define _CPS4035_CHIP_H_

#define CPS4035_SW_I2C_ADDR                   0x30 /* SW: software */
#define CPS4035_HW_I2C_ADDR                   0x31 /* HW: hardware */
#define CPS4035_ADDR_LEN                      2
#define CPS4035_GPIO_PWR_GOOD_VAL             1
#define CPS4035_DFT_IOUT_MAX                  1300
#define CPS4035_CHIP_INFO_STR_LEN             128
#define CPS4035_WRITE_LEN                     32

/* chip id register */
#define CPS4035_CHIP_ID_ADDR                  0x1D00
#define CPS4035_CHIP_ID_LEN                   4
#define CPS4035_CHIP_ID                       0x4035
/* mtp_version register */
#define CPS4035_MTP_VER_ADDR                  0x1D10
#define CPS4035_MTP_VER_LEN                   2
/* op mode register */
#define CPS4035_OP_MODE_ADDR                  0x1D14
#define CPS4035_OP_MODE_LEN                   1
#define CPS4035_OP_MODE_NA                    0x00
#define CPS4035_OP_MODE_BP                    0x01 /* back_powered */
#define CPS4035_OP_MODE_TX                    0x02
#define CPS4035_OP_MODE_RX                    0x03
/* crc val register */
#define CPS4035_CRC_ADDR                      0x1D16
#define CPS4035_CRC_LEN                       2
/* send_msg_data register */
#define CPS4035_SEND_MSG_HEADER_ADDR          0x1D82
#define CPS4035_SEND_MSG_CMD_ADDR             0x1D83
#define CPS4035_SEND_MSG_DATA_ADDR            0x1D84
/* send_msg: bit[0]: header, bit[1]: cmd, bit[2:5]: data */
#define CPS4035_SEND_MSG_DATA_LEN             4
#define CPS4035_SEND_MSG_PKT_LEN              6
/* rcvd_msg_data register */
#define CPS4035_RCVD_MSG_HEADER_ADDR          0x1DC2
#define CPS4035_RCVD_MSG_CMD_ADDR             0x1DC3
#define CPS4035_RCVD_MSG_DATA_ADDR            0x1DC4
/* rcvd_msg: bit[0]: header, bit[1]: cmd, bit[2:5]: data */
#define CPS4035_RCVD_MSG_DATA_LEN             4 //need
#define CPS4035_RCVD_MSG_PKT_LEN              6
#define CPS4035_RCVD_PKT_BUFF_LEN             8
#define CPS4035_RCVD_PKT_STR_LEN              64

/*
 * tx mode
 */

/* tx_irq_en register */
#define CPS4035_TX_IRQ_EN_ADDR                0x1D40
#define CPS4035_TX_IRQ_VAL                    0XFBFF
#define CPS4035_TX_IRQ_EN_LEN                 2
/* tx_irq_latch register */
#define CPS4035_TX_IRQ_ADDR                   0x1D42
#define CPS4035_TX_IRQ_LEN                    2
#define CPS4035_TX_IRQ_START_PING             BIT(0)
#define CPS4035_TX_IRQ_SS_PKG_RCVD            BIT(1)
#define CPS4035_TX_IRQ_ID_PKT_RCVD            BIT(2)
#define CPS4035_TX_IRQ_CFG_PKT_RCVD           BIT(3)
#define CPS4035_TX_IRQ_ASK_PKT_RCVD           BIT(4)
#define CPS4035_TX_IRQ_EPT_PKT_RCVD           BIT(5)
#define CPS4035_TX_IRQ_RPP_TIMEOUT            BIT(6)
#define CPS4035_TX_IRQ_CEP_TIMEOUT            BIT(7)
#define CPS4035_TX_IRQ_AC_DET                 BIT(8)
#define CPS4035_TX_IRQ_TX_INIT                BIT(9)
#define CPS4035_TX_IRQ_RPP_TYPE_ERR           BIT(11)
#define CPS4035_TX_IRQ_FSK_ACK                BIT(12)
/* tx_irq_clr register */
#define CPS4035_TX_IRQ_CLR_ADDR               0x1D44
#define CPS4035_TX_IRQ_CLR_LEN                2
#define CPS4035_TX_IRQ_CLR_ALL                0xFFFF
/* tx_cmd register */
#define CPS4035_TX_CMD_ADDR                   0x1D47
#define CPS4035_TX_CMD_LEN                    1
#define CPS4035_TX_CMD_VAL                    1
#define CPS4035_TX_CMD_CRC_CHK                BIT(0)
#define CPS4035_TX_CMD_CRC_CHK_SHIFT          0
#define CPS4035_TX_CMD_EN_TX                  BIT(1)
#define CPS4035_TX_CMD_EN_TX_SHIFT            1
#define CPS4035_TX_CMD_DIS_TX                 BIT(2)
#define CPS4035_TX_CMD_DIS_TX_SHIFT           2
#define CPS4035_TX_CMD_SEND_MSG               BIT(3)
#define CPS4035_TX_CMD_SEND_MSG_SHIFT         3
#define CPS4035_TX_CMD_SYS_RST                BIT(4)
#define CPS4035_TX_CMD_SYS_RST_SHIFT          4
/* tx_pmax register */
#define CPS4035_TX_PMAX_ADDR                  0x1E02
#define CPS4035_TX_PMAX_LEN                   1
#define CPS4035_TX_RPP_VAL_UNIT               2 /* in 0.5Watt units */
/* tx_ocp_thres register, in mA */
#define CPS4035_TX_OCP_TH_ADDR                0x1E42
#define CPS4035_TX_OCP_TH_LEN                 2
#define CPS4035_TX_OCP_TH                     2000
/* tx_uvp_thres register, in mA */
#define CPS4035_TX_UVP_TH_ADDR                0x1E44
#define CPS4035_TX_UVP_TH_LEN                 2
#define CPS4035_TX_UVP_TH                     3000
/* tx_ovp_thres register, in mV */
#define CPS4035_TX_OVP_TH_ADDR                0x1E46
#define CPS4035_TX_OVP_TH_LEN                 2
#define CPS4035_TX_OVP_TH                     13000
/* tx_min_fop, in kHz */
#define CPS4035_TX_MIN_FOP_ADDR               0x1E48
#define CPS4035_TX_MIN_FOP_LEN                1
#define CPS4035_TX_MIN_FOP                    111
/* tx_max_fop, in kHz */
#define CPS4035_TX_MAX_FOP_ADDR               0x1E49
#define CPS4035_TX_MAX_FOP_LEN                1
#define CPS4035_TX_MAX_FOP                    147
/* tx_ping_freq, in kHz */
#define CPS4035_TX_PING_FREQ_ADDR             0x1E4A
#define CPS4035_TX_PING_FREQ_LEN              1
#define CPS4035_TX_PING_FREQ                  135
#define CPS4035_TX_PING_FREQ_MIN              100
#define CPS4035_TX_PING_FREQ_MAX              150
/* tx_ping_ocp, in mA */
#define CPS4035_TX_PING_OCP_ADDR              0x1E4E
#define CPS4035_TX_PING_OCP_TH                500
#define CPS4035_TX_PING_OCP_LEN               2
/* tx_ping_interval, in ms */
#define CPS4035_TX_PING_INTERVAL_ADDR         0x1E54
#define CPS4035_TX_PING_INTERVAL_LEN          2
#define CPS4035_TX_PING_INTERVAL_MIN          0
#define CPS4035_TX_PING_INTERVAL_MAX          1000
#define CPS4035_TX_PING_INTERVAL              120
/* tx_fod_ploss_cnt register */
#define CPS4035_TX_PLOSS_CNT_ADDR             0x1E57
#define CPS4035_TX_PLOSS_CNT_VAL              3
#define CPS4035_TX_PLOSS_CNT_LEN              1
/* tx_fod_ploss_thres register, in mW */
#define CPS4035_TX_PLOSS_TH0_ADDR             0x1E58 /* 5v full bridge */
#define CPS4035_TX_PLOSS_TH0_VAL              3000
#define CPS4035_TX_PLOSS_TH0_LEN              2
/* func_en register */
#define CPS4035_TX_FUNC_EN_ADDR               0x1E5E
#define CPS4035_TX_FUNC_EN_LEN                2
#define CPS4035_TX_FUNC_EN                    1
#define CPS4035_TX_FUNC_DIS                   0
#define CPS4035_TX_FOD_EN_MASK                BIT(0)
#define CPS4035_TX_FOD_EN_SHIFT               0
 #define CPS4035_FULL_BRIDGE_ITH              200 /* mA */
/* tx_ilimit register */
#define CPS4035_TX_ILIM_ADDR                  0x1E60
#define CPS4035_TX_ILIM_LEN                   2
#define CPS4035_TX_ILIM_MIN                   500
#define CPS4035_TX_ILIM_MAX                   2000
/* tx_oper_freq register, in Hz */
#define CPS4035_TX_OP_FREQ_ADDR               0x1E84
#define CPS4035_TX_OP_FREQ_LEN                2
/* tx_vin register, in mV */
#define CPS4035_TX_VIN_ADDR                   0x1E86
#define CPS4035_TX_VIN_LEN                    2
/* tx_vrect register, in mV */
#define CPS4035_TX_VRECT_ADDR                 0x1E88
#define CPS4035_TX_VRECT_LEN                  2
/* tx_iin register, in mA */
#define CPS4035_TX_IIN_ADDR                   0x1E8A
#define CPS4035_TX_IIN_LEN                    2
/* tx_cep_value register */
#define CPS4035_TX_CEP_ADDR                   0x1E8D
#define CPS4035_TX_CEP_LEN                    1
/* tx_ept_type register */
#define CPS4035_TX_EPT_SRC_ADDR               0x1E90
#define CPS4035_TX_EPT_SRC_CLEAR              0
#define CPS4035_TX_EPT_SRC_LEN                2
#define CPS4035_TX_EPT_SRC_WRONG_PKT          BIT(0) /* re ping */
#define CPS4035_TX_EPT_SRC_AC_DET             BIT(1) /* re ping */
#define CPS4035_TX_EPT_SRC_SSP                BIT(2) /* re ping */
#define CPS4035_TX_EPT_SRC_RX_EPT             BIT(3) /* re ping */
#define CPS4035_TX_EPT_SRC_CEP_TIMEOUT        BIT(4) /* re ping */
#define CPS4035_TX_EPT_SRC_OCP                BIT(6) /* re ping */
#define CPS4035_TX_EPT_SRC_OVP                BIT(7) /* re ping */
#define CPS4035_TX_EPT_SRC_UVP                BIT(8) /* re ping */
#define CPS4035_TX_EPT_SRC_FOD                BIT(9) /* stop */
#define CPS4035_TX_EPT_SRC_OTP                BIT(10) /* re ping */
#define CPS4035_TX_EPT_SRC_POCP               BIT(11) /* stop */
/* tx_pwm_duty register */
#define CPS4035_TX_PWM_DUTY_ADDR              0x1E92 /* need */
#define CPS4035_TX_PWM_DUTY_LEN               2
#define CPS4035_TX_PWM_DUTY_UNIT              50
/* tx_chip_temp register, in degC */
#define CPS4035_TX_CHIP_TEMP_ADDR             0x1E94
#define CPS4035_TX_CHIP_TEMP_LEN              2

/*
 * firmware register
 */

/* i2c test */
#define CPS4035_I2C_TEST_ADDR                 0x1D80
#define CPS4035_I2C_TEST_LEN                  4
#define CPS4035_I2C_TEST_VAL                  0x12345678 /* any data but 0 */
/* hw cmd */
#define CPS4035_CMD_UNLOCK_I2C                0xF500
#define CPS4035_I2C_CODE                      0x19E5
#define CPS4035_CMD_HOLD_MCU                  0xF501
#define CPS4035_HOLD_MCU                      0x153F
#define CPS4035_RELEASE_MCU                   0x0000
#define CPS4035_CMD_SET_HI_ADDR               0xF503
#define CPS4035_CMD_INC_MODE                  0xF505
#define CPS4035_BYTE_INC                      0x0004
#define CPS4035_WORD_INC                      0x0006
/* sram addr */
#define CPS4035_SRAM_HI_ADDR                  0x2000
#define CPS4035_SRAM_BTL_BUFF                 0x0000 /* 2k */
#define CPS4035_SRAM_MTP_BUFF0                0x0800 /* 2k */
#define CPS4035_SRAM_MTP_BUFF1                0x1000 /* 2k */
#define CPS4035_SRAM_MTP_BUFF_SIZE            2048
#define CPS4035_SRAM_BTL_VER_ADDR             0x180C
/* sram cmd */
#define CPS4035_SRAM_CMD_LEN                  4
#define CPS4035_SRAM_STRAT_CMD_ADDR           0x20001800
#define CPS4035_STRAT_CARRY_BUF0              0x00000010
#define CPS4035_STRAT_CARRY_BUF1              0x00000020
#define CPS4035_START_CHK_BTL                 0x000000B0
#define CPS4035_START_CHK_MTP                 0x00000090
#define CPS4035_START_CHK_PGM                 0x00000080
#define CPS4035_SRAM_CHK_CMD_ADDR             0x20001804
#define CPS4035_CHK_SUCC                      0x55
#define CPS4035_CHK_FAIL                      0xAA /* 0x66: running */
/* programming addr */
#define CPS4035_PGM_CMD_LEN                   4
#define CPS4035_PGM_EN_TEST_ADDR              0x40012120
#define CPS4035_PGM_EN_TEST                   0x00001250
#define CPS4035_PGM_DIS_TEST                  0x00000000
#define CPS4035_PGM_EN_CARRY_ADDR             0x40012EE8
#define CPS4035_PGM_EN_CARRY                  0x0000D148
#define CPS4035_PGM_DIS_CARRY                 0x00000000
/* sys_set addr */
#define CPS4035_SYS_CMD_LEN                   4
#define CPS4035_SYS_SOFT_REST_ADDR            0x40040070
#define CPS4035_SYS_SOFT_REST                 0x61A00000
#define CPS4035_SYS_REMAP_EN_ADDR             0x400400A0
#define CPS4035_SYS_REMAP_EN                  0x000000FF
#define CPS4035_SYS_TRIM_DIS_ADDR             0x40040010
#define CPS4035_SYS_TRIM_DIS                  0x00008000

#endif /* _CPS4035_CHIP_H_ */
