/*
 *  mfc_charger.c
 *  Samsung MFC IC Charger Driver
 *
 *  Copyright (C) 2016 Samsung Electronics
 *  Jungmin Lee <jmru.lee@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "include/charger/mfc_charger.h"
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/ctype.h>
#include <linux/firmware.h>

#define ENABLE 1
#define DISABLE 0
#define CMD_CNT 3

int mfc_otp_update = 0;

extern bool sleep_mode;

static enum power_supply_property mfc_charger_props[] = {
};

extern unsigned int lpcharge;
int mfc_get_firmware_version(struct mfc_charger_data *charger, int firm_mode);
static irqreturn_t mfc_wpc_det_irq_thread(int irq, void *irq_data);
static irqreturn_t mfc_wpc_irq_thread(int irq, void *irq_data);

static int mfc_reg_read(struct i2c_client *client, u16 reg, u8 *val)
{
	struct mfc_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2];
	u8 rbuf[2];

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rbuf;

	mutex_lock(&charger->io_lock);
	ret = i2c_transfer(client->adapter, msg, 2);
	mutex_unlock(&charger->io_lock);
	if (ret < 0)
	{
		pr_err("%s: i2c read error, reg: 0x%x, ret: %d (called by %ps)\n",
			__func__, reg, ret, __builtin_return_address(0));
		return -1;
	}
	*val = rbuf[0];

	return ret;
}

static int mfc_reg_multi_read(struct i2c_client *client, u16 reg, u8 *val, int size)
{
	struct mfc_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2];

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = val;

	mutex_lock(&charger->io_lock);
	ret = i2c_transfer(client->adapter, msg, 2);
	mutex_unlock(&charger->io_lock);
	if (ret < 0)
	{
		pr_err("%s: i2c transfer fail", __func__);
		return -1;
	}

	return ret;
}

static int mfc_reg_write(struct i2c_client *client, u16 reg, u8 val)
{
	struct mfc_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };

	mutex_lock(&charger->io_lock);
	ret = i2c_master_send(client, data, 3);
	mutex_unlock(&charger->io_lock);
	if (ret < 3) {
		pr_err("%s: i2c write error, reg: 0x%x, ret: %d (called by %ps)\n",
				__func__, reg, ret, __builtin_return_address(0));
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

static int mfc_reg_update(struct i2c_client *client, u16 reg, u8 val, u8 mask)
{
	//val = 0b 0000 0001
	//ms = 0b 1111 1110
	struct mfc_charger_data *charger = i2c_get_clientdata(client);
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };
	u8 data2;
	int ret;

	ret = mfc_reg_read(client, reg, &data2);
	if (ret >= 0) {
		u8 old_val = data2 & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		data[2] = new_val;

		mutex_lock(&charger->io_lock);
		ret = i2c_master_send(client, data, 3);
		mutex_unlock(&charger->io_lock);
		if (ret < 3) {
			pr_err("%s: i2c write error, reg: 0x%x, ret: %d\n",
				__func__, reg, ret);
			return ret < 0 ? ret : -EIO;
		}
	}
	mfc_reg_read(client, reg, &data2);

	return ret;
}
////
int mfc_get_firmware_version(struct mfc_charger_data *charger, int firm_mode)
{
	int version = -1;
	int ret;
	u8 fw_major[2] = {0,};
	u8 fw_minor[2] = {0,};

	pr_info("%s: called by (%ps)\n", __func__, __builtin_return_address(0));
	switch (firm_mode) {
		case MFC_RX_FIRMWARE:
			ret = mfc_reg_read(charger->client, MFC_FW_MAJOR_REV_L_REG, &fw_major[0]);
			ret = mfc_reg_read(charger->client, MFC_FW_MAJOR_REV_H_REG, &fw_major[1]);
			if (ret >= 0) {
				version =  fw_major[0] | (fw_major[1] << 8);
			}
			pr_info("%s rx major firmware version 0x%x\n", __func__, version);

			ret = mfc_reg_read(charger->client, MFC_FW_MINOR_REV_L_REG, &fw_minor[0]);
			ret = mfc_reg_read(charger->client, MFC_FW_MINOR_REV_H_REG, &fw_minor[1]);
			if (ret >= 0) {
				version = fw_minor[0] | (fw_minor[1] << 8);
			}
			pr_info("%s rx minor firmware version 0x%x\n", __func__, version);
			break;
		default:
			pr_err("%s Wrong firmware mode\n", __func__);
			version = -1;
			break;
	}

	return version;
}
int mfc_get_chip_id(struct mfc_charger_data *charger)
{
	u8 chip_id;

	mfc_reg_read(charger->client, MFC_CHIP_ID_L_REG, &chip_id);
	if (chip_id == 0x40) {
		charger->chip_id = MFC_CHIP_LSI;
		pr_info("%s: LSI CHIP\n", __func__);
	} else { /* 0x20 */
		charger->chip_id = MFC_CHIP_IDT;
		pr_info("%s: IDT CHIP\n", __func__);
	}
	return charger->chip_id;
}
int mfc_get_ic_revision(struct mfc_charger_data *charger, int read_mode)
{
	u8 temp;
	int ret;

	pr_info("%s: called by (%ps)\n", __func__, __builtin_return_address(0));

	switch (read_mode) {
		case MFC_IC_REVISION:
			ret = mfc_reg_read(charger->client, MFC_CHIP_REVISION_REG, &temp);

			if (ret >= 0) {
				temp &= MFC_CHIP_REVISION_MASK;
				pr_info("%s ic revision = %d\n", __func__, temp);
				ret =  temp;
			}
			else
				ret = -1;
			break;
		case MFC_IC_FONT:
			ret = mfc_reg_read(charger->client, MFC_CHIP_REVISION_REG, &temp);

			if (ret >= 0) {
				temp &= MFC_CHIP_FONT_MASK;
				pr_info("%s ic font = %d\n", __func__, temp);
				ret =  temp;
			}
			else
				ret = -1;
			break;
		default :
			ret = -1;
			break;
	}
	return ret;
}

int mfc_get_adc(struct mfc_charger_data *charger, int adc_type)
{
	int ret = 0;
	u8 data[2] = {0,};

	switch (adc_type) {
		case MFC_ADC_VOUT:
			ret = mfc_reg_read(charger->client, MFC_ADC_VOUT_L_REG, &data[0]);
			ret = mfc_reg_read(charger->client, MFC_ADC_VOUT_H_REG, &data[1]);
			if (ret >= 0) {
				ret = (data[0] | (data[1] << 8));
			} else
				ret = -1;
			break;
		case MFC_ADC_VRECT:
			ret = mfc_reg_read(charger->client, MFC_ADC_VRECT_L_REG, &data[0]);
			ret = mfc_reg_read(charger->client, MFC_ADC_VRECT_H_REG, &data[1]);
			if (ret >= 0) {
				ret = (data[0] | (data[1] << 8));
			} else
				ret = -1;
			break;
		case MFC_ADC_TX_ISENSE:
			ret = mfc_reg_read(charger->client, MFC_ADC_TX_ISENSE_L_REG, &data[0]);
			ret = mfc_reg_read(charger->client, MFC_ADC_TX_ISENSE_H_REG, &data[1]);
			if (ret >= 0) {
				ret = (data[0] | (data[1] << 8));
			} else
				ret = -1;
			break;
		case MFC_ADC_RX_IOUT:
			ret = mfc_reg_read(charger->client, MFC_ADC_RX_IOUT_L_REG, &data[0]);
			ret = mfc_reg_read(charger->client, MFC_ADC_RX_IOUT_H_REG, &data[1]);
			if (ret >= 0) {
				ret = (data[0] | (data[1] << 8));
			} else
				ret = -1;
			break;
		case MFC_ADC_DIE_TEMP:
			/* only 4 MSB[3:0] field is used, Celsius */
			ret = mfc_reg_read(charger->client, MFC_ADC_DIE_TEMP_L_REG, &data[0]);
			ret = mfc_reg_read(charger->client, MFC_ADC_DIE_TEMP_H_REG, &data[1]);
			if (ret >= 0) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8));
			} else
				ret = -1;
			break;
		case MFC_ADC_OP_FRQ: /* kHz */
			ret = mfc_reg_read(charger->client, MFC_RX_OP_FREQ_L_REG, &data[0]);
			ret = mfc_reg_read(charger->client, MFC_RX_OP_FREQ_H_REG, &data[1]);
			if (ret >= 0) {
				ret = (data[0] | (data[1] << 8));
			} else
				ret = -1;
			break;
		case MFC_ADC_PING_FRQ:
			ret = mfc_reg_read(charger->client, MFC_RX_PING_FREQ_L_REG, &data[0]);
			ret = mfc_reg_read(charger->client, MFC_RX_PING_FREQ_H_REG, &data[0]);
			if (ret >= 0) {
				ret = (data[0] | (data[1] << 8));
			} else
				ret = -1;
		default:
			break;
	}

	return ret;
}

void mfc_set_vout(struct mfc_charger_data *charger, int vout)
{
	switch (vout) {
	case MFC_VOUT_5V:
	case MFC_VOUT_5_5V:
	case MFC_VOUT_6V:
	case MFC_VOUT_7V:
	case MFC_VOUT_8V:
	case MFC_VOUT_9V:
	case MFC_VOUT_10V:
		if (charger->chip_id == MFC_CHIP_IDT)
			mfc_reg_write(charger->client, MFC_VOUT_SET_REG, mfc_idt_vout_val[vout]);
		else /* LSI */
			mfc_reg_write(charger->client, MFC_VOUT_SET_REG, mfc_lsi_vout_val[vout]);
		msleep(100);
		break;
	default:
		break;
	}

	pr_info("%s vout(%d) read = %d mV\n", __func__, vout, mfc_get_adc(charger, MFC_ADC_VOUT));
	charger->pdata->vout_status = vout;
}

int mfc_get_vout(struct mfc_charger_data *charger)
{
	u8 data;
	int ret;
	ret = mfc_reg_read(charger->client, MFC_VOUT_SET_REG, &data);
	if (ret < 0) {
		pr_err("%s: fail to read vout. (%d)\n", __func__, ret);
		return ret;
	} else
		pr_info("%s: vout(0x%x)\n", __func__, data);

	return data;
}
void mfc_rpp_set(struct mfc_charger_data *charger)
{
	u8 data;
	int ret;

	if (charger->led_cover) {
		pr_info("%s: LED cover exists. RPP 3/4 (0x%x)\n", __func__, charger->pdata->wc_cover_rpp);
		mfc_reg_write(charger->client, MFC_RPP_SCALE_COEF_REG, charger->pdata->wc_cover_rpp);
	} else {
		pr_info("%s: LED cover not exists. RPP 1/2 (0x%x)\n", __func__, charger->pdata->wc_hv_rpp);
		mfc_reg_write(charger->client, MFC_RPP_SCALE_COEF_REG, charger->pdata->wc_hv_rpp);
	}
	msleep(5);
	ret = mfc_reg_read(charger->client, MFC_RPP_SCALE_COEF_REG, &data);
	if (ret < 0) {
		pr_err("%s: fail to read RPP scaling coefficient. (%d)\n", __func__, ret);
	} else
		pr_info("%s: RPP scaling coefficient(0x%x)\n", __func__, data);
}

void mfc_fod_set(struct mfc_charger_data *charger)
{
	int i = 0;
	pr_info("%s\n", __func__);

	switch (charger->pdata->cable_type) {
	case MFC_PAD_A4WP:
		for (i = 0; i < MFC_NUM_FOD_REG; i++) {
			mfc_reg_write(charger->client, MFC_A4WP_FOD_0A_REG+i, charger->pdata->fod_a4wp_data[i]);
		}
		break;

	/* need to  unify WPC and PMA */
	case MFC_PAD_PMA:
		for (i = 0; i < MFC_NUM_FOD_REG; i++) {
			mfc_reg_write(charger->client, MFC_PMA_FOD_0A_REG+i, charger->pdata->fod_pma_data[i]);
		}
		break;

	case MFC_PAD_WPC:
	case MFC_PAD_WPC_AFC:
	case MFC_PAD_WPC_PACK:
	case MFC_PAD_WPC_PACK_TA:
	case MFC_PAD_WPC_STAND:
	case MFC_PAD_WPC_STAND_HV:
		for (i = 0; i < MFC_NUM_FOD_REG; i++) {
			mfc_reg_write(charger->client, MFC_WPC_FOD_0A_REG+i, charger->pdata->fod_wpc_data[i]);
		}
		break;

	case MFC_PAD_NONE:
	default:
		break;
	}

}

void mfc_fod_set_cv(struct mfc_charger_data *charger)
{
	int i = 0;

	pr_info("%s\n", __func__);
	switch (charger->pdata->cable_type) {
	case MFC_PAD_A4WP:
		for (i = 0; i < MFC_NUM_FOD_REG; i++) {
			mfc_reg_write(charger->client, MFC_A4WP_FOD_0A_REG+i, charger->pdata->fod_a4wp_data_cv[i]);
		}
		break;
	/* need to  unify WPC and PMA */
	case MFC_PAD_PMA:
		for (i = 0; i < MFC_NUM_FOD_REG; i++) {
			mfc_reg_write(charger->client, MFC_PMA_FOD_0A_REG+i, charger->pdata->fod_pma_data_cv[i]);
		}
		break;

	case MFC_PAD_WPC:
	case MFC_PAD_WPC_AFC:
	case MFC_PAD_WPC_PACK:
	case MFC_PAD_WPC_PACK_TA:
	case MFC_PAD_WPC_STAND:
	case MFC_PAD_WPC_STAND_HV:
	case MFC_PAD_NONE:
		for (i = 0; i < MFC_NUM_FOD_REG; i++) {
			mfc_reg_write(charger->client, MFC_WPC_FOD_0A_REG+i, charger->pdata->fod_wpc_data_cv[i]);
		}
		break;

	default:
		break;
	}

}

void mfc_fod_set_cs100(struct mfc_charger_data *charger)
{
	int i = 0;

	pr_info("%s\n", __func__);

	switch (charger->pdata->cable_type) {
	case MFC_PAD_A4WP:
		for (i = 0; i < MFC_NUM_FOD_REG; i++)
			mfc_reg_write(charger->client, MFC_A4WP_FOD_0A_REG+i, 0x7f);
		break;
	/* need to  unify WPC and PMA */
	case MFC_PAD_PMA:
		for (i = 0; i < MFC_NUM_FOD_REG; i++)
			mfc_reg_write(charger->client, MFC_PMA_FOD_0A_REG+i, 0x7f);
		break;
	case MFC_PAD_WPC:
	case MFC_PAD_WPC_AFC:
	case MFC_PAD_WPC_PACK:
	case MFC_PAD_WPC_PACK_TA:
	case MFC_PAD_WPC_STAND:
	case MFC_PAD_WPC_STAND_HV:
	case MFC_PAD_NONE:
		for (i = 0; i < MFC_NUM_FOD_REG; i++)
			mfc_reg_write(charger->client, MFC_WPC_FOD_0A_REG+i, 0x7f);
		break;
	default:
		break;
	}

}

void mfc_fod_set_hero_5v(struct mfc_charger_data *charger)
{
	int i = 0;
	u8 fod[12] = {0, };

	pr_info("%s\n", __func__);

	if (charger->pdata->fod_hero_5v_data) {
		for (i = 0; i < MFC_NUM_FOD_REG; i++)
			mfc_reg_write(charger->client, MFC_WPC_FOD_0A_REG+i, charger->pdata->fod_hero_5v_data[i]);
		msleep(2);
		for (i = 0; i < MFC_NUM_FOD_REG; i++)
			mfc_reg_read(charger->client, MFC_WPC_FOD_0A_REG+i, &fod[i]);

		pr_info("%s: HERO 5V FOD(%d %d %d %d %d %d %d %d %d %d %d %d)\n", __func__,
			fod[0], fod[1], fod[2], fod[3], fod[4], fod[5], fod[6], fod[7], fod[8], fod[9], fod[10], fod[11]);
	}
}

void mfc_set_cmd_l_reg(struct mfc_charger_data *charger, u8 val, u8 mask)
{
	u8 temp = 0;
	int ret = 0, i = 0;

	do {
		pr_info("%s\n", __func__);
		ret = mfc_reg_update(charger->client, MFC_AP2MFC_CMD_L_REG, val, mask); // command
		if (ret >= 0) {
			ret = mfc_reg_read(charger->client, MFC_AP2MFC_CMD_L_REG, &temp); // check out set bit exists
			if (ret < 0 || i > 3)
				break;
		}
		i++;
	} while ((temp != 0) && (i < 3));
}

void mfc_set_cmd_h_reg(struct mfc_charger_data *charger, u8 val, u8 mask)
{
	u8 temp = 0;
	int ret = 0, i = 0;

	do {
		pr_info("%s\n", __func__);
		ret = mfc_reg_update(charger->client, MFC_AP2MFC_CMD_H_REG, val, mask); // command
		if (ret >= 0) {
			msleep(250);
			ret = mfc_reg_read(charger->client, MFC_AP2MFC_CMD_H_REG, &temp); // check out set bit exists
			if (ret < 0 || i > 3)
				break;
		}
		i++;
	} while ((temp != 0) && (i < 3));
}

void mfc_send_eop(struct mfc_charger_data *charger, int health_mode)
{
	int i = 0;
	int ret = 0;

	pr_info("%s: health_mode (0x%x)\n", __func__, health_mode);
	switch(health_mode) {
	case POWER_SUPPLY_HEALTH_OVERHEAT:
	case POWER_SUPPLY_HEALTH_OVERHEATLIMIT:
	case POWER_SUPPLY_HEALTH_COLD:
		if (charger->pdata->cable_type == MFC_PAD_PMA) {
			pr_info("%s pma mode\n", __func__);
			for (i = 0; i < CMD_CNT; i++) {
				ret = mfc_reg_write(charger->client, MFC_EPT_REG, MFC_WPC_EPT_END_OF_CHG);
				if (ret >= 0) {
					mfc_set_cmd_l_reg(charger, MFC_CMD_SEND_EOP_MASK, MFC_CMD_SEND_EOP_MASK);
					msleep(250);
				} else
					break;
			}
		} else if (charger->pdata->cable_type == MFC_PAD_A4WP) {
			pr_info("%s a4wp mode\n", __func__);
		} else {
			pr_info("%s wpc mode\n", __func__);
			for (i = 0; i < CMD_CNT; i++) {
				ret = mfc_reg_write(charger->client, MFC_EPT_REG, MFC_WPC_EPT_OVER_TEMP);
				if (ret >= 0) {
					mfc_set_cmd_l_reg(charger, MFC_CMD_SEND_EOP_MASK, MFC_CMD_SEND_EOP_MASK);
					msleep(250);
				} else
					break;
			}
		}
		break;
	case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
		break;
	default:
		break;
	}
}

void mfc_send_packet(struct mfc_charger_data *charger, u8 header, u8 rx_data_com, u8 *data_val, int data_size)
{
	int i;

	if (charger->pdata->cable_type == MFC_PAD_A4WP) {
		/* set AP2BT COM, and VALUE, and trigger INT_B */
		mfc_reg_write(charger->client, MFC_AP2BT_DATA_COM_REG, rx_data_com);
		for (i = 0; i < data_size; i++) {
			mfc_reg_write(charger->client, MFC_AP2BT_DATA_VALUE0_REG+ i, data_val[i]);
		}
		mfc_set_cmd_l_reg(charger, MFC_CMD_AP2BT_DATA_MASK, MFC_CMD_AP2BT_DATA_MASK);
	} else {
		mfc_reg_write(charger->client, MFC_WPC_PCKT_HEADER_REG, header);
		mfc_reg_write(charger->client, MFC_WPC_RX_DATA_COM_REG, rx_data_com);

		for (i = 0; i < data_size; i++) {
			mfc_reg_write(charger->client, MFC_WPC_RX_DATA_VALUE0_REG+ i, data_val[i]);
		}
		mfc_set_cmd_l_reg(charger, MFC_CMD_SEND_RX_DATA_MASK, MFC_CMD_SEND_RX_DATA_MASK);
	}
}

int mfc_send_cs100(struct mfc_charger_data *charger)
{
	int i = 0;
	int ret = 0;
	u8 data_val[2];

	if (charger->pdata->cable_type == MFC_PAD_A4WP) {
		data_val[0] = 0x80; /* charge complete */
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF, AP2BT_COM_CHG_STATUS, data_val, 1);
	}
	for (i = 0; i < CMD_CNT; i++) {
		ret = mfc_reg_write(charger->client, MFC_BATTERY_CHG_STATUS_REG, 100);
		if (ret >= 0) {
			mfc_set_cmd_l_reg(charger, MFC_CMD_SEND_CHG_STS_MASK, MFC_CMD_SEND_CHG_STS_MASK);
			msleep(250);
			ret = 1;
		} else {
			ret = -1;
			break;
		}
	}
	return ret;
}

void mfc_send_command(struct mfc_charger_data *charger, int cmd_mode)
{
	u8 data_val[4];
	u8 cmd = 0;

	switch (cmd_mode) {
	case MFC_AFC_CONF_5V:
		if (charger->pdata->cable_type == MFC_PAD_A4WP) {
			pr_info("%s: A4WP set 5V\n", __func__);
			cmd = AP2BT_COM_AFC_MODE;
			data_val[0] = 0x00; /* Value for A4WP AFC_SET 5V */
			mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
			msleep(120);

			charger->vout_mode = WIRELESS_VOUT_5V;
			cancel_delayed_work(&charger->wpc_vout_mode_work);
			wake_lock(&charger->wpc_vout_mode_lock);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_vout_mode_work, 0);
			pr_info("%s vout read = %d\n", __func__,  mfc_get_adc(charger, MFC_ADC_VOUT));

			mfc_reg_read(charger->client, MFC_AP2BT_DATA_COM_REG, &data_val[0]);
			mfc_reg_read(charger->client, MFC_AP2BT_DATA_VALUE0_REG, &data_val[0]);
			mfc_reg_read(charger->client, MFC_AP2MFC_CMD_L_REG, &data_val[0]);
		} else {
			pr_info("%s: WPC/PMA set 5V\n", __func__);
			cmd = WPC_COM_AFC_SET;
			data_val[0] = 0x05; /* Value for WPC AFC_SET 5V */
			mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
			msleep(120);

			charger->vout_mode = WIRELESS_VOUT_5V;
			cancel_delayed_work(&charger->wpc_vout_mode_work);
			wake_lock(&charger->wpc_vout_mode_lock);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_vout_mode_work, 0);
			pr_info("%s vout read = %d\n", __func__,  mfc_get_adc(charger, MFC_ADC_VOUT));

			mfc_reg_read(charger->client, MFC_WPC_RX_DATA_COM_REG, &data_val[0]);
			mfc_reg_read(charger->client, MFC_WPC_RX_DATA_VALUE0_REG, &data_val[0]);
			mfc_reg_read(charger->client, MFC_AP2MFC_CMD_L_REG, &data_val[0]);
		}
		break;
	case MFC_AFC_CONF_10V:
		if (charger->pdata->cable_type == MFC_PAD_A4WP) { /* PAD : A4WP */
			pr_info("%s: A4WP set 10V\n", __func__);
			cmd = AP2BT_COM_AFC_MODE;
			data_val[0] = 0x01; /* Value for A4WP AFC_SET 10V */
			mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
			msleep(120);

			charger->vout_mode = WIRELESS_VOUT_10V;
			cancel_delayed_work(&charger->wpc_vout_mode_work);
			wake_lock(&charger->wpc_vout_mode_lock);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_vout_mode_work, 0);
			pr_info("%s vout read = %d\n", __func__,  mfc_get_adc(charger, MFC_ADC_VOUT));

			mfc_reg_read(charger->client, MFC_AP2BT_DATA_COM_REG, &data_val[0]);
			mfc_reg_read(charger->client, MFC_AP2BT_DATA_VALUE0_REG, &data_val[0]);
			mfc_reg_read(charger->client, MFC_AP2MFC_CMD_L_REG, &data_val[0]);
		} else { /* PAD : WPC, PMA */
			pr_info("%s: WPC set 10V\n", __func__);
			//trigger 10V vout work after 8sec
			wake_lock(&charger->wpc_afc_vout_lock);
#if defined(CONFIG_SEC_FACTORY)
			queue_delayed_work(charger->wqueue, &charger->wpc_afc_vout_work, msecs_to_jiffies(0));
#else
			queue_delayed_work(charger->wqueue, &charger->wpc_afc_vout_work, msecs_to_jiffies(8000));
#endif
		}
		break;
	case MFC_LED_CONTROL_ON:
		pr_info("%s led on\n", __func__);
		if (charger->pdata->cable_type == MFC_PAD_A4WP) {
			cmd = AP2BT_COM_LED_CONTROL;
			data_val[0] = 0x00; /* Value for A4WP LED ON */
		} else {
			cmd = WPC_COM_LED_CONTROL;
			data_val[0] = 0x00; /* Value for WPC LED ON */
		}
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
		break;
	case MFC_LED_CONTROL_OFF:
		pr_info("%s led off\n", __func__);
		if (charger->pdata->cable_type == MFC_PAD_A4WP) {
			cmd = AP2BT_COM_LED_CONTROL;
			data_val[0] = 0xff; /* Value for A4WP LED OFF */
		} else {
			cmd = WPC_COM_LED_CONTROL;
			data_val[0] = 0xff; /* Value for WPC LED OFF */
		}
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
		break;
	case MFC_FAN_CONTROL_ON:
		pr_info("%s fan on\n", __func__);
		if (charger->pdata->cable_type == MFC_PAD_A4WP) {
			cmd = AP2BT_COM_COOLING_CTRL;
			data_val[0] = 0x00; /* Value for A4WP FAN ON */
		} else {
			cmd = WPC_COM_COOLING_CTRL;
			data_val[0] = 0x00; /* Value for WPC FAN ON */
		}
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
		break;
	case MFC_FAN_CONTROL_OFF:
		pr_info("%s fan off\n", __func__);
		if (charger->pdata->cable_type == MFC_PAD_A4WP) {
			cmd = AP2BT_COM_COOLING_CTRL;
			data_val[0] = 0xff; /* Value for A4WP FAN OFF */
		} else {
			cmd = WPC_COM_COOLING_CTRL;
			data_val[0] = 0xff; /* Value for WPC FAN OFF */
		}
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
		break;
	case MFC_REQUEST_AFC_TX:
		pr_info("%s request afc tx, cable(0x%x)\n", __func__, charger->pdata->cable_type);
		if (charger->pdata->cable_type == MFC_PAD_A4WP) {
			cmd = AP2BT_COM_REQ_AFC_TX;
			data_val[0] = 0x00; /* Value for A4WP Request AFC_TX */
		} else {
			cmd = WPC_COM_REQ_AFC_TX;
			data_val[0] = 0x00; /* Value for WPC Request AFC_TX */
		}
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
		break;
	case MFC_REQUEST_TX_ID:
		pr_info("%s request TX ID\n", __func__);
		if (charger->pdata->cable_type == MFC_PAD_A4WP) {
			cmd = AP2BT_COM_TX_ID;
			data_val[0] = 0x00; /* Value for A4WP TX ID */
		} else {
			cmd = WPC_COM_TX_ID;
			data_val[0] = 0x00; /* Value for WPC TX ID */
		}
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
		break;
	default:
		break;
	}
}

void mfc_led_control(struct mfc_charger_data *charger, bool on)
{
	int i = 0;

	if (on) {
		for (i = 0; i < CMD_CNT; i++)
			mfc_send_command(charger, MFC_LED_CONTROL_ON);
	} else {
		for (i = 0; i < CMD_CNT; i++)
			mfc_send_command(charger, MFC_LED_CONTROL_OFF);
	}
}

void mfc_fan_control(struct mfc_charger_data *charger, bool on)
{
	int i = 0;

	if (on) {
		for (i = 0; i < CMD_CNT - 1; i++)
			mfc_send_command(charger, MFC_FAN_CONTROL_ON);
	} else {
		for (i = 0; i < CMD_CNT - 1; i++)
			mfc_send_command(charger, MFC_FAN_CONTROL_OFF);
	}
}

void mfc_set_vrect_adjust(struct mfc_charger_data *charger, int set)
{
	int i = 0;

	switch (set) {
	case MFC_HEADROOM_0:
		for (i = 0; i < 6; i++) {
			mfc_reg_write(charger->client, MFC_VRECT_ADJ_REG, 0x0);
			msleep(50);
		}
		break;
	case MFC_HEADROOM_1:
		for (i = 0; i < 6; i++) {
			mfc_reg_write(charger->client, MFC_VRECT_ADJ_REG, 0x36);
			msleep(50);
		}
		break;
	case MFC_HEADROOM_2:
		for (i = 0; i < 6; i++) {
			mfc_reg_write(charger->client, MFC_VRECT_ADJ_REG, 0x61);
			msleep(50);
		}
		break;
	case MFC_HEADROOM_3:
		for (i = 0; i < 6; i++) {
			mfc_reg_write(charger->client, MFC_VRECT_ADJ_REG, 0x7f);
			msleep(50);
		}
		break;
	case MFC_HEADROOM_4:
		for (i = 0; i < 6; i++) {
			mfc_reg_write(charger->client, MFC_VRECT_ADJ_REG, 0x06);
			msleep(50);
		}
		break;
	case MFC_HEADROOM_5:
		for (i = 0; i < 6; i++) {
			mfc_reg_write(charger->client, MFC_VRECT_ADJ_REG, 0x10);
			msleep(50);
		}
		break;
	default:
		pr_info("%s no headroom mode\n", __func__);
		break;
	}
}

void mfc_mis_align(struct mfc_charger_data *charger)
{
	pr_info("%s: Reset M0\n", __func__);
	if (charger->pdata->cable_type == MFC_PAD_WPC_AFC ||
		charger->pdata->cable_type == MFC_PAD_PMA)
		/* reset MCU of MFC IC */
		mfc_set_cmd_l_reg(charger, MFC_CMD_MCU_RESET_MASK, MFC_CMD_MCU_RESET_MASK);
}

static int datacmp(const char *cs, const char *ct, int count)
{
	unsigned char c1, c2;

	while (count) {
		c1 = *cs++;
		c2 = *ct++;
		if (c1 != c2) {
			pr_err("%s, cnt %d\n", __func__, count);
			return c1 < c2 ? -1 : 1;
		}
		count--;
	}
	return 0;
}

static int mfc_reg_multi_write_verify(struct i2c_client *client, u16 reg, const u8 * val, int size)
{
	int ret = 0;
	const int sendsz = 16;
	int cnt = 0;
	int retry_cnt = 0;
	unsigned char data[sendsz+2];
	u8 rdata[sendsz+2];

//	dev_dbg(&client->dev, "%s: size: 0x%x\n", __func__, size);
	while(size > sendsz) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, sendsz);
//		dev_dbg(&client->dev, "%s: addr: 0x%x, cnt: 0x%x\n", __func__, reg+cnt, cnt);
		ret = i2c_master_send(client, data, sendsz+2);
		if (ret < sendsz+2) {
			pr_err("%s: i2c write error, reg: 0x%x\n", __func__, reg);
			return ret < 0 ? ret : -EIO;
		}
		if (mfc_reg_multi_read(client, reg+cnt, rdata, sendsz) < 0) {
			pr_err("%s, read failed(%d)\n", __func__, reg+cnt);
			return -1;
		}
		if (datacmp(val+cnt, rdata, sendsz)) {
			pr_err("%s, data is not matched. retry(%d)\n", __func__, retry_cnt);
			retry_cnt++;
			if (retry_cnt > 4) {
				pr_err("%s, data is not matched. write failed\n", __func__);
				retry_cnt = 0;
				return -1;
			}
			continue;
		}
//		pr_debug("%s, data is matched!\n", __func__);
		cnt += sendsz;
		size -= sendsz;
		retry_cnt = 0;
	}
	while (size > 0) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, size);
//		dev_dbg(&client->dev, "%s: addr: 0x%x, cnt: 0x%x, size: 0x%x\n", __func__, reg+cnt, cnt, size);
		ret = i2c_master_send(client, data, size+2);
		if (ret < size+2) {
			pr_err("%s: i2c write error, reg: 0x%x\n", __func__, reg);
			return ret < 0 ? ret : -EIO;
		}
		if (mfc_reg_multi_read(client, reg+cnt, rdata, size) < 0) {
			pr_err("%s, read failed(%d)\n", __func__, reg+cnt);
			return -1;
		}
		if (datacmp(val+cnt, rdata, size)) {
			pr_err("%s, data is not matched. retry(%d)\n", __func__, retry_cnt);
			retry_cnt++;
			if (retry_cnt > 4) {
				pr_err("%s, data is not matched. write failed\n", __func__);
				retry_cnt = 0;
				return -1;
			}
			continue;
		}
		size = 0;
		pr_err("%s, data is matched!\n", __func__);
	}
	return ret;
}

static int mfc_reg_multi_write(struct i2c_client *client, u16 reg, const u8 * val, int size)
{
	struct mfc_charger_data *charger = i2c_get_clientdata(client);
	int ret = 0;
	const int sendsz = 16;
	unsigned char data[sendsz+2];
	int cnt = 0;

	pr_err("%s: size: 0x%x\n",
				__func__, size);
	while(size > sendsz) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, sendsz);
		mutex_lock(&charger->io_lock);
		ret = i2c_master_send(client, data, sendsz+2);
		mutex_unlock(&charger->io_lock);
		if (ret < sendsz+2) {
			pr_err("%s: i2c write error, reg: 0x%x\n",
					__func__, reg);
			return ret < 0 ? ret : -EIO;
		}
		cnt = cnt + sendsz;
		size = size - sendsz;
	}
	if (size > 0) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, size);
		mutex_lock(&charger->io_lock);
		ret = i2c_master_send(client, data, size+2);
		mutex_unlock(&charger->io_lock);
		if (ret < size+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n",
					__func__, reg);
			return ret < 0 ? ret : -EIO;
		}
	}

	return ret;
}

static int LoadOTPLoaderInRAM(struct mfc_charger_data *charger, u16 addr)
{
	int i, size;
	u8 data[1024];
	if (mfc_reg_multi_write_verify(charger->client, addr, MTPBootloader9320, sizeof(MTPBootloader9320)) < 0) {
		pr_err("%s,fail", __func__);
	}
	size = sizeof(MTPBootloader9320);
	i = 0;
	while(size > 0) {
		if (mfc_reg_multi_read(charger->client, addr+i, data+i, 16) < 0) {
			pr_err("%s, read failed(%d)", __func__, addr+i);
			return 0;
		}
		i += 16;
		size -= 16;
	}
	size = sizeof(MTPBootloader9320);
	if (datacmp(data, MTPBootloader9320, size)) {
		pr_err("%s, data is not matched\n", __func__);
		return 0;
	}
	return 1;
}

static int mfc_firmware_verify(struct mfc_charger_data *charger)
{
	int ret = 0;
	const u16 sendsz = 16;
	size_t i = 0;
	int block_len = 0;
	int block_addr = 0;
	u8 rdata[sendsz+2];

	mutex_lock(&charger->fw_lock);

	/* I2C WR to prepare boot-loader write */
	if (mfc_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: key error\n", __func__);
		goto skip_fw_verify;
	}

	if (mfc_reg_write(charger->client, 0x3040, 0x11) < 0) {
		pr_err("%s: halt M0, OTP_I2C_EN set error\n", __func__);
		goto skip_fw_verify;
	}

	dev_err(&charger->client->dev, "%s, request_firmware\n", __func__);
	ret = request_firmware(&charger->firm_data_bin, MFC_FLASH_FW_HEX_PATH,
		&charger->client->dev);
	if ( ret < 0) {
		dev_err(&charger->client->dev, "%s: failed to request firmware %s (%d)\n",
				__func__, MFC_FLASH_FW_HEX_PATH, ret);
		goto skip_fw_verify;
	}
	ret = 1;
	wake_lock(&charger->wpc_update_lock);
	for (i = 0; i < charger->firm_data_bin->size; i += sendsz) {
		block_len = (i + sendsz) > charger->firm_data_bin->size ? charger->firm_data_bin->size - i : sendsz;
		block_addr = 0x8000 + i;

		if (mfc_reg_multi_read(charger->client, block_addr, rdata, block_len) < 0) {
			pr_err("%s, read failed\n", __func__);
			ret = 0;
			break;
		}
		if (datacmp(charger->firm_data_bin->data + i, rdata, block_len)) {
			pr_err("%s, verify data is not matched. block_len(%d), block_addr(%d)\n",
				__func__, block_len, block_addr);
			ret = -1;
			break;
		}
	}
	release_firmware(charger->firm_data_bin);
	wake_unlock(&charger->wpc_update_lock);

skip_fw_verify:
	mutex_unlock(&charger->fw_lock);
	return ret;
}

bool WriteWordToMtp(struct mfc_charger_data *charger, u16 StartAddr, u32 data)
{
	int j, cnt;
	u8 sBuf[16] = {0,};
	u16 CheckSum = StartAddr;
	u16 CodeLength = 4;
	//*(u32*)&sBuf[8] = data;
	sBuf[8] = (u8)(data >> 0);
	sBuf[9] = (u8)(data >> 8);
	sBuf[10] = (u8)(data >> 16);
	sBuf[11] = (u8)(data >> 24);

	pr_info("%s: changed sBuf codes\n", __func__);
	for (j = 3; j >= 0; j--)
		CheckSum += sBuf[j + 8];	// add the non zero values
	CheckSum += CodeLength;			// finish calculation of the check sum
	//*(u16*)&sBuf[2] = StartAddr;
	//*(u16*)&sBuf[4] = CodeLength;
	//*(u16*)&sBuf[6] = CheckSum;
	sBuf[2] = (u8)(StartAddr >> 0);
	sBuf[3] = (u8)(StartAddr >> 8);
	sBuf[4] = (u8)(CodeLength >> 0);
	sBuf[5] = (u8)(CodeLength >> 8);
	sBuf[6] = (u8)(CheckSum >> 0);
	sBuf[7] = (u8)(CheckSum >> 8);

	if (mfc_reg_multi_write(charger->client, 0x400, sBuf, 4 + 8) < 0)
	{
		pr_err("ERROR: on writing to OTP buffer");
		return false;
	}

	sBuf[0] = 0x01;
	if (mfc_reg_write(charger->client, 0x400, sBuf[0]) < 0)
	{
		pr_err("ERROR: on OTP buffer validation");
		return false;
	}

	cnt = 0;
	do {
		msleep(20);
		if (mfc_reg_read(charger->client, 0x400, sBuf) < 0)
		{
			pr_err("ERROR: on readign OTP buffer status(%d)", cnt);
			return false;
		}

		if (cnt > 1000) {
			pr_err("ERROR: time out on buffer program to OTP");
			break;
		}
		cnt++;
	} while ((sBuf[0]&1) != 0);

	if (sBuf[0] != 2) // not OK
	{
		pr_err("ERROR: buffer write to OTP returned status %d ",sBuf[0]);
		return false;
	}
	return true;
}

static int PgmOTPwRAM_IDT(struct mfc_charger_data *charger, unsigned short OtpAddr,
					  const u8 * srcData, int srcOffs, int size)
{
	int i, cnt;
	u8 fw_major[2] = {0,};
	u8 fw_minor[2] = {0,};

	mfc_reg_read(charger->client, MFC_FW_MAJOR_REV_L_REG, &fw_major[0]);
	mfc_reg_read(charger->client, MFC_FW_MAJOR_REV_H_REG, &fw_major[1]);
	mfc_reg_read(charger->client, MFC_FW_MINOR_REV_L_REG, &fw_minor[0]);
	mfc_reg_read(charger->client, MFC_FW_MINOR_REV_H_REG, &fw_minor[1]);

	msleep(10);
	if (mfc_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: write key error\n", __func__);
		return false;		// write key
	}
	msleep(10);
	if (mfc_reg_write(charger->client, 0x3040, 0x10) < 0) {
		pr_err("%s: halt M0 error\n", __func__);
		return false;		// halt M0
	}
	msleep(10);
	if (!LoadOTPLoaderInRAM(charger, 0x1c00)){
		pr_err("%s: LoadOTPLoaderInRAM error\n", __func__);
		return false;		// make sure load address and 1KB size are OK
	}
	msleep(10);

	// Clear MTP program status byte
	if (mfc_reg_write(charger->client, 0x0400, 0x00) < 0) {
			pr_err("%s: clear MTP programming status byte error\n", __func__);
		   return false;
	}

	if (mfc_reg_write(charger->client, 0x3048, 0x80) < 0) {
		pr_err("%s: map RAM to OTP error\n", __func__);
		return false;		// map RAM to OTP
	}

	// Check Key lock state
	mfc_reg_write(charger->client, 0x3040, 0x80); //M0 RESET : P9320 will not acknowledge for this transaction !!
	msleep(100);

	pr_info("%s: start to write f/w bin to mtp\n", __func__);
	for (i = 0; i < size; i += 128)	// program pages of 128 bytes
	{
		u8 sBuf[144] = {0,};	// align size in 16 bytes boundary. may not be important for SS
		u16 StartAddr = (u16)i;
		u16 CheckSum = StartAddr;
		u16 CodeLength = 128;
		int j;
		memcpy(sBuf + 8, srcData + i + srcOffs, 128);

		if (i == 0x1480) { // the FW rev address for rev 58 is 0x14a8. 0x1280 is the half page base address.
			//*(u32*)&sBuf[8 + 0x28] = 0;
			sBuf[0x30] = 0;
			sBuf[0x31] = 0;
			sBuf[0x32] = 0;
			sBuf[0x33] = 0;
		}

		j = size - i;	// calculate how many bytes need to be programmed in the current run and round up to 16

		if (j < 128)
		{
			j = ((j + 15) / 16) * 16;
			CodeLength = (u16)j;
		}
		else
		{
			j = 128;
		}
		j -= 1;	// compensate for index

		for (; j >= 0; j--)
			CheckSum += sBuf[j+8];	// add the non zero values
		CheckSum += CodeLength;		// finish calculation of the check sum
		memcpy(sBuf+2, &StartAddr, 2);
		memcpy(sBuf+4, &CodeLength, 2);
		memcpy(sBuf+6, &CheckSum, 2);

		// FOR REFERENCE HERE IS THE DATA STRUCTURE
		//typedef struct {
		// 		   u16 Status;
		// 		   u16 StartAddr;
		// 		   u16 CodeLength;
		// 		   u16 DataChksum;
		// 		   u8  DataBuf[128];
		//}  P9320PgmStrType;	 // the structure is located at address 0x400

		// TODO sBuf[0] = 0x00; // done during initialization.

		if (mfc_reg_multi_write(charger->client, 0x400, sBuf, ((CodeLength+8+15)/16)*16) < 0)
		{	// TODO the write size is aligned to 16 bytes. SS may not need to do this.
			pr_err("ERROR: on writing to OTP buffer");
			return false;
		}
		sBuf[0] = 0x01;	// TODO write 0x11 if Vrect is powered from 5V
			//write 0x31 if Vrect is powered from 8.2V
			//write 0x01 if Vrect is 5V and there is a problem

		if (mfc_reg_write(charger->client, 0x400, sBuf[0]) < 0)
		{
			pr_err("ERROR: on OTP buffer validation");
			return false;
		}
		cnt = 0;
		do
		{
			msleep(20);
			if (mfc_reg_read(charger->client, 0x400, sBuf) < 0)
			{
				pr_err("ERROR: on readign OTP buffer status(%d)", cnt);
				return false;
			}
			if (cnt > 1000) {
				pr_err("ERROR: time out on buffer program to OTP");
				break;
			}
			cnt++;
		} while ((sBuf[0]&1) != 0);
		if (sBuf[0] != 2) // not OK
		{
			pr_err("ERROR: buffer write to OTP returned status %d in sector 0x%x ",sBuf[0], i);
			return false;
		}
	}

	pr_info("%s: write current f/w rev (0x%x)\n", __func__, MFC_FW_BIN_FULL_VERSION);
	if (!WriteWordToMtp(charger, MFC_FW_BIN_VERSION_ADDR, MFC_FW_BIN_FULL_VERSION)) {
			// The address for fw rev 58 is 0x14a8
		  pr_err("ERROR: on writing FW rev to MTP\n");
		  return false;
	}

	msleep(10);
	if (mfc_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: write key error..\n", __func__);
		return false;		// write key
	}

	msleep(10);
	if (mfc_reg_write(charger->client, 0x3048, 0x00) < 0) {
		pr_err("%s: remove code remapping error..\n", __func__);
		return false;		// remove code remapping
	}

	pr_err("OTP Programming finished in");
	pr_info("%s-------------------------------------------------\n", __func__);
	return true;
}

static int mfc_write_fw_flash_LSI(struct mfc_charger_data *charger, u8 addr_l, u8 addr_h, u8 *wdata)
{
	if (mfc_reg_write(charger->client, 0x1F11, 0x04) < 0) {
		pr_err("%s: failed to write 0x04 at 0x1F11\n", __func__);
		return -1;
	}
	if (mfc_reg_write(charger->client, 0x1F12, addr_l) < 0) {
		pr_err("%s: failed to write addr_l(0x%x) at 0x1F11\n", __func__, addr_l);
		return -1;
	}
	if (mfc_reg_write(charger->client, 0x1F13, addr_h) < 0) {
		pr_err("%s: failed to write addr_h(0x%x) at 0x1F11\n", __func__, addr_h);
		return -1;
	}
	if (mfc_reg_write(charger->client, 0x1F14, wdata[0]) < 0) {
		pr_err("%s: failed to write wdata[0]\n", __func__);
		return -1;
	}
	if (mfc_reg_write(charger->client, 0x1F15, wdata[1]) < 0) {
		pr_err("%s: failed to write wdata[1]\n", __func__);
		return -1;
	}
	if (mfc_reg_write(charger->client, 0x1F16, wdata[2]) < 0) {
		pr_err("%s: failed to write wdata[2]\n", __func__);
		return -1;
	}
	if (mfc_reg_write(charger->client, 0x1F17, wdata[3]) < 0) {
		pr_err("%s: failed to write wdata[3]\n", __func__);
		return -1;
	}
	if (mfc_reg_write(charger->client, 0x1F10, 0x42) < 0) {
		pr_err("%s: failed to write 0x42 at 0x1F10\n", __func__);
		return -1;
	}

	return 0;
}

#define LSI_MFC_FW_FLASH_START_ADDR		0x1000
static int PgmOTPwRAM_LSI(struct mfc_charger_data *charger, unsigned short OtpAddr,
					  const u8 * srcData, int srcOffs, int size)
{
	int addr;
	u8 fw_major[2] = {0,};
	u8 fw_minor[2] = {0,};
	u8 wdata[4] = {0,};
//	static int startAddr;
	u16 temp;
	u8 addr_l, addr_h;

	mfc_reg_read(charger->client, MFC_FW_MAJOR_REV_L_REG, &fw_major[0]);
	mfc_reg_read(charger->client, MFC_FW_MAJOR_REV_H_REG, &fw_major[1]);
	mfc_reg_read(charger->client, MFC_FW_MINOR_REV_L_REG, &fw_minor[0]);
	mfc_reg_read(charger->client, MFC_FW_MINOR_REV_H_REG, &fw_minor[1]);

	pr_info("%s: Enter the flash mode (0x1F10)\n", __func__);
	if (mfc_reg_write(charger->client, 0x1F10, 0x10) < 0) {
		pr_err("%s: failed to enter the flash mode\n", __func__);
		return false;
	}
	msleep(2);
	pr_info("%s: Erase the flash memory\n", __func__);
	if (mfc_reg_write(charger->client, 0x1F10, 0x44) < 0) {
		pr_err("%s: failed to erase flash\n", __func__);
		return false;
	}

	msleep(250); /* erasing flash needs 200ms delay at least */

	pr_info("%s: write fwimg by 4 bytes\n", __func__);
	size -= 0x1000;
	for (addr = 0x00; addr < size; addr += 4)	// program pages of 4bytes
	{
		temp = (0x1000 + addr) & 0xff;
		addr_l = (u8)temp;
		temp = (((0x1000 + addr) & 0xff00) >> 8);
		addr_h = (u8)temp;
		pr_info("%s: addr_h(0x%x), addr_l(0x%x)\n", __func__, addr_h, addr_l);
		memcpy(wdata, srcData + LSI_MFC_FW_FLASH_START_ADDR + addr, 4);

		mfc_write_fw_flash_LSI(charger, addr_l, addr_h, wdata);
	}

	pr_info("%s: write fw length --------------------\n", __func__);
	wdata[0] = 0x8C; /* length */
	wdata[1] = 0x3E;
	wdata[2] = 0x00;
	wdata[3] = 0x00;
	mfc_write_fw_flash_LSI(charger, 0xf4, 0x6f, wdata);

	pr_info("%s: write fw checksum --------------------\n", __func__);
	wdata[0] = 0x90; /* checksum */
	wdata[1] = 0x00;
	wdata[2] = 0x00;
	wdata[3] = 0x00;
	mfc_write_fw_flash_LSI(charger, 0xf8, 0x6f, wdata);

	pr_info("%s: write fw version --------------------\n", __func__);
	wdata[0] = 0x00; /* fw major rev l */
	wdata[1] = 0x00; /* fw major rev h */
	wdata[2] = 0x05; /* fw minor rev l */
	wdata[3] = 0x11; /* fw minor rev h */
	mfc_write_fw_flash_LSI(charger, 0x00, 0x6f, wdata);

	pr_info("%s: write fw date and timer code --------------------\n", __func__);
	wdata[0] = 0x4A; /* J , date code start */
	wdata[1] = 0x75; /* u */
	wdata[2] = 0x6E; /* n */
	wdata[3] = 0x20; /* space */
	mfc_write_fw_flash_LSI(charger, 0x04, 0x6f, wdata);
	wdata[0] = 0x31; /* 1 */
	wdata[1] = 0x35; /* 5 */
	wdata[2] = 0x20; /* space */
	wdata[3] = 0x32; /* 2 */
	mfc_write_fw_flash_LSI(charger, 0x08, 0x6f, wdata);
	wdata[0] = 0x30; /* 0 */
	wdata[1] = 0x31; /* 1 */
	wdata[2] = 0x36; /* 6 */
	wdata[3] = 0x31; /* 1 , timer code start */
	mfc_write_fw_flash_LSI(charger, 0x0c, 0x6f, wdata);
	wdata[0] = 0x33; /* 3 */
	wdata[1] = 0x3A; /* : */
	wdata[2] = 0x30; /* 0 */
	wdata[3] = 0x30; /* 0 */
	mfc_write_fw_flash_LSI(charger, 0x10, 0x6f, wdata);
	wdata[0] = 0x3A; /* : */
	wdata[1] = 0x30; /* 0 */
	wdata[2] = 0x30; /* 0 */
	wdata[3] = 0x00; /* reserved */
	mfc_write_fw_flash_LSI(charger, 0x14, 0x6f, wdata);

	pr_info("%s: write flash done flag --------------------*\n", __func__);
	wdata[0] = 0x01;
	wdata[1] = 0x00;
	wdata[2] = 0x00;
	wdata[3] = 0x00;
	mfc_write_fw_flash_LSI(charger, 0xfc, 0x6f, wdata);

	msleep(10);
	pr_info("%s: Enter the normal mode\n", __func__);
	if (mfc_reg_write(charger->client, 0x1F10, 0x20) < 0) {
		pr_err("%s: failed to enter the normal mode\n", __func__);
		return false;
	}
	msleep(10);

	return true;
}
static void mfc_uno_on(struct mfc_charger_data *charger, bool onoff)
{
	union power_supply_propval value = {0, };

	if (onoff) { /* UNO ON */
		psy_do_property("otg", get,
			POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval) {
			charger->is_otg_on = true;
			psy_do_property(charger->pdata->wired_charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW, value);
			pr_info("%s CHGIN_OTG on, check OTG flag\n", __func__);
		} else
			charger->is_otg_on = false;
		value.intval = 1;
		psy_do_property(charger->pdata->wired_charger_name, set,
			POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL, value);
		pr_info("%s: ENABLE\n", __func__);
	} else { /* UNO OFF */
		value.intval = 0;
		psy_do_property(charger->pdata->wired_charger_name, set,
			POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL, value);
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_ONLINE, value);

		if ((charger->is_otg_on) &&
			value.intval == SEC_BATTERY_CABLE_OTG) { /* OTG status has to be recovered */
			value.intval = 1;
			psy_do_property(charger->pdata->wired_charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
			pr_info("%s CHGIN_OTG was ON, recover OTG status\n", __func__);
		}
		charger->is_otg_on = false;
		pr_info("%s: DISABLE\n", __func__);
	}
}

static void mfc_wpc_cm_fet_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_cm_fet_work.work);
	u8 tmp = 0;

	/* disable all CM FETs for MST operation */
	mfc_reg_write(charger->client, MFC_RX_COMM_MOD_FET_REG, 0xf0);
	mfc_reg_read(charger->client, MFC_RX_COMM_MOD_FET_REG, &tmp);
	pr_info("%s: disable CM FET (0x%x)\n", __func__, tmp);
}

static void mfc_wpc_afc_vout_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_afc_vout_work.work);
	u8 data_val[4];
	u8 cmd = 0;
	u8 i;
	union power_supply_propval value = {0, };

	pr_info("%s start\n", __func__);

	/* change cable type */
	if (charger->pdata->cable_type == MFC_PAD_WPC_STAND_HV)
		value.intval = SEC_WIRELESS_PAD_WPC_STAND_HV;
	else if (charger->pdata->cable_type == MFC_PAD_WPC_VEHICLE_HV)
		value.intval = SEC_WIRELESS_PAD_VEHICLE_HV;
	else {
		charger->pdata->cable_type = MFC_PAD_WPC_AFC;
		value.intval = SEC_WIRELESS_PAD_WPC_HV;
	}
	psy_do_property("wireless", set,
		POWER_SUPPLY_PROP_ONLINE, value);

#if defined(CONFIG_BATTERY_SWELLING)
	/* check swelling mode */
	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, value);
	pr_info("%s: check swelling mode(%d)\n", __func__, value.intval);

	if (value.intval)
		goto skip_set_afc_vout;
#endif

	for (i = 0; i < CMD_CNT; i++) {
		cmd = WPC_COM_AFC_SET;
		data_val[0] = 0x2c; /* Value for WPC AFC_SET 10V */
		pr_info("%s set 10V , cnt = %d\n", __func__, i);
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
		mfc_reg_read(charger->client, MFC_WPC_RX_DATA_COM_REG, &data_val[0]);
		mfc_reg_read(charger->client, MFC_WPC_RX_DATA_VALUE0_REG, &data_val[0]);
		mfc_reg_read(charger->client, MFC_AP2MFC_CMD_L_REG, &data_val[0]);
		msleep(100);
	}
	charger->is_afc_tx = true;
	pr_info("%s: is_afc_tx = %d vout read = %d\n",
		__func__, charger->is_afc_tx, mfc_get_adc(charger, MFC_ADC_VOUT));

	/* use all CM FETs for 10V wireless charging */
	mfc_reg_write(charger->client, MFC_RX_COMM_MOD_FET_REG, 0x00);
	mfc_reg_read(charger->client, MFC_RX_COMM_MOD_FET_REG, &cmd);
	pr_info("%s: CM FET setting(0x%x)\n", __func__, cmd);

	psy_do_property("otg", get,
		POWER_SUPPLY_PROP_ONLINE, value);
	pr_info("%s: check state(%d, %d, %d)\n", __func__,
		charger->vout_mode, charger->pdata->vout_status, value.intval);

	if (!charger->is_full_status && !value.intval &&
		charger->vout_mode != WIRELESS_VOUT_5V &&
		charger->vout_mode != WIRELESS_VOUT_5V_STEP) {
		charger->vout_mode = WIRELESS_VOUT_10V;
		cancel_delayed_work(&charger->wpc_vout_mode_work);
		wake_lock(&charger->wpc_vout_mode_lock);
		queue_delayed_work(charger->wqueue,
			&charger->wpc_vout_mode_work, 0);
	}

#if defined(CONFIG_BATTERY_SWELLING)
skip_set_afc_vout:
#endif
	wake_unlock(&charger->wpc_afc_vout_lock);
}

static void mfc_wpc_fw_update_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_fw_update_work.work);

	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	const u8 *fw_img;

	int ret = 0;
	int i = 0;
	char fwtime[8] = {32, 32, 32, 32, 32, 32, 32, 32};
	char fwdate[8] = {32, 32, 32, 32, 32, 32, 32, 32};
	u8 data = 32; /* ascii space */

	mutex_lock(&charger->fw_lock);
	pr_info("%s firmware update mode is = %d\n", __func__, charger->fw_cmd);
	switch(charger->fw_cmd) {
	case SEC_WIRELESS_RX_SDCARD_MODE:
		mfc_uno_on(charger, true);
		charger->pdata->otp_firmware_result = MFC_FW_RESULT_DOWNLOADING;
		msleep(200);
		disable_irq(charger->pdata->irq_wpc_int);
		disable_irq(charger->pdata->irq_wpc_det);
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(MFC_FW_SDCARD_BIN_PATH, O_RDONLY, S_IRUSR);

		if (IS_ERR(fp)) {
			pr_err("%s: failed to open %s, (%d)\n", __func__, MFC_FW_SDCARD_BIN_PATH, IS_ERR(fp));
			set_fs(old_fs);
			goto fw_err;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_err("%s: start, file path %s, size %ld Bytes\n",
			__func__, MFC_FW_SDCARD_BIN_PATH, fsize);

		fw_img = kmalloc(fsize, GFP_KERNEL);

		if (fw_img == NULL) {
			pr_err("%s, kmalloc failed\n", __func__);
			goto malloc_error;
		}

		nread = vfs_read(fp, (char __user *)fw_img,
					fsize, &fp->f_pos);
		pr_err("nread %ld Bytes\n", nread);
		if (nread != fsize) {
			pr_err("failed to read firmware file, nread %ld Bytes\n", nread);
			goto read_err;
		}

		filp_close(fp, current->files);
		set_fs(old_fs);
		mfc_get_chip_id(charger);
		pr_info("%s chip_id(%d)\n", __func__, charger->chip_id);

		mfc_otp_update = 1;
		if (charger->chip_id == MFC_CHIP_LSI) {
			pr_info("%s: S.LSI MFC IC doesn't support sdcard f/w update\n", __func__);
			goto read_err;
		}
		else /* MFC_CHIP_IDT */
			ret = PgmOTPwRAM_IDT(charger, 0 ,fw_img, 0, fsize);
		mfc_otp_update = 0;
		charger->pdata->otp_firmware_ver = mfc_get_firmware_version(charger, MFC_RX_FIRMWARE);
		charger->pdata->wc_ic_grade = mfc_get_ic_revision(charger, MFC_IC_FONT);
		charger->pdata->wc_ic_rev = mfc_get_ic_revision(charger, MFC_IC_REVISION);

		if (ret)
			charger->pdata->otp_firmware_result = MFC_FW_RESULT_PASS;
		else
			charger->pdata->otp_firmware_result = MFC_FW_RESULT_FAIL;

		kfree(fw_img);
		enable_irq(charger->pdata->irq_wpc_int);
		enable_irq(charger->pdata->irq_wpc_det);
		break;
	case SEC_WIRELESS_RX_BUILT_IN_MODE:
		mfc_uno_on(charger, true);
		charger->pdata->otp_firmware_result = MFC_FW_RESULT_DOWNLOADING;
		msleep(200);
		disable_irq(charger->pdata->irq_wpc_int);
		disable_irq(charger->pdata->irq_wpc_det);
		dev_err(&charger->client->dev, "%s, request_firmware\n", __func__);
		ret = request_firmware(&charger->firm_data_bin, MFC_FLASH_FW_HEX_PATH,
			&charger->client->dev);
		if ( ret < 0) {
			dev_err(&charger->client->dev, "%s: failed to request firmware %s (%d)\n", __func__, MFC_FLASH_FW_HEX_PATH, ret);
			charger->pdata->otp_firmware_result = MFC_FW_RESULT_FAIL;
			goto fw_err;
		}
		wake_lock(&charger->wpc_update_lock);
		mfc_get_chip_id(charger);
		pr_info("%s data size = %ld, chip_id(%d)\n", __func__, charger->firm_data_bin->size, charger->chip_id);
		mfc_otp_update = 1;
		if (charger->chip_id == MFC_CHIP_LSI)
			ret = PgmOTPwRAM_LSI(charger, 0 ,charger->firm_data_bin->data, 0, charger->firm_data_bin->size);
		else /* MFC_CHIP_IDT */
			ret = PgmOTPwRAM_IDT(charger, 0 ,charger->firm_data_bin->data, 0, charger->firm_data_bin->size);
		mfc_otp_update = 0;
		release_firmware(charger->firm_data_bin);

		for (i = 0; i < 8; i++) {
			if (mfc_reg_read(charger->client, MFC_FW_DATA_CODE_0+i, &data) > 0)
				fwdate[i] = (char)data;
		}
		for (i = 0; i < 8; i++) {
			if (mfc_reg_read(charger->client, MFC_FW_TIMER_CODE_0+i, &data) > 0)
				fwtime[i] = (char)data;
		}
		pr_info("%s: %d%d%d%d%d%d%d%d, %d%d%d%d%d%d%d%d\n", __func__,
			fwdate[0], fwdate[1], fwdate[2],fwdate[3], fwdate[4], fwdate[5], fwdate[6], fwdate[7],
			fwtime[0], fwtime[1], fwtime[2],fwtime[3], fwtime[4], fwtime[5], fwtime[6], fwtime[7]);

		charger->pdata->otp_firmware_ver = mfc_get_firmware_version(charger, MFC_RX_FIRMWARE);
		charger->pdata->wc_ic_grade = mfc_get_ic_revision(charger, MFC_IC_FONT);
		charger->pdata->wc_ic_rev = mfc_get_ic_revision(charger, MFC_IC_REVISION);

		if (ret)
			charger->pdata->otp_firmware_result = MFC_FW_RESULT_PASS;
		else
			charger->pdata->otp_firmware_result = MFC_FW_RESULT_FAIL;

		for (i = 0; i < 8; i++) {
			if (mfc_reg_read(charger->client, MFC_FW_DATA_CODE_0+i, &data) > 0)
				fwdate[i] = (char)data;
		}
		for (i = 0; i < 8; i++) {
			if (mfc_reg_read(charger->client, MFC_FW_TIMER_CODE_0+i, &data) > 0)
				fwtime[i] = (char)data;
		}
		pr_info("%s: %d%d%d%d%d%d%d%d, %d%d%d%d%d%d%d%d\n", __func__,
			fwdate[0], fwdate[1], fwdate[2],fwdate[3], fwdate[4], fwdate[5], fwdate[6], fwdate[7],
			fwtime[0], fwtime[1], fwtime[2],fwtime[3], fwtime[4], fwtime[5], fwtime[6], fwtime[7]);

		enable_irq(charger->pdata->irq_wpc_int);
		enable_irq(charger->pdata->irq_wpc_det);
		wake_unlock(&charger->wpc_update_lock);
		break;
	case SEC_WIRELESS_TX_ON_MODE:
		charger->pdata->cable_type = MFC_PAD_TX;
		break;
	case SEC_WIRELESS_TX_OFF_MODE:
		/* need to check */
		break;
	case SEC_WIRELESS_RX_INIT:
		/* need to check */
		break;
	default:
		break;
	}

	msleep(200);
	mfc_uno_on(charger, false);
	pr_info("%s ---------------------------------------------------------------\n", __func__);
	mutex_unlock(&charger->fw_lock);

	return;

read_err:
	kfree(fw_img);
malloc_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
fw_err:
	mfc_uno_on(charger, false);
	mutex_unlock(&charger->fw_lock);
}

/*#if !defined(CONFIG_SEC_FACTORY)
static void mfc_wpc_fw_booting_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_fw_booting_work.work);
	union power_supply_propval value = {0, };
	int fw_version;

	value.intval =
		SEC_FUELGAUGE_CAPACITY_TYPE_SCALE;
	psy_do_property(charger->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CAPACITY, value);
	pr_info("%s: battery capacity (%d)\n", __func__, value.intval);

	if (value.intval >= 10) {
		mfc_uno_on(charger, true);
		msleep(200);
		fw_version = mfc_get_firmware_version(charger, MFC_RX_FIRMWARE);
		pr_info("%s: fw version (0x%x)\n", __func__, fw_version);
		if (fw_version != MFC_FW_BIN_VERSION) {
			charger->fw_cmd = SEC_WIRELESS_RX_BUILT_IN_MODE;
			queue_delayed_work(charger->wqueue, &charger->wpc_fw_update_work, 0);
		} else {
			mfc_uno_on(charger, false);
		}
	}
}
#endif*/

static int mfc_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct mfc_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = psp;
//	union power_supply_propval value;
	u8 mst_mode;
	u8 reg_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		pr_info("%s charger->pdata->cs100_status %d\n", __func__, charger->pdata->cs100_status);
		val->intval = charger->pdata->cs100_status;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = mfc_reg_read(charger->client, MFC_MST_MODE_SEL_REG, &mst_mode);
		if (val->intval < 0) {
			pr_info("%s mst mode(0x2) i2c write failed, val->intval = %d\n",
					__func__, val->intval);
			break;
		}

		val->intval = mfc_reg_read(charger->client, MFC_SYS_OP_MODE_REG, &reg_data);
		if (val->intval < 0) {
			pr_info("%s mst mode change irq(0x4) read failed, val->intval = %d\n",
					__func__, val->intval);
			break;
		}
		reg_data &= 0x0C; /* use only [3:2]bit of sys_op_mode register for MST */

		pr_info("%s mst mode check: mst_mode = %d, reg_data = %d\n",
				__func__, mst_mode, reg_data);
		if (reg_data == 0x4)
			val->intval = mst_mode;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
	case POWER_SUPPLY_PROP_HEALTH:
		return -ENODATA;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (charger->pdata->ic_on_mode || charger->pdata->is_charging) {
			val->intval = mfc_get_vout(charger);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
	case POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		return -ENODATA;
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("%s cable_type =%d\n", __func__, charger->pdata->cable_type);
		val->intval = charger->pdata->cable_type;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		val->intval = charger->pdata->vout_status;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		pr_info("%s: POWER_SUPPLY_PROP_MANUFACTURER, intval(0x%x), called by(%ps)\n", __func__, val->intval, __builtin_return_address(0));
		if (val->intval == SEC_WIRELESS_OTP_FIRM_RESULT) {
			pr_info("%s otp firmware result = %d,\n", __func__, charger->pdata->otp_firmware_result);
			val->intval = charger->pdata->otp_firmware_result;
		} else if (val->intval == SEC_WIRELESS_IC_REVISION) {
			pr_info("%s: check f/w revision\n", __func__);
			val->intval = mfc_get_ic_revision(charger, MFC_IC_REVISION);
		} else if (val->intval == SEC_WIRELESS_IC_GRADE) {
			pr_info("%s: check f/w revision\n", __func__);
			val->intval = mfc_get_ic_revision(charger, MFC_IC_FONT);
		} else if (val->intval == SEC_WIRELESS_OTP_FIRM_VER_BIN) {
			val->intval = MFC_FW_BIN_VERSION; /* latest MFC F/W binary version */
		} else if (val->intval == SEC_WIRELESS_OTP_FIRM_VER) {
			val->intval = mfc_get_firmware_version(charger, MFC_RX_FIRMWARE);
			pr_info("%s: check f/w revision (0x%x)\n", __func__, val->intval);
		} else if (val->intval == SEC_WIRELESS_TX_FIRM_RESULT) {
			val->intval = charger->pdata->tx_firmware_result;
		} else if (val->intval == SEC_WIRELESS_TX_FIRM_VER) {
			val->intval = charger->pdata->tx_firmware_ver;
		} else if (val->intval == SEC_TX_FIRMWARE) {
			val->intval = charger->pdata->tx_status;
		} else if (val->intval == SEC_WIRELESS_OTP_FIRM_VERIFY) {
			mfc_get_chip_id(charger);
			if (charger->chip_id == MFC_CHIP_LSI) {
				pr_info("%s: LSI FIRM_VERIFY is not implemented\n", __func__);
				val->intval = 1;
			} else {
				pr_info("%s: IDT FIRM_VERIFY\n", __func__);
				msleep(10);
				val->intval = mfc_firmware_verify(charger);
			}
		} else if (val->intval == SEC_WIRELESS_MST_SWITCH_VERIFY) {
			/* This is the WA codes that reduces VRECT invalid case. */
			if (gpio_is_valid(charger->pdata->mst_pwr_en)) {
				charger->mst_off_lock = 1;
				gpio_direction_output(charger->pdata->mst_pwr_en, 1);
				usleep_range(3600, 4000);
				gpio_direction_output(charger->pdata->mst_pwr_en, 0);
				msleep(50);
				charger->mst_off_lock = 0;

				gpio_direction_output(charger->pdata->mst_pwr_en, 1);
				msleep(charger->pdata->mst_switch_delay);
				val->intval = mfc_get_firmware_version(charger, MFC_RX_FIRMWARE);
				pr_info("%s: check f/w revision, mst power on (0x%x)\n", __func__, val->intval);
				gpio_direction_output(charger->pdata->mst_pwr_en, 0);
			} else {
				pr_info("%s: MST_SWITCH_VERIFY, invalid gpio(mst_pwr_en)\n", __func__);
				val->intval = -1;
			}
		} else {
			val->intval = -ENODATA;
			pr_err("%s wrong mode\n", __func__);
		}
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW: /* vout */
		if (charger->pdata->ic_on_mode || charger->pdata->is_charging) {
			val->intval = mfc_get_adc(charger, MFC_ADC_VOUT);
			pr_info("%s: wc vout (%d)\n", __func__, val->intval);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG: /* vrect */
		if (charger->pdata->ic_on_mode || charger->pdata->is_charging) {
			val->intval = mfc_get_adc(charger, MFC_ADC_VRECT);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = mfc_get_adc(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN:
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_WIRELESS_OP_FREQ:
			val->intval = mfc_get_adc(charger, MFC_ADC_OP_FRQ);
			pr_info("%s: Operating FQ %dkHz\n", __func__, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_CMD:
			val->intval = charger->pdata->tx_data_cmd;
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VAL:
			val->intval = charger->pdata->tx_data_val;
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID:
			val->intval = charger->tx_id;
			break;
		default:
			return -ENODATA;
		}
		break;

	default:
		return -ENODATA;
	}
	return 0;
}

static void mfc_wpc_vout_mode_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_vout_mode_work.work);
	int vout_step = charger->pdata->vout_status;
	int vout = MFC_VOUT_10V;

	pr_info("%s: start - vout_mode(%d), vout_status(%d)\n",
		__func__, charger->vout_mode, charger->pdata->vout_status);
	switch (charger->vout_mode) {
	case WIRELESS_VOUT_5V:
		mfc_set_vout(charger, MFC_VOUT_5V);
		break;
	case WIRELESS_VOUT_9V:
		mfc_set_vout(charger, MFC_VOUT_9V);
		break;
	case WIRELESS_VOUT_10V:
		mfc_set_vout(charger, MFC_VOUT_10V);
		break;
	case WIRELESS_VOUT_5V_STEP:
		vout_step--;
		if (vout_step >= MFC_VOUT_5V) {
			mfc_set_vout(charger, vout_step);
			cancel_delayed_work(&charger->wpc_vout_mode_work);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_vout_mode_work, msecs_to_jiffies(250));
			return;
		}
		break;
	case WIRELESS_VOUT_9V_STEP:
		vout = MFC_VOUT_9V;
	case WIRELESS_VOUT_10V_STEP:
		vout_step++;
		if (vout_step <= vout) {
			mfc_set_vout(charger, vout_step);
			cancel_delayed_work(&charger->wpc_vout_mode_work);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_vout_mode_work, msecs_to_jiffies(250));
			return;
		}
		break;
	case WIRELESS_VOUT_CV_CALL:
	case WIRELESS_VOUT_CC_CALL:
		mfc_set_vrect_adjust(charger, MFC_HEADROOM_3);
		msleep(500);
		mfc_set_vout(charger, MFC_VOUT_5V);
		msleep(500);
		mfc_set_vrect_adjust(charger, MFC_HEADROOM_0);
		break;
	case WIRELESS_VOUT_CC_CV_VOUT:
		mfc_set_vout(charger, MFC_VOUT_5_5V);
		break;
	default:
		break;
	}
#if !defined(CONFIG_SEC_FACTORY)
	if ((charger->pdata->cable_type == MFC_PAD_WPC_AFC ||
		charger->pdata->cable_type == MFC_PAD_WPC_STAND_HV ||
		charger->pdata->cable_type == MFC_PAD_WPC_VEHICLE_HV) &&
		charger->pdata->vout_status <= MFC_VOUT_5V && charger->is_full_status) {
		u8 data = 0x05;
		/* send data for decreasing VRECT to 5V */
		mfc_send_packet(charger, MFC_HEADER_AFC_CONF,
			WPC_COM_AFC_SET, &data, 1);
		pr_info("%s: set TX 5V after cs100\n", __func__);
	}
#endif
	pr_info("%s: finish - vout_mode(%d), vout_status(%d)\n",
		__func__, charger->vout_mode, charger->pdata->vout_status);
	wake_unlock(&charger->wpc_vout_mode_lock);
}

#if defined(CONFIG_UPDATE_BATTERY_DATA)
static int mfc_chg_parse_dt(struct device *dev, mfc_charger_platform_data_t *pdata);
#endif
static int mfc_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct mfc_charger_data *charger = power_supply_get_drvdata(psy);

	int vout, vrect, iout, freq, i = 0;
	u8 tmp = 0;
	/* int ret; */
	union power_supply_propval value;
	u8 fod[12] = {0, };

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL) {
			pr_info("%s set cs100\n", __func__);
			if (charger->pdata->cable_type == SEC_WIRELESS_PAD_WPC) {
				/* set fake FOD values before send cs100, need to tune */
				mfc_fod_set_cs100(charger);
			}
			charger->pdata->cs100_status = mfc_send_cs100(charger);
#if !defined(CONFIG_SEC_FACTORY)
			charger->is_full_status = 1;
			if (charger->pdata->cable_type == MFC_PAD_WPC_AFC ||
				charger->pdata->cable_type == MFC_PAD_WPC_STAND_HV ||
				charger->pdata->cable_type == MFC_PAD_WPC_VEHICLE_HV) {
				charger->vout_mode = WIRELESS_VOUT_5V_STEP;
				cancel_delayed_work(&charger->wpc_vout_mode_work);
				wake_lock(&charger->wpc_vout_mode_lock);
				queue_delayed_work(charger->wqueue,
					&charger->wpc_vout_mode_work, msecs_to_jiffies(250));
			}
#endif
		} else if (val->intval == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			mfc_mis_align(charger);
		} else if (val->intval == POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE) {
			mfc_fod_set_cv(charger);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		value.intval = charger->pdata->cable_type;
		psy_do_property("wireless", set, POWER_SUPPLY_PROP_ONLINE, value);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == POWER_SUPPLY_HEALTH_OVERHEAT ||
			val->intval == POWER_SUPPLY_HEALTH_OVERHEATLIMIT ||
			val->intval == POWER_SUPPLY_HEALTH_COLD) {
			pr_info("%s ept-ot\n", __func__);
			mfc_send_eop(charger, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (is_wireless_type(val->intval))
			charger->pdata->ic_on_mode = true;
		else
			charger->pdata->ic_on_mode = false;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		if (val->intval) {
			charger->is_mst_on = MST_MODE_2;
			pr_info("%s: set MST mode 2\n", __func__);
			/* disable CM FETs to avoid MST/WPC crash situation  */
			queue_delayed_work(charger->wqueue,
				&charger->wpc_cm_fet_work, msecs_to_jiffies(1000));
		} else {
			if (charger->chip_id == MFC_CHIP_LSI) {
				/*
				* Default Idle voltage of the INT_A is LOW.
				* Prevent the un-wanted INT_A Falling handling.
				* This is a work-around, and will be fixed by the revision.
				*/
				charger->mst_off_lock = 1;
				if (!delayed_work_pending(&charger->mst_off_work))
					queue_delayed_work(charger->wqueue, &charger->mst_off_work, 0);
			}
			pr_info("%s: set MST mode off\n", __func__);
			charger->is_mst_on = MST_MODE_0;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		charger->pdata->siop_level = val->intval;
		if (charger->pdata->siop_level == 100) {
			pr_info("%s vrect headroom set ROOM 2, siop = %d\n", __func__, charger->pdata->siop_level);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_2);
		} else if (charger->pdata->siop_level < 100) {
			pr_info("%s vrect headroom set ROOM 0, siop = %d\n", __func__, charger->pdata->siop_level);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_0);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		if (val->intval)
			charger->pdata->ic_on_mode = true;
		else
			charger->pdata->ic_on_mode = false;
		break;
	case POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL:
		charger->fw_cmd = val->intval;
		queue_delayed_work(charger->wqueue, &charger->wpc_fw_update_work, 0);
		pr_info("%s: rx result = %d, tx result = %d\n", __func__,
			charger->pdata->otp_firmware_result, charger->pdata->tx_firmware_result);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		if (val->intval == WIRELESS_VOUT_NORMAL_VOLTAGE) {
			pr_info("%s: Wireless Vout forced set to 5V. + PAD CMD\n", __func__);
			for (i = 0; i < CMD_CNT; i++) {
				mfc_send_command(charger, MFC_AFC_CONF_5V);
				msleep(250);
			}
		} else if (val->intval == WIRELESS_VOUT_HIGH_VOLTAGE) {
			pr_info("%s: Wireless Vout forced set to 10V. + PAD CMD\n", __func__);
			for (i = 0; i < CMD_CNT; i++) {
				mfc_send_command(charger, MFC_AFC_CONF_10V);
				msleep(250);
			}
		} else if (val->intval == WIRELESS_VOUT_CC_CV_VOUT ||
				val->intval == WIRELESS_VOUT_CV_CALL ||
				val->intval == WIRELESS_VOUT_CC_CALL) {
			charger->vout_mode = val->intval;
			cancel_delayed_work(&charger->wpc_vout_mode_work);
			wake_lock(&charger->wpc_vout_mode_lock);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_vout_mode_work, 0);
		} else if (val->intval == WIRELESS_VOUT_5V ||
				val->intval == WIRELESS_VOUT_5V_STEP) {
			int def_delay = 0;
			charger->vout_mode = val->intval;
			if ((charger->pdata->cable_type == MFC_PAD_WPC_AFC ||
				charger->pdata->cable_type == MFC_PAD_WPC_STAND_HV ||
				charger->pdata->cable_type == MFC_PAD_WPC_VEHICLE_HV)) {
				def_delay = 250;
			}
			cancel_delayed_work(&charger->wpc_vout_mode_work);
			wake_lock(&charger->wpc_vout_mode_lock);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_vout_mode_work, msecs_to_jiffies(def_delay));
		} else if (val->intval == WIRELESS_VOUT_9V ||
			val->intval == WIRELESS_VOUT_10V ||
			val->intval == WIRELESS_VOUT_9V_STEP ||
			val->intval == WIRELESS_VOUT_10V_STEP) {
			if (!charger->is_full_status) {
				if (!charger->is_afc_tx) {
					u8 data_val[4], cmd = 0;

					pr_info("%s: need to set afc tx before vout control\n", __func__);
					for (i = 0; i < CMD_CNT; i++) {
						cmd = WPC_COM_AFC_SET;
						data_val[0] = 0x2c; /* Value for WPC AFC_SET 10V */
						pr_info("%s set 10V , cnt = %d \n", __func__, i);
						mfc_send_packet(charger, MFC_HEADER_AFC_CONF, cmd, data_val, 1);
						mfc_reg_read(charger->client, MFC_WPC_RX_DATA_COM_REG, &data_val[0]);
						mfc_reg_read(charger->client, MFC_WPC_RX_DATA_VALUE0_REG, &data_val[0]);
						mfc_reg_read(charger->client, MFC_AP2MFC_CMD_L_REG, &data_val[0]);
						msleep(100);
					}
					charger->is_afc_tx = true;
					pr_info("%s: is_afc_tx = %d vout read = %d \n",
						__func__, charger->is_afc_tx, mfc_get_adc(charger, MFC_ADC_VOUT));

					/* use all CM FETs for 10V wireless charging */
					mfc_reg_write(charger->client, MFC_RX_COMM_MOD_FET_REG, 0x00);
					mfc_reg_read(charger->client, MFC_RX_COMM_MOD_FET_REG, &cmd);
					pr_info("%s: CM FET setting(0x%x) \n", __func__, cmd);
				}
				charger->vout_mode = val->intval;
				cancel_delayed_work(&charger->wpc_vout_mode_work);
				wake_lock(&charger->wpc_vout_mode_lock);
				queue_delayed_work(charger->wqueue,
					&charger->wpc_vout_mode_work, msecs_to_jiffies(250));
			} else {
				pr_info("%s: block to set high vout level(vs=%d) because full status\n",
					__func__, charger->pdata->vout_status);
			}
		} else if (val->intval == WIRELESS_PAD_FAN_OFF) {
			pr_info("%s: fan off\n", __func__);
			mfc_fan_control(charger, 0);
		} else if (val->intval == WIRELESS_PAD_FAN_ON) {
			pr_info("%s: fan on\n", __func__);
			mfc_fan_control(charger, 1);
		} else if (val->intval == WIRELESS_PAD_LED_OFF) {
			pr_info("%s: led off\n", __func__);
			mfc_led_control(charger, 0);
		} else if (val->intval == WIRELESS_PAD_LED_ON) {
			pr_info("%s: led on\n", __func__);
			mfc_led_control(charger, 1);
		} else if (val->intval == WIRELESS_VRECT_ADJ_ON) {
			pr_info("%s: vrect adjust to have big headroom(default value)\n", __func__);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_1);
		} else if (val->intval == WIRELESS_VRECT_ADJ_OFF) {
			pr_info("%s: vrect adjust to have small headroom\n", __func__);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_0);
		} else if (val->intval == WIRELESS_VRECT_ADJ_ROOM_0) {
			pr_info("%s: vrect adjust to have headroom 0(0mV)\n", __func__);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_0);
		} else if (val->intval == WIRELESS_VRECT_ADJ_ROOM_1) {
			pr_info("%s: vrect adjust to have headroom 1(277mV)\n", __func__);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_1);
		} else if (val->intval == WIRELESS_VRECT_ADJ_ROOM_2) {
			pr_info("%s: vrect adjust to have headroom 2(497mV)\n", __func__);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_2);
		} else if (val->intval == WIRELESS_VRECT_ADJ_ROOM_3) {
			pr_info("%s: vrect adjust to have headroom 3(650mV)\n", __func__);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_3);
		} else if (val->intval == WIRELESS_VRECT_ADJ_ROOM_4) {
			pr_info("%s: vrect adjust to have headroom 4(30mV)\n", __func__);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_4);
		} else if (val->intval == WIRELESS_VRECT_ADJ_ROOM_5) {
			pr_info("%s: vrect adjust to have headroom 5(82mV)\n", __func__);
			mfc_set_vrect_adjust(charger, MFC_HEADROOM_5);
		} else if (val->intval == WIRELESS_CLAMP_ENABLE) {
			pr_info("%s: enable clamp1, clamp2 for WPC modulation\n", __func__);
			//default enabled state. no need to config.
			//mfc_reg_update(charger->client, MFC_RX_COMM_MOD_FET_REG, 0x00, 0x00);
		} else {
			pr_info("%s: Unknown Command(%d)\n", __func__, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		charger->pdata->otp_firmware_result = val->intval;
		pr_info("%s: otp_firmware result initialize (%d)\n", __func__,
			charger->pdata->otp_firmware_result);
		break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	case POWER_SUPPLY_PROP_POWER_DESIGN:
		mfc_chg_parse_dt(charger->dev, charger->pdata);
		break;
#endif
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		/* send battery level to TX */
		if (val->intval != charger->pdata->capacity) {
			charger->pdata->capacity = val->intval;
			pr_info("%s Send Capacity(%d) to TX\n", __func__, charger->pdata->capacity);
			mfc_send_packet(charger, MFC_HEADER_AFC_CONF,
				WPC_COM_CHG_LEVEL, &(charger->pdata->capacity), 1);
			msleep(250);
			mfc_send_packet(charger, MFC_HEADER_AFC_CONF,
				WPC_COM_CHG_LEVEL, &(charger->pdata->capacity), 1);
		}
		vout = mfc_get_adc(charger, MFC_ADC_VOUT);
		vrect = mfc_get_adc(charger, MFC_ADC_VRECT);
		iout = mfc_get_adc(charger, MFC_ADC_RX_IOUT);
		freq = mfc_get_adc(charger, MFC_ADC_OP_FRQ);
		pr_info("%s RX_VOUT = %dmV, RX_VRECT = %dmV, RX_IOUT = %dmA, OP_FREQ = %dKHz, IC Rev = 0x%x, IC Font = 0x%x, cable=%d\n",
			__func__, vout, vrect, iout, freq, mfc_get_ic_revision(charger, MFC_IC_REVISION),
			mfc_get_ic_revision(charger, MFC_IC_FONT), charger->pdata->cable_type);

		if ((vout < 6500) && (charger->pdata->capacity >= 85)) {
			mfc_reg_read(charger->client, MFC_RX_COMM_MOD_FET_REG, &tmp);
			if (tmp != 0x00) {
				/* use all CM FETs for 10V wireless charging */
				mfc_reg_write(charger->client, MFC_RX_COMM_MOD_FET_REG, 0x00);
				mfc_reg_read(charger->client, MFC_RX_COMM_MOD_FET_REG, &tmp);
				pr_info("%s: CM FET setting(0x%x)\n", __func__, tmp);
			}
		}

		for (i = 0; i < MFC_NUM_FOD_REG; i++)
			mfc_reg_read(charger->client, MFC_WPC_FOD_0A_REG+i, &fod[i]);
		pr_info("%s: FOD(%d %d %d %d %d %d %d %d %d %d %d %d)\n", __func__,
			fod[0], fod[1], fod[2], fod[3], fod[4], fod[5],
			fod[6], fod[7], fod[8], fod[9], fod[10], fod[11]);
		break;
	case POWER_SUPPLY_PROP_FILTER_CFG:
		charger->led_cover = val->intval;
		pr_info("%s: LED_COVER(%d)\n", __func__, charger->led_cover);
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		mfc_reg_read(charger->client, MFC_STATUS_L_REG, &tmp);
		tmp = tmp >> 7;
		pr_info("%s: MFC LDO (%d), vout (%d)\n", __func__, val->intval, tmp);
		if (val->intval && !tmp) { /* LDO ON */
			mfc_set_cmd_l_reg(charger, MFC_CMD_TOGGLE_LDO_MASK, MFC_CMD_TOGGLE_LDO_MASK);
			pr_info("%s: MFC LDO toggle ------------ cable_work\n", __func__);
			msleep(3);
			mfc_reg_read(charger->client, MFC_STATUS_L_REG, &tmp);
			tmp = tmp >> 7;
			pr_info("%s: MFC LDO STAT(%d)\n", __func__, tmp);
		} else if (!val->intval && tmp) { /* LDO OFF */
			pr_info("%s: MFC LDO toggle ------------ cable_work\n", __func__);
			mfc_set_cmd_l_reg(charger, MFC_CMD_TOGGLE_LDO_MASK, MFC_CMD_TOGGLE_LDO_MASK);
			msleep(3);
			mfc_reg_read(charger->client, MFC_STATUS_L_REG, &tmp);
			tmp = tmp >> 7;
			pr_info("%s: MFC LDO STAT(%d)\n", __func__, tmp);
		}
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		return -ENODATA;
	default:
		return -ENODATA;
	}

	return 0;
}

#define FREQ_OFFSET	384000 /* 64*6000 */
static void mfc_wpc_opfq_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_opfq_work.work);

	u16 op_fq;
	u8 pad_mode;
	union power_supply_propval value;

	mfc_reg_read(charger->client, MFC_SYS_OP_MODE_REG, &pad_mode);
	if ((pad_mode == PAD_MODE_WPC_BASIC) ||\
			(pad_mode == PAD_MODE_WPC_ADV)) {
		op_fq = mfc_get_adc(charger, MFC_ADC_OP_FRQ);
		pr_info("%s: Operating FQ %dkHz(0x%x)\n", __func__, op_fq, op_fq);
		if (op_fq > 230) { /* wpc threshold 230kHz */
			pr_info("%s: Reset M0\n", __func__);
			mfc_reg_write(charger->client, 0x3040, 0x80); /*restart M0 */

			charger->pdata->opfq_cnt++;
			if (charger->pdata->opfq_cnt <= CMD_CNT) {
				queue_delayed_work(charger->wqueue, &charger->wpc_opfq_work, msecs_to_jiffies(10000));
				return;
			}
		}
	} else if ((pad_mode == PAD_MODE_PMA_SR1) ||\
			(pad_mode == PAD_MODE_PMA_SR1E)) {
			charger->pdata->cable_type = MFC_PAD_PMA;
			value.intval = SEC_WIRELESS_PAD_PMA;
			psy_do_property("wireless", set, POWER_SUPPLY_PROP_ONLINE, value);
	}
	charger->pdata->opfq_cnt = 0;
	wake_unlock(&charger->wpc_opfq_lock);

}

static void mfc_wpc_det_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_det_work.work);
	int wc_w_state;
	union power_supply_propval value;
	u8 pad_mode;
	u8 vrect;

	mfc_get_chip_id(charger);
	pr_info("%s : first chip_id read(%d)\n", __func__, charger->chip_id);
	if (charger->chip_id == MFC_CHIP_LSI) {
		/*
		* We don't have to handle the wpc detect handling,
		* when it's the MST mode.
		*/
		if (charger->is_mst_on == MST_MODE_2) {
			pr_info("%s MST RETURN!\n", __func__);
			return;
		}

		if (charger->mst_off_lock == 1) {
			pr_info("%s MST Off Lock!\n", __func__);
			return;
		}
	}

	if (charger->is_mst_on == MST_MODE_2) {
		pr_info("%s: skip wpc_det_work for MST operation\n", __func__);
		return;
	}

	wake_lock(&charger->wpc_wake_lock);
	pr_info("%s\n", __func__);
	wc_w_state = gpio_get_value(charger->pdata->wpc_det);

	if ((charger->wc_w_state == 0) && (wc_w_state == 1)) {
		charger->pdata->vout_status = MFC_VOUT_5V;

#if 0 /* To prepare for the future issue */
		/* read firmware version */
		if (mfc_get_firmware_version(charger, MFC_RX_FIRMWARE) == MFC_OTP_FIRM_VERSION && adc_cal > 0)
			mfc_runtime_sram_change(charger);/* change sram */
#endif

		mfc_get_chip_id(charger);

		/* enable Mode Change INT */
		mfc_reg_update(charger->client, MFC_INT_A_ENABLE_L_REG,
						MFC_STAT_L_OP_MODE_MASK, MFC_STAT_L_OP_MODE_MASK);

		/* read vrect adjust */
		mfc_reg_read(charger->client, MFC_VRECT_ADJ_REG, &vrect);

		pr_info("%s: wireless charger activated, set V_INT as PN\n", __func__);

		/* read pad mode */
		mfc_reg_read(charger->client, MFC_SYS_OP_MODE_REG, &pad_mode);
		pad_mode = pad_mode >> 5;
		pr_info("%s: Pad type (0x%x)\n", __func__, pad_mode);
		if ((pad_mode == PAD_MODE_PMA_SR1) ||
			(pad_mode == PAD_MODE_PMA_SR1E)) {
			charger->pdata->cable_type = MFC_PAD_PMA;
			value.intval = SEC_WIRELESS_PAD_PMA;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		} else if ((pad_mode == PAD_MODE_WPC_BASIC) ||
			(pad_mode == PAD_MODE_WPC_ADV)) {
			charger->pdata->cable_type = MFC_PAD_WPC;
			value.intval = SEC_WIRELESS_PAD_WPC;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
			wake_lock(&charger->wpc_opfq_lock);
			queue_delayed_work(charger->wqueue, &charger->wpc_opfq_work, msecs_to_jiffies(10000));
		} else if ((pad_mode == PAD_MODE_A4WP) ||
			(pad_mode == PAD_MODE_A4WP_LPM)) {
			/* Enable BT2AP INT src */
			mfc_reg_write(charger->client, MFC_INT_A_ENABLE_L_REG, 0x03); // ENABLE BT2AP INTR
			mfc_reg_write(charger->client, MFC_INT_A_ENABLE_H_REG, 0x80); // ENABLE BT2AP INTR

			/* Enable AP2BT INT src */
			mfc_reg_write(charger->client, MFC_INT_B_ENABLE_REG, 0x83); // ENABLE AP2BT INTR

			charger->pdata->cable_type = MFC_PAD_A4WP;
			value.intval = SEC_WIRELESS_PAD_WPC;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}

		/* set fod value */
		if (charger->pdata->fod_data_check)
			mfc_fod_set(charger);
#if !defined(CONFIG_WIRELESS_NO_HV)
		/* set request afc_tx */
		mfc_send_command(charger, MFC_REQUEST_AFC_TX);
#endif
		/* set rpp scaling factor for LED cover */
		mfc_rpp_set(charger);
#if 0
		/* set request TX_ID */
		mfc_send_command(charger, MFC_REQUEST_TX_ID);
#endif

		charger->pdata->is_charging = 1;
	} else if ((charger->wc_w_state == 1) && (wc_w_state == 0)) {

		charger->pdata->cable_type = MFC_PAD_NONE;
		charger->pdata->is_charging = 0;
		charger->pdata->vout_status = MFC_VOUT_5V;
		charger->pdata->opfq_cnt = 0;
		charger->pdata->tx_data_cmd = 0;
		charger->pdata->tx_data_val = 0;
		charger->vout_mode = 0;
		charger->is_full_status = 0;
		charger->pdata->capacity = 101;
		charger->is_afc_tx = false;
		charger->tx_id = 0;

		value.intval = SEC_WIRELESS_PAD_NONE;
		psy_do_property("wireless", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: wpc deactivated, set V_INT as PD\n", __func__);

		msleep(1000);
		/* if vrect >= 3000mV and vout <= 2000mV, restart M0 */
		if (mfc_get_adc(charger, MFC_ADC_VRECT) >= 3000 &&
			mfc_get_adc(charger, MFC_ADC_VOUT) <= 2000) {
			pr_err("%s Restart M0\n", __func__);
			/* reset MCU of MFC IC */
			mfc_set_cmd_l_reg(charger, MFC_CMD_MCU_RESET_MASK, MFC_CMD_MCU_RESET_MASK);
		}

		if (delayed_work_pending(&charger->wpc_opfq_work)) {
			wake_unlock(&charger->wpc_opfq_lock);
			cancel_delayed_work(&charger->wpc_opfq_work);
		}
		if (delayed_work_pending(&charger->wpc_afc_vout_work)) {
			wake_unlock(&charger->wpc_afc_vout_lock);
			cancel_delayed_work(&charger->wpc_afc_vout_work);
		}
		if (delayed_work_pending(&charger->wpc_vout_mode_work)) {
			wake_unlock(&charger->wpc_vout_mode_lock);
			cancel_delayed_work(&charger->wpc_vout_mode_work);
		}

		cancel_delayed_work(&charger->wpc_isr_work);
		cancel_delayed_work(&charger->wpc_opfq_work);
		cancel_delayed_work(&charger->wpc_tx_id_work);
	}

	pr_info("%s: w(%d to %d)\n", __func__,
		charger->wc_w_state, wc_w_state);

	charger->wc_w_state = wc_w_state;
	wake_unlock(&charger->wpc_wake_lock);
}

/* INT_A (BT2AP interrupt) */
static void mfc_wpc_isr_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_isr_work.work);

	u8 cmd_data, val_data;
#if !defined(CONFIG_WIRELESS_NO_HV)
	int i;
#endif
	union power_supply_propval value;

	if (!charger->wc_w_state) {
		pr_info("%s: charger->wc_w_state is 0. exit wpc_isr_work.\n", __func__);
		return;
	}

	pr_info("%s: cable_type (0x%x)\n", __func__, charger->pdata->cable_type);
	wake_lock(&charger->wpc_wake_lock);
	pr_info("%s\n", __func__);


	if (charger->pdata->cable_type == MFC_PAD_A4WP) {
		mfc_reg_read(charger->client, MFC_BT2AP_DATA_COM_REG, &cmd_data);
		mfc_reg_read(charger->client, MFC_BT2AP_DATA_VALUE0_REG, &val_data);
		charger->pdata->tx_data_cmd = cmd_data;
		charger->pdata->tx_data_val = val_data;

		pr_info("%s: A4WP Interrupt Occured, CMD : 0x%x, DATA : 0x%x\n",
			__func__, cmd_data, val_data);

		switch (cmd_data)
		{
		case BT2AP_COM_TX_ID:
			switch (val_data) {
			case TX_ID_UNKNOWN:
			case TX_ID_BATT_PACK_TA:
			case TX_ID_BATT_PACK:
			case TX_ID_STAND_TYPE_START:
			default:
				break;
			} //cmd_data : BT2AP_COM_TX_ID switch end
			break;
		case BT2AP_COM_REQ_AFC_TX:
			break;
		case BT2AP_COM_AFC_MODE:
			switch (val_data) {
			case TX_AFC_SET_5V:
				charger->pad_vout = PAD_VOUT_5V;
				break;
			case TX_AFC_SET_10V:
				pr_info("%s data = 0x%x, might be 10V irq\n", __func__, val_data);
				if (!gpio_get_value(charger->pdata->wpc_det)) {
					wake_unlock(&charger->wpc_wake_lock);
					return;
				}
#if !defined(CONFIG_WIRELESS_NO_HV)
				mfc_send_command(charger, MFC_AFC_CONF_10V);
				msleep(500);
				charger->pdata->cable_type = MFC_PAD_A4WP;
				/* If A4WP_HV is supported, then SEC_WIRELESS_PAD_A4WP_HV type should be used.
					and sec_battery and charger file also have to change wireless cable type.*/
				value.intval = SEC_WIRELESS_PAD_WPC_HV;
				psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);

				for (i = 0; i < CMD_CNT - 1; i++) {
					if (!gpio_get_value(charger->pdata->wpc_det)) {
						wake_unlock(&charger->wpc_wake_lock);
						return;
					}
					if (mfc_get_adc(charger, MFC_ADC_VOUT) > 7500) {
						pr_info("%s 10V set is done\n", __func__);
						break;
					} else {
						pr_info("%s send AFC_CONF_10V again\n", __func__);
						mfc_send_command(charger, MFC_AFC_CONF_10V);
						msleep(500);
					}
				}

				if (sleep_mode) {
					pr_info("%s sleep mode, turn on fan\n", __func__);
					mfc_fan_control(charger, true);
					msleep(250);

					pr_info("%s sleep mode, turn off fan\n", __func__);
					mfc_fan_control(charger, false);
					msleep(250);
				}
				charger->pad_vout = PAD_VOUT_10V;
#endif
				break;
			case TX_AFC_SET_12V:
			case TX_AFC_SET_18V:
			case TX_AFC_SET_19V:
			case TX_AFC_SET_20V:
			case TX_AFC_SET_24V:
			default:
				pr_info("%s: unsupport : 0x%x", __func__, val_data);
			}
			break;
		case BT2AP_COM_CHG_STATUS:
		case BT2AP_COM_UNKNOWN:
		case BT2AP_COM_PWR_STATUS:
		case BT2AP_COM_SID_TAG:
		case BT2AP_COM_SID_TOKEN:
		case BT2AP_COM_TX_STANDBY:
		case BT2AP_COM_COOLING_CTRL:
		default:
			break;
		}
	} else { /* WPC, PMA */

		mfc_reg_read(charger->client, MFC_WPC_TX_DATA_COM_REG, &cmd_data);
		mfc_reg_read(charger->client, MFC_WPC_TX_DATA_VALUE0_REG, &val_data);
		charger->pdata->tx_data_cmd = cmd_data;
		charger->pdata->tx_data_val = val_data;

		pr_info("%s: WPC Interrupt Occured, CMD : 0x%x, DATA : 0x%x\n",
			__func__, cmd_data, val_data);

		if (cmd_data == WPC_TX_COM_AFC_SET) {
			switch (val_data) {
			case TX_AFC_SET_5V:
				charger->pad_vout = PAD_VOUT_5V;
				break;
			case TX_AFC_SET_10V:
				pr_info("%s data = 0x%x, might be 10V irq\n", __func__, val_data);
				if (!gpio_get_value(charger->pdata->wpc_det)) {
					pr_err("%s Wireless charging is paused during set high voltage.\n", __func__);
					wake_unlock(&charger->wpc_wake_lock);
					return;
				}
#if !defined(CONFIG_WIRELESS_NO_HV)
				if (charger->pdata->cable_type == MFC_PAD_WPC_AFC ||
					charger->pdata->cable_type == MFC_PAD_PREPARE_HV ||
					charger->pdata->cable_type == MFC_PAD_WPC_STAND_HV ||
					charger->pdata->cable_type == MFC_PAD_WPC_VEHICLE_HV) {
					pr_err("%s: Is is already HV wireless cable. No need to set again\n", __func__);
					wake_unlock(&charger->wpc_wake_lock);
					return;
				}

				/* send AFC_SET */
				mfc_send_command(charger, MFC_AFC_CONF_10V);
				msleep(500);

				/* change cable type */
				charger->pdata->cable_type = MFC_PAD_PREPARE_HV;
				value.intval = SEC_WIRELESS_PAD_PREPARE_HV;
				psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);

				if (sleep_mode) {
					pr_info("%s sleep mode, turn on fan\n", __func__);
					mfc_fan_control(charger, true);
					msleep(250);

					pr_info("%s sleep mode, turn off fan\n", __func__);
					mfc_fan_control(charger, false);
					msleep(250);
				}
				charger->pad_vout = PAD_VOUT_10V;
#endif
				break;
			case TX_AFC_SET_12V:
				break;
			case TX_AFC_SET_18V:
			case TX_AFC_SET_19V:
			case TX_AFC_SET_20V:
			case TX_AFC_SET_24V:
				break;
			case TX_ID_VEHICLE_PAD:
				pr_info("%s: VEHICLE PAD\n", __func__);
				charger->pdata->cable_type = MFC_PAD_WPC_VEHICLE;
				value.intval = SEC_WIRELESS_PAD_VEHICLE;
				psy_do_property("wireless", set, POWER_SUPPLY_PROP_ONLINE, value);
				break;
			case TX_ID_BATT_PACK:
				pr_info("%s: WIRELESS BATTERY PACK\n", __func__);
				charger->pdata->cable_type = MFC_PAD_WPC_PACK;
				value.intval = SEC_WIRELESS_PAD_WPC_PACK;
				psy_do_property("wireless", set, POWER_SUPPLY_PROP_ONLINE, value);
				break;
			case TX_ID_BATT_PACK_TA:
				pr_info("%s: WIRELESS BATTERY PACK with TA\n", __func__);
				charger->pdata->cable_type = MFC_PAD_WPC_PACK_TA;
				value.intval = SEC_WIRELESS_PAD_WPC_PACK_TA;
				psy_do_property("wireless", set, POWER_SUPPLY_PROP_ONLINE, value);
				break;
			default:
				pr_info("%s: unsupport : 0x%x", __func__, val_data);
			}

			queue_delayed_work(charger->wqueue, &charger->wpc_tx_id_work, msecs_to_jiffies(1000));
		} else if (cmd_data == WPC_TX_COM_TX_ID) {
			switch (val_data) {
			case TX_ID_UNKNOWN:
			break;
			case TX_ID_VEHICLE_PAD:
				if (charger->pad_vout == PAD_VOUT_10V) {
					if (charger->pdata->cable_type == MFC_PAD_PREPARE_HV) {
						charger->pdata->cable_type = MFC_PAD_WPC_VEHICLE_HV;
						value.intval = SEC_WIRELESS_PAD_PREPARE_HV;
					} else {
						charger->pdata->cable_type = MFC_PAD_WPC_VEHICLE_HV;
						value.intval = SEC_WIRELESS_PAD_VEHICLE_HV;
					}
				} else {
					charger->pdata->cable_type = MFC_PAD_WPC_VEHICLE;
					value.intval = SEC_WIRELESS_PAD_VEHICLE;
				}
				pr_info("%s: VEHICLE Wireless Charge PAD %s\n", __func__,
					charger->pad_vout == PAD_VOUT_10V ? "HV" : "");

				break;
			case TX_ID_STAND_TYPE_START:
				if (charger->pad_vout == PAD_VOUT_10V) {
					if (charger->pdata->cable_type == MFC_PAD_PREPARE_HV) {
						charger->pdata->cable_type = MFC_PAD_WPC_STAND_HV;
						value.intval = SEC_WIRELESS_PAD_PREPARE_HV;
					} else {
						charger->pdata->cable_type = MFC_PAD_WPC_STAND_HV;
						value.intval = SEC_WIRELESS_PAD_WPC_STAND_HV;
					}
				} else {
					charger->pdata->cable_type = MFC_PAD_WPC_STAND;
					value.intval = SEC_WIRELESS_PAD_WPC_STAND;
						mfc_fod_set_hero_5v(charger);
				}
				pr_info("%s: STAND Wireless Charge PAD %s\n", __func__,
					charger->pad_vout == PAD_VOUT_10V ? "HV" : "");
				pr_info("%s: cable_type(%d)\n", __func__, charger->pdata->cable_type);
				break;
			case TX_ID_BATT_PACK:
				charger->pdata->cable_type = MFC_PAD_WPC_PACK;
				value.intval = SEC_WIRELESS_PAD_WPC_PACK;
				pr_info("%s: WIRELESS BATTERY PACK\n", __func__);
				break;
			case TX_ID_BATT_PACK_TA:
				charger->pdata->cable_type = MFC_PAD_WPC_PACK_TA;
				value.intval = SEC_WIRELESS_PAD_WPC_PACK_TA;
				pr_info("%s: WIRELESS BATTERY PACK with TA\n", __func__);
				break;
			default:
				value.intval = charger->pdata->cable_type;
				pr_info("%s: UNDEFINED PAD : 0x%x\n", __func__, val_data);
				break;
			}

			if (value.intval != MFC_PAD_PREPARE_HV)
				psy_do_property("wireless", set, POWER_SUPPLY_PROP_ONLINE, value);

			charger->tx_id = val_data;
			pr_info("%s: TX_ID : 0x%x\n", __func__, val_data);
			value.intval = val_data;
			psy_do_property("wireless", set, POWER_SUPPLY_PROP_AUTHENTIC, value);
		}
	}
	wake_unlock(&charger->wpc_wake_lock);
}

static void mfc_wpc_tx_id_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_tx_id_work.work);

	pr_info("%s\n", __func__);

	mfc_send_command(charger, MFC_REQUEST_TX_ID);
}

/*
* Prevent the un-wanted INT_A Falling handling.
* This is a work-around, and will be fixed by the revision.
*/
static void mfc_mst_off_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, mst_off_work.work);
	pr_info("%s\n", __func__);

	charger->mst_off_lock = 1;
	msleep(25);
	charger->mst_off_lock = 0;
}

static irqreturn_t mfc_wpc_det_irq_thread(int irq, void *irq_data)
{
	struct mfc_charger_data *charger = irq_data;

	pr_info("%s !\n", __func__);

	if (charger->is_probed)
		queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);
	else
		pr_info("%s: prevent work thread before device is probed.\n", __func__);

	return IRQ_HANDLED;
}

/* mfc_mst_routine : MST dedicated codes */
void mfc_mst_routine(struct mfc_charger_data *charger, u8 *irq_src)
{
	if (charger->is_mst_on == MST_MODE_2) {
		/* clear intterupt */
		mfc_reg_write(charger->client, MFC_INT_A_CLEAR_L_REG, irq_src[0]); // clear int
		mfc_reg_write(charger->client, MFC_INT_A_CLEAR_H_REG, irq_src[1]); // clear int
		mfc_set_cmd_l_reg(charger, 0x20, MFC_CMD_CLEAR_INT_MASK); // command

		mfc_reg_write(charger->client, MFC_MST_MODE_SEL_REG, 0x02); /* set MST mode2 */
		pr_info("%s 2AC Missing ! : MST on REV : %d\n", __func__, charger->pdata->wc_ic_rev);

		/* clear intterupt */
		mfc_reg_write(charger->client, MFC_INT_A_CLEAR_L_REG, irq_src[0]); // clear int
		mfc_reg_write(charger->client, MFC_INT_A_CLEAR_H_REG, irq_src[1]); // clear int
		mfc_set_cmd_l_reg(charger, 0x20, MFC_CMD_CLEAR_INT_MASK); // command

		msleep(10);
	}
}

static irqreturn_t mfc_wpc_irq_thread(int irq, void *irq_data)
{
	struct mfc_charger_data *charger = irq_data;
	int wc_w_state_irq;
	int ret;
	u8 irq_src[2];
	u8 reg_data;
//	u8 cnt = 0;

	if ((charger->chip_id == MFC_CHIP_LSI) && (charger->mst_off_lock == 1)) {
		pr_info("%s MST Off Lock!\n", __func__);
		return IRQ_NONE;
	}

	pr_info("%s !\n", __func__);
	wake_lock(&charger->wpc_wake_lock);

	ret = mfc_reg_read(charger->client, MFC_INT_A_L_REG, &irq_src[0]);
	ret = mfc_reg_read(charger->client, MFC_INT_A_H_REG, &irq_src[1]);

	wc_w_state_irq = gpio_get_value(charger->pdata->wpc_int);
	pr_info("%s wc_w_state_irq = %d\n", __func__, wc_w_state_irq);

	if (ret < 0) {
		pr_err("%s: Failed to read interrupt source: %d\n",
			__func__, ret);
		wake_unlock(&charger->wpc_wake_lock);
		//return IRQ_NONE;
		goto INT_ERROR;
	}

	if (irq_src[1] & MFC_STAT_H_AC_MISSING_DET_MASK) {
		pr_info("%s 1AC Missing ! : MST on REV : %d\n", __func__, charger->pdata->wc_ic_rev);
		mfc_mst_routine(charger, irq_src);
	}

	pr_info("%s: interrupt source(0x%x)\n", __func__, irq_src[1] << 8 | irq_src[0]);
	mfc_get_firmware_version(charger, MFC_RX_FIRMWARE);

	if (irq_src[0] & MFC_STAT_L_OP_MODE_MASK) {
		ret = mfc_reg_read(charger->client, MFC_SYS_OP_MODE_REG, &reg_data);
		reg_data &= 0x0C; /* use only [3:2]bit of sys_op_mode register for MST */
		pr_info("%s MODE CHANGE IRQ ! (0x%x)\n", __func__, reg_data);
	}

	if ((irq_src[0] & MFC_STAT_L_OVER_VOL_MASK) ||
		(irq_src[0] & MFC_STAT_L_OVER_CURR_MASK) ||
		(irq_src[0] & MFC_STAT_L_OVER_TEMP_MASK)) {
		pr_info("%s ABNORMAL STAT IRQ !\n", __func__);
		//ret = mfc_reg_read(charger->client, MFC_SYS_OP_MODE_REG, &reg_data);
	}

	if (irq_src[0] & MFC_STAT_L_INT_LPM_MASK) {
		pr_info("%s INT LPM IRQ !\n", __func__);
	}

	if (irq_src[0] & MFC_STAT_L_BT2AP_DATA_MASK) {
		pr_info("%s BT2AP DATA IRQ !\n", __func__);
		if (!delayed_work_pending(&charger->wpc_isr_work))
			queue_delayed_work(charger->wqueue, &charger->wpc_isr_work, msecs_to_jiffies(1000));
	}


	if (irq_src[1] & MFC_STAT_H_TX_DATA_RECEIVED_MASK) {
		pr_info("%s TX RECEIVED IRQ !\n", __func__);
		if (charger->pdata->cable_type == MFC_PAD_WPC_STAND ||
			charger->pdata->cable_type == MFC_PAD_WPC_STAND_HV)
			pr_info("%s Don't run ISR_WORK for NO ACK !\n", __func__);
		else if (!delayed_work_pending(&charger->wpc_isr_work))
			queue_delayed_work(charger->wqueue, &charger->wpc_isr_work, msecs_to_jiffies(1000));
	}


	if (irq_src[1] & MFC_STAT_H_TX_OVER_CURR_MASK) {
		pr_info("%s TX OVER CURRENT IRQ !\n", __func__);
	}

	if (irq_src[1] & MFC_STAT_H_TX_OVER_TEMP_MASK) {
		pr_info("%s TX OVER TEMP IRQ !\n", __func__);
	}

	if (irq_src[1] & MFC_STAT_H_TX_CON_DISCON_MASK) {
		pr_info("%s TX CONNECT IRQ !\n", __func__);
		charger->pdata->tx_status = SEC_TX_POWER_TRANSFER;
	}

	/* clear intterupt */
	mfc_reg_write(charger->client, MFC_INT_A_CLEAR_L_REG, irq_src[0]); // clear int
	mfc_reg_write(charger->client, MFC_INT_A_CLEAR_H_REG, irq_src[1]); // clear int
	mfc_set_cmd_l_reg(charger, 0x20, MFC_CMD_CLEAR_INT_MASK); // command

	/* debug */
	ret = mfc_reg_read(charger->client, MFC_INT_A_L_REG, &irq_src[0]);
	ret = mfc_reg_read(charger->client, MFC_INT_A_H_REG, &irq_src[1]);
	wc_w_state_irq = gpio_get_value(charger->pdata->wpc_int);
	pr_info("%s wc_w_state_irq = %d\n", __func__, wc_w_state_irq);
	wake_unlock(&charger->wpc_wake_lock);

	return IRQ_HANDLED;

INT_ERROR:
	/* clear intterupt */
	pr_info("%s interrup error!\n", __func__);
	mfc_reg_write(charger->client, MFC_INT_A_CLEAR_L_REG, irq_src[0]); // clear int
	mfc_reg_write(charger->client, MFC_INT_A_CLEAR_H_REG, irq_src[1]); // clear int
	mfc_set_cmd_l_reg(charger, 0x20, MFC_CMD_CLEAR_INT_MASK); // command
	wake_unlock(&charger->wpc_wake_lock);

	return IRQ_NONE;
}


static int mfc_chg_parse_dt(struct device *dev,
		mfc_charger_platform_data_t *pdata)
{
	int ret = 0;
	struct device_node *np  = dev->of_node;
	enum of_gpio_flags irq_gpio_flags;
	int len,i;
	const u32 *p;

	if (!np) {
		pr_err("%s np NULL\n", __func__);
		return 1;
	} else {
		p = of_get_property(np, "battery,fod_wpc_data", &len);
		if (p) {
			len = len / sizeof(u32);
			pdata->fod_wpc_data = kzalloc(sizeof(*pdata->fod_wpc_data) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,fod_wpc_data",
							 pdata->fod_wpc_data, len);
			pdata->fod_data_check = 1;

			for (i = 0; i < len; i++)
				pr_info("%s fod WPC data = %d ", __func__, pdata->fod_wpc_data[i]);
		} else {
			pdata->fod_data_check = 0;
			pr_err("%s there is not fod_wpc_data\n", __func__);
		}

		p = of_get_property(np, "battery,fod_pma_data", &len);
		if (p) {
			len = len / sizeof(u32);
			pdata->fod_pma_data = kzalloc(sizeof(*pdata->fod_pma_data) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,fod_pma_data",
							 pdata->fod_pma_data, len);
			pdata->fod_data_check = 1;

			for (i = 0; i < len; i++)
				pr_info("%s fod PMA data = %d ", __func__, pdata->fod_pma_data[i]);
		} else {
			pdata->fod_data_check = 0;
			pr_err("%s there is not fod_pma_data\n", __func__);
		}

		p = of_get_property(np, "battery,fod_a4wp_data", &len);
		if (p) {
			len = len / sizeof(u32);
			pdata->fod_a4wp_data = kzalloc(sizeof(*pdata->fod_a4wp_data) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,fod_a4wp_data",
							 pdata->fod_a4wp_data, len);
			pdata->fod_data_check = 1;

			for (i = 0; i < len; i++)
				pr_info("%s fod A4WP data = %d ", __func__, pdata->fod_a4wp_data[i]);
		} else {
			pdata->fod_data_check = 0;
			pr_err("%s there is not fod_a4wp_data\n", __func__);
		}

		p = of_get_property(np, "battery,fod_wpc_data_cv", &len);
		if (p) {
			len = len / sizeof(u32);
			pdata->fod_wpc_data_cv = kzalloc(sizeof(*pdata->fod_wpc_data_cv) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,fod_wpc_data_cv",
							 pdata->fod_wpc_data_cv, len);
			pdata->fod_data_check = 1;

			for (i = 0; i < len; i++)
				pr_info("%s fod WPC data_cv = %d ", __func__, pdata->fod_wpc_data_cv[i]);
		} else {
			pdata->fod_data_check = 0;
			pr_err("%s there is not fod_wpc_data_cv\n", __func__);
		}

		p = of_get_property(np, "battery,fod_pma_data_cv", &len);
		if (p) {
			len = len / sizeof(u32);
			pdata->fod_pma_data_cv = kzalloc(sizeof(*pdata->fod_pma_data_cv) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,fod_pma_data_cv",
							 pdata->fod_pma_data_cv, len);
			pdata->fod_data_check = 1;

			for (i = 0; i < len; i++)
				pr_info("%s fod PMA data_cv = %d ", __func__, pdata->fod_pma_data_cv[i]);
		} else {
			pdata->fod_data_check = 0;
			pr_err("%s there is not fod_pma_data_cv\n", __func__);
		}

		p = of_get_property(np, "battery,fod_a4wp_data_cv", &len);
		if (p) {
			len = len / sizeof(u32);
			pdata->fod_a4wp_data_cv = kzalloc(sizeof(*pdata->fod_a4wp_data_cv) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,fod_a4wp_data_cv",
							 pdata->fod_a4wp_data_cv, len);
			pdata->fod_data_check = 1;

			for (i = 0; i < len; i++)
				pr_info("%s fod A4WP data_cv = %d ", __func__, pdata->fod_a4wp_data_cv[i]);
		} else {
			pdata->fod_data_check = 0;
			pr_err("%s there is not fod_a4wp_data_cv\n", __func__);
		}

		p = of_get_property(np, "battery,fod_hero_5v_data", &len);
		if (p) {
			len = len / sizeof(u32);
			pdata->fod_hero_5v_data = kzalloc(sizeof(*pdata->fod_hero_5v_data) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,fod_hero_5v_data",
							 pdata->fod_hero_5v_data, len);

			for (i = 0; i < len; i++)
				pr_info("%s fod Hero 5V data = 0x%x ", __func__, pdata->fod_hero_5v_data[i]);
		} else {
			pr_err("%s there is not fod_hero_5v_data\n", __func__);
		}

		ret = of_property_read_string(np,
			"battery,wireless_charger_name", (char const **)&pdata->wireless_charger_name);
		if (ret < 0)
			pr_info("%s: Wireless Charger name is Empty\n", __func__);

		ret = of_property_read_string(np,
			"battery,charger_name", (char const **)&pdata->wired_charger_name);
		if (ret < 0)
			pr_info("%s: Charger name is Empty\n", __func__);

		ret = of_property_read_string(np,
			"battery,fuelgauge_name", (char const **)&pdata->fuelgauge_name);
		if (ret < 0)
			pr_info("%s: Fuelgauge name is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_cc_cv_vout",
						&pdata->wpc_cc_cv_vout);
		if (ret < 0)
			pr_info("%s: wpc_cv_call_vout is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_cv_call_vout",
						&pdata->wpc_cv_call_vout);
		if (ret < 0)
			pr_info("%s: wpc_cv_call_vout is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_cc_call_vout",
						&pdata->wpc_cc_call_vout);
		if (ret < 0)
			pr_info("%s: wpc_cc_call_vout is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,hv_vout_wa",
						&pdata->hv_vout_wa);
		if (ret < 0) {
			pr_info("%s: no need hv_vout_wa.\n", __func__);
			pdata->hv_vout_wa = 0;
		}

		ret = of_property_read_u32(np, "battery,mst_switch_delay",
						&pdata->mst_switch_delay);
		if (ret < 0) {
			pr_info("%s: mst_switch_delay is Empty\n", __func__);
			pdata->mst_switch_delay = 1000; /* set default value (dream) */
		}

		ret = of_property_read_u32(np, "battery,wc_cover_rpp",
						&pdata->wc_cover_rpp);
		if (ret < 0) {
			pr_info("%s: fail to read wc_cover_rpp.\n", __func__);
			pdata->wc_cover_rpp = 0x55;
		}

		ret = of_property_read_u32(np, "battery,wc_hv_rpp",
						&pdata->wc_hv_rpp);
		if (ret < 0) {
			pr_info("%s: fail to read wc_hv_rpp.\n", __func__);
			pdata->wc_hv_rpp = 0x40;
		}

		/* wpc_det */
		ret = pdata->wpc_det = of_get_named_gpio_flags(np, "battery,wpc_det",
				0, &irq_gpio_flags);
		if (ret < 0) {
			dev_err(dev, "%s : can't get wpc_det\r\n", __FUNCTION__);
		} else {
			pdata->irq_wpc_det = gpio_to_irq(pdata->wpc_det);
			pr_info("%s wpc_det = 0x%x, irq_wpc_det = 0x%x\n", __func__, pdata->wpc_det, pdata->irq_wpc_det);
		}
		/* wpc_int (This GPIO means MFC_AP_INT) */
		ret = pdata->wpc_int = of_get_named_gpio_flags(np, "battery,wpc_int",
				0, &irq_gpio_flags);
		if (ret < 0) {
			dev_err(dev, "%s : can't wpc_int\r\n", __FUNCTION__);
		} else {
			pdata->irq_wpc_int = gpio_to_irq(pdata->wpc_int);
			pr_info("%s wpc_int = 0x%x, irq_wpc_int = 0x%x\n", __func__, pdata->wpc_int, pdata->irq_wpc_int);
		}

		/* mst_pwr_en (MST PWR EN) */
		ret = pdata->mst_pwr_en = of_get_named_gpio_flags(np, "battery,mst_pwr_en",
				0, &irq_gpio_flags);
		if (ret < 0) {
			dev_err(dev, "%s : can't mst_pwr_en\r\n", __FUNCTION__);
		}

		/* wpc_en (MFC EN) */
		ret = pdata->wpc_en = of_get_named_gpio_flags(np, "battery,wpc_en",
				0, &irq_gpio_flags);
		if (ret < 0) {
			dev_err(dev, "%s : can't wpc_en\r\n", __FUNCTION__);
		}

		return 0;
	}
}

static ssize_t mfc_store_addr(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mfc_charger_data *charger = power_supply_get_drvdata(psy);
	int x;
	if (sscanf(buf, "0x%10x\n", &x) == 1) {
		charger->addr = x;
	}
	return count;
}

static ssize_t mfc_show_addr(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mfc_charger_data *charger = power_supply_get_drvdata(psy);
	return sprintf(buf, "0x%x\n", charger->addr);
}

static ssize_t mfc_store_size(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mfc_charger_data *charger = power_supply_get_drvdata(psy);
	int x;
	if (sscanf(buf, "%10d\n", &x) == 1) {
		charger->size = x;
	}
	return count;
}

static ssize_t mfc_show_size(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mfc_charger_data *charger = power_supply_get_drvdata(psy);

	return sprintf(buf, "0x%x\n", charger->size);
}
static ssize_t mfc_store_data(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mfc_charger_data *charger = power_supply_get_drvdata(psy);
	int x;

	if (sscanf(buf, "0x%10x", &x) == 1) {
		u8 data = x;
		if (mfc_reg_write(charger->client, charger->addr, data) < 0)
		{
			dev_info(charger->dev,
					"%s: addr: 0x%x write fail\n", __func__, charger->addr);
		}
	}
	return count;
}

static ssize_t mfc_show_data(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mfc_charger_data *charger = power_supply_get_drvdata(psy);
	u8 data;
	int i, count = 0;;
	if (charger->size == 0)
		charger->size = 1;

	for (i = 0; i < charger->size; i++) {
		if (mfc_reg_read(charger->client, charger->addr+i, &data) < 0) {
			dev_info(charger->dev,
					"%s: read fail\n", __func__);
			count += sprintf(buf+count, "addr: 0x%x read fail\n", charger->addr+i);
			continue;
		}
		count += sprintf(buf+count, "addr: 0x%x, data: 0x%x\n", charger->addr+i,data);
	}
	return count;
}

static DEVICE_ATTR(addr, 0644, mfc_show_addr, mfc_store_addr);
static DEVICE_ATTR(size, 0644, mfc_show_size, mfc_store_size);
static DEVICE_ATTR(data, 0644, mfc_show_data, mfc_store_data);

static struct attribute *mfc_attributes[] = {
	&dev_attr_addr.attr,
	&dev_attr_size.attr,
	&dev_attr_data.attr,
	NULL
};

static const struct attribute_group mfc_attr_group = {
	.attrs = mfc_attributes,
};

static const struct power_supply_desc mfc_charger_power_supply_desc = {
	.name = "mfc-charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = mfc_charger_props,
	.num_properties = ARRAY_SIZE(mfc_charger_props),
	.get_property = mfc_chg_get_property,
	.set_property = mfc_chg_set_property,
};

static void mfc_wpc_int_req_work(struct work_struct *work)
{
	struct mfc_charger_data *charger =
		container_of(work, struct mfc_charger_data, wpc_int_req_work.work);

	int ret = 0;

	pr_info("%s\n", __func__);
	/* wpc_irq */
	if (charger->pdata->irq_wpc_int) {
		msleep(100);
		ret = request_threaded_irq(charger->pdata->irq_wpc_int,
				NULL, mfc_wpc_irq_thread,
				IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT,
				"wpc-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
		}
	}
	if (ret < 0)
		free_irq(charger->pdata->irq_wpc_det, NULL);
}

static int mfc_charger_probe(
						struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct device_node *of_node = client->dev.of_node;
	struct mfc_charger_data *charger;
	mfc_charger_platform_data_t *pdata = client->dev.platform_data;
	struct power_supply_config mfc_cfg = {};
	int ret = 0;
	int wc_w_state_irq;

	dev_info(&client->dev,
		"%s: MFC Charger Driver Loading\n", __func__);

	if (of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = mfc_chg_parse_dt(&client->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		pdata = client->dev.platform_data;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (charger == NULL) {
		dev_err(&client->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_wpc_nomem;
	}
	charger->dev = &client->dev;

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(client->adapter);
		dev_err(charger->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}

	charger->client = client;
	charger->pdata = pdata;

    pr_info("%s: %s\n", __func__, charger->pdata->wireless_charger_name);

	i2c_set_clientdata(client, charger);

	charger->pdata->cable_type = MFC_PAD_NONE;
	charger->pdata->is_charging = 0;
	charger->pdata->tx_status = 0;
	charger->pdata->cs100_status = 0;
	charger->pdata->capacity = 101;
	charger->pdata->vout_status = MFC_VOUT_5V;
	charger->pdata->opfq_cnt = 0;

	charger->is_mst_on = MST_MODE_0;
	charger->chip_id = MFC_CHIP_IDT;
	charger->is_otg_on = false;
	charger->led_cover = 0;
	charger->vout_mode = MFC_VOUT_5V;
	charger->is_full_status = 0;
	charger->is_afc_tx = false;

	mutex_init(&charger->io_lock);
	mutex_init(&charger->fw_lock);

	/* wpc_det */
	if (charger->pdata->irq_wpc_det) {
		INIT_DELAYED_WORK(&charger->wpc_det_work, mfc_wpc_det_work);
		INIT_DELAYED_WORK(&charger->wpc_opfq_work, mfc_wpc_opfq_work);
	}

	/* wpc_irq (INT_A) */
	if (charger->pdata->irq_wpc_int) {
		INIT_DELAYED_WORK(&charger->wpc_isr_work, mfc_wpc_isr_work);
		INIT_DELAYED_WORK(&charger->wpc_tx_id_work, mfc_wpc_tx_id_work);
		INIT_DELAYED_WORK(&charger->wpc_int_req_work, mfc_wpc_int_req_work);
	}
	INIT_DELAYED_WORK(&charger->wpc_vout_mode_work, mfc_wpc_vout_mode_work);
	INIT_DELAYED_WORK(&charger->wpc_afc_vout_work, mfc_wpc_afc_vout_work);
	INIT_DELAYED_WORK(&charger->wpc_fw_update_work, mfc_wpc_fw_update_work);
	INIT_DELAYED_WORK(&charger->wpc_cm_fet_work, mfc_wpc_cm_fet_work);
/*#if !defined(CONFIG_SEC_FACTORY)
	INIT_DELAYED_WORK(&charger->wpc_fw_booting_work, mfc_wpc_fw_booting_work);
#endif*/

	/*
	* Default Idle voltage of the INT_A is LOW.
	* Prevent the un-wanted INT_A Falling handling.
	* This is a work-around, and will be fixed by the revision.
	*/
	INIT_DELAYED_WORK(&charger->mst_off_work, mfc_mst_off_work);

	mfc_cfg.drv_data = charger;
	charger->psy_chg = power_supply_register(charger->dev, &mfc_charger_power_supply_desc, &mfc_cfg);
	if (IS_ERR(charger->psy_chg)) {
		ret = PTR_ERR(charger->psy_chg);
		pr_err("%s: Failed to Register psy_chg(%d)\n", __func__, ret);
		goto err_supply_unreg;
	}

	charger->wqueue = create_singlethread_workqueue("mfc_workqueue");
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}

	wake_lock_init(&charger->wpc_wake_lock, WAKE_LOCK_SUSPEND,
			"wpc_wakelock");
	wake_lock_init(&charger->wpc_update_lock, WAKE_LOCK_SUSPEND,
			"wpc_update_lock");
	wake_lock_init(&charger->wpc_opfq_lock, WAKE_LOCK_SUSPEND,
			"wpc_opfq_lock");
	wake_lock_init(&charger->wpc_afc_vout_lock, WAKE_LOCK_SUSPEND,
			"wpc_afc_vout_lock");
	wake_lock_init(&charger->wpc_vout_mode_lock, WAKE_LOCK_SUSPEND,
			"wpc_vout_mode_lock");

	/* Enable interrupts after battery driver load */
	/* wpc_det */
	if (charger->pdata->irq_wpc_det) {
		ret = request_threaded_irq(charger->pdata->irq_wpc_det,
				NULL, mfc_wpc_det_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT,
				"wpd-det-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
			goto err_irq_wpc_det;
		}
	}

	/* wpc_irq */
	queue_delayed_work(charger->wqueue, &charger->wpc_int_req_work, msecs_to_jiffies(100));

	wc_w_state_irq = gpio_get_value(charger->pdata->wpc_int);
	pr_info("%s wc_w_state_irq = %d\n", __func__, wc_w_state_irq);
	if (gpio_get_value(charger->pdata->wpc_det)) {
		u8 irq_src[2];
		pr_info("%s: Charger interrupt occured during lpm\n", __func__);

		mfc_reg_read(charger->client, MFC_INT_A_L_REG, &irq_src[0]);
		mfc_reg_read(charger->client, MFC_INT_A_H_REG, &irq_src[1]);
		/* clear intterupt */
		mfc_reg_write(charger->client, MFC_INT_A_CLEAR_L_REG, irq_src[0]); // clear int
		mfc_reg_write(charger->client, MFC_INT_A_CLEAR_H_REG, irq_src[1]); // clear int
		mfc_set_cmd_l_reg(charger, 0x20, MFC_CMD_CLEAR_INT_MASK); // command
		queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);
		if (!wc_w_state_irq && !delayed_work_pending(&charger->wpc_isr_work))
			queue_delayed_work(charger->wqueue, &charger->wpc_isr_work, msecs_to_jiffies(2000));
	}
/*#if !defined(CONFIG_SEC_FACTORY)
	else if (!lpcharge) {
		pr_info("%s: call wpc_fw_booting_work for firmware update\n", __func__);
		queue_delayed_work(charger->wqueue, &charger->wpc_fw_booting_work, 0);
	}
#endif*/

	ret = sysfs_create_group(&charger->psy_chg->dev.kobj, &mfc_attr_group);
	if (ret) {
		dev_info(&client->dev,
			"%s: sysfs_create_group failed\n", __func__);
	}
	charger->is_probed = true;
	dev_info(&client->dev,
		"%s: MFC Charger Driver Loaded\n", __func__);

	device_init_wakeup(charger->dev, 1);
	return 0;

err_irq_wpc_det:
err_pdata_free:
	power_supply_unregister(charger->psy_chg);
err_supply_unreg:
	mutex_destroy(&charger->io_lock);
	mutex_destroy(&charger->fw_lock);
err_i2cfunc_not_support:
	kfree(charger);
err_wpc_nomem:
err_parse_dt:
	devm_kfree(&client->dev, pdata);
	return ret;
}

static int mfc_charger_remove(struct i2c_client *client)
{
	return 0;
}

#if defined(CONFIG_PM)
static int mfc_charger_suspend(struct device *dev)
{
	struct mfc_charger_data *charger = dev_get_drvdata(dev);

	if (device_may_wakeup(charger->dev)){
		enable_irq_wake(charger->pdata->irq_wpc_int);
		enable_irq_wake(charger->pdata->irq_wpc_det);
	}
	disable_irq(charger->pdata->irq_wpc_int);
	disable_irq(charger->pdata->irq_wpc_det);

	return 0;
}

static int mfc_charger_resume(struct device *dev)
{
	struct mfc_charger_data *charger = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

	if (device_may_wakeup(charger->dev)) {
		disable_irq_wake(charger->pdata->irq_wpc_int);
		disable_irq_wake(charger->pdata->irq_wpc_det);
	}
	enable_irq(charger->pdata->irq_wpc_int);
	enable_irq(charger->pdata->irq_wpc_det);

	return 0;
}
#else
#define mfc_charger_suspend NULL
#define mfc_charger_resume NULL
#endif

static void mfc_charger_shutdown(struct i2c_client *client)
{
	struct mfc_charger_data *charger = i2c_get_clientdata(client);

	pr_info("%s\n", __func__);
	if (charger->pdata->is_charging)
		mfc_set_vrect_adjust(charger, MFC_HEADROOM_1);
}

static const struct i2c_device_id mfc_charger_id_table[] = {
	{ "mfc-charger", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, mfc_id_table);

#ifdef CONFIG_OF
static struct of_device_id mfc_charger_match_table[] = {
	{ .compatible = "idt,mfc-charger",},
	{},
};
#else
#define mfc_charger_match_table NULL
#endif

const struct dev_pm_ops mfc_pm = {
	.suspend = mfc_charger_suspend,
	.resume = mfc_charger_resume,
};

static struct i2c_driver mfc_charger_driver = {
	.driver = {
		.name	= "mfc-charger",
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm = &mfc_pm,
#endif /* CONFIG_PM */
		.of_match_table = mfc_charger_match_table,
	},
	.shutdown	= mfc_charger_shutdown,
	.probe	= mfc_charger_probe,
	.remove	= mfc_charger_remove,
	.id_table	= mfc_charger_id_table,
};

static int __init mfc_charger_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&mfc_charger_driver);
}

static void __exit mfc_charger_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&mfc_charger_driver);
}

module_init(mfc_charger_init);
module_exit(mfc_charger_exit);

MODULE_DESCRIPTION("Samsung MFC Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
