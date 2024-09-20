/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/exynos-ss.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <video/mipi_display.h>
#include <soc/samsung/cal-if.h>
#include <dt-bindings/clock/exynos9810.h>
#if defined(CONFIG_ION_EXYNOS)
#include <linux/exynos_iovmm.h>
#endif

#include <linux/of_reserved_mem.h>
#include "../../../../../mm/internal.h"

#include "decon.h"
#include "dsim.h"

#if defined(CONFIG_EXYNOS_MASS_PANEL)
#include "decon_board.h"
#include "panels/dsim_panel.h"
#include "panels/dd.h"
#endif

int dsim_log_level = 6;

struct dsim_device *dsim_drvdata[MAX_DSIM_CNT];
EXPORT_SYMBOL(dsim_drvdata);

static char *dsim_state_names[] = {
	"INIT",
	"ON",
#ifdef CONFIG_SUPPORT_DOZE
	"DOZE",
#endif
	"ULPS",
#ifdef CONFIG_SUPPORT_DOZE
	"DOZE_SUSPEND",
#endif
	"OFF",
};

static int dsim_runtime_suspend(struct device *dev);
static int dsim_runtime_resume(struct device *dev);

#define PREFIX_LEN      40
#define ROW_LEN         32
static void dsim_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	int i, row;

	for (i = 0; i < len; i += ROW_LEN) {
		p = buf - regs + i;

		if (len - i < ROW_LEN)
			row = len - i;
		else
			row = ROW_LEN;

		snprintf(prefix_buf, sizeof(prefix_buf), "[%08lX] ", p);
		print_hex_dump(KERN_NOTICE, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

static void __dsim_dump(struct dsim_device *dsim)
{
	/* change to updated register read mode (meaning: SHADOW in DECON) */
	dsim_info("=== DSIM %d LINK SFR DUMP ===\n", dsim->id);
	dsim_reg_enable_shadow_read(dsim->id, 0);
	dsim_print_hex_dump(dsim->res.regs, dsim->res.regs + 0x0000, 0xFC);

#if defined(CONFIG_SOC_EXYNOS9810)
	dsim_info("=== DSIM %d DPHY SFR DUMP ===\n", dsim->id);
	/* DPHY dump */
	/* PMSK */
	dsim_print_hex_dump(dsim->res.phy_regs, dsim->res.phy_regs + 0x0c00, 0x40);

	/* CLOCK lane */
	dsim_print_hex_dump(dsim->res.phy_regs, dsim->res.phy_regs + 0x1080, 0x40);

	/* Data lane : D0 */
	dsim_print_hex_dump(dsim->res.phy_regs, dsim->res.phy_regs + 0x1480, 0x30);

	/* Data lane : D1 */
	dsim_print_hex_dump(dsim->res.phy_regs, dsim->res.phy_regs + 0x1880, 0x30);

	/* Data lane : D2 */
	dsim_print_hex_dump(dsim->res.phy_regs, dsim->res.phy_regs + 0x1c80, 0x30);

	/* Data lane : D3 */
	dsim_print_hex_dump(dsim->res.phy_regs, dsim->res.phy_regs + 0x2080, 0x30);
#endif

	/* restore to avoid size mismatch (possible config error at DECON) */
	dsim_reg_enable_shadow_read(dsim->id, 1);
}

static void dsim_dump(struct dsim_device *dsim)
{
	u32 i = 0, max_dsim = 1;
	struct decon_device *decon = get_decon_drvdata(0);
	struct dsim_device *d;

	if (decon)
		max_dsim = (decon->dt.dsi_mode == DSI_MODE_DUAL_DSI) ? 2 : 1;

	for (i = 0; i < max_dsim; i++) {
		d = get_dsim_drvdata(i);
		if (!d)
			continue;

		dsim_info("=== DSIM%d SFR DUMP ===\n", dsim->id);
		__dsim_dump(d);
		/* Show panel status */
		call_panel_ops(d, dump, d);
	}
}

static void dsim_long_data_wr(struct dsim_device *dsim, unsigned long d0, u32 d1)
{
	unsigned int data_cnt = 0, payload = 0;

	/* in case that data count is more then 4 */
	for (data_cnt = 0; data_cnt < d1; data_cnt += 4) {
		/*
		 * after sending 4bytes per one time,
		 * send remainder data less then 4.
		 */
		if ((d1 - data_cnt) < 4) {
			if ((d1 - data_cnt) == 3) {
				payload = *(u8 *)(d0 + data_cnt) |
				    (*(u8 *)(d0 + (data_cnt + 1))) << 8 |
					(*(u8 *)(d0 + (data_cnt + 2))) << 16;
			dsim_dbg("count = 3 payload = %x, %x %x %x\n",
				payload, *(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)),
				*(u8 *)(d0 + (data_cnt + 2)));
			} else if ((d1 - data_cnt) == 2) {
				payload = *(u8 *)(d0 + data_cnt) |
					(*(u8 *)(d0 + (data_cnt + 1))) << 8;
			dsim_dbg("count = 2 payload = %x, %x %x\n", payload,
				*(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)));
			} else if ((d1 - data_cnt) == 1) {
				payload = *(u8 *)(d0 + data_cnt);
			}

			dsim_reg_wr_tx_payload(dsim->id, payload);
		/* send 4bytes per one time. */
		} else {
			payload = *(u8 *)(d0 + data_cnt) |
				(*(u8 *)(d0 + (data_cnt + 1))) << 8 |
				(*(u8 *)(d0 + (data_cnt + 2))) << 16 |
				(*(u8 *)(d0 + (data_cnt + 3))) << 24;

			dsim_dbg("count = 4 payload = %x, %x %x %x %x\n",
				payload, *(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)),
				*(u8 *)(d0 + (data_cnt + 2)),
				*(u8 *)(d0 + (data_cnt + 3)));

			dsim_reg_wr_tx_payload(dsim->id, payload);
		}
	}
}

static int dsim_wait_for_cmd_fifo_empty(struct dsim_device *dsim, bool must_wait)
{
	int ret = 0;

	if (!must_wait) {
		/* timer is running, but already command is transferred */
		if (dsim_reg_header_fifo_is_empty(dsim->id))
			del_timer(&dsim->cmd_timer);

		dsim_dbg("%s Doesn't need to wait fifo_completion\n", __func__);
		return ret;
	} else {
		del_timer(&dsim->cmd_timer);
		dsim_dbg("%s Waiting for fifo_completion...\n", __func__);
	}

	if (!wait_for_completion_timeout(&dsim->ph_wr_comp, MIPI_WR_TIMEOUT)) {
		if (dsim_reg_header_fifo_is_empty(dsim->id)) {
			reinit_completion(&dsim->ph_wr_comp);
			dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
			return 0;
		}
		ret = -ETIMEDOUT;
	}

	if (IS_DSIM_ON_STATE(dsim) && (ret == -ETIMEDOUT)) {
		dsim_err("%s have timed out\n", __func__);
		__dsim_dump(dsim);
	}
	return ret;
}

/* wait for until SFR fifo is empty */
int dsim_wait_for_cmd_done(struct dsim_device *dsim)
{
	int ret = 0;
	/* FIXME: hiber only support for DECON0 */
	struct decon_device *decon = get_decon_drvdata(0);

	decon_hiber_block_exit(decon);

	mutex_lock(&dsim->cmd_lock);
	ret = dsim_wait_for_cmd_fifo_empty(dsim, true);
	mutex_unlock(&dsim->cmd_lock);

	decon_hiber_unblock(decon);

	return ret;
}

static bool dsim_fifo_empty_needed(struct dsim_device *dsim, unsigned int data_id,
	unsigned long data0)
{
	/* read case or partial update command */
	if (data_id == MIPI_DSI_DCS_READ
			|| ((data_id == MIPI_DSI_DCS_SHORT_WRITE
					|| data_id == MIPI_DSI_DCS_SHORT_WRITE_PARAM)
				&& (data0 == MIPI_DCS_SOFT_RESET
					|| data0 == MIPI_DCS_SET_DISPLAY_OFF
					|| data0 == MIPI_DCS_SET_DISPLAY_ON
					|| data0 == MIPI_DCS_ENTER_SLEEP_MODE
					|| data0 == MIPI_DCS_EXIT_SLEEP_MODE))
			/* for poc write */
			|| data0 == 0xC1
			|| data0 == 0xC0
			|| data0 == MIPI_DCS_SET_COLUMN_ADDRESS
			|| data0 == MIPI_DCS_SET_PAGE_ADDRESS) {
		dsim_dbg("%s: id:%d, data=%ld\n", __func__, data_id, data0);
		return true;
	}

	/* Check a FIFO level whether writable or not */
	if (!dsim_reg_is_writable_fifo_state(dsim->id))
		return true;

	return false;
}

#if defined(CONFIG_EXYNOS_DUAL_DSI)
int dsim_write_data_dual(struct dsim_device *dsim, u32 id, unsigned long d0, u32 d1)
{
	int ret0 = 0;
	int ret1 = 0;
	bool must_wait0 = true;
	bool must_wait1 = true;
	struct decon_device *decon = get_decon_drvdata(0);
	struct dsim_device *dsim0 = get_dsim_drvdata(0);
	struct dsim_device *dsim1 = get_dsim_drvdata(1);

	if (!dsim0 || !dsim1)
		return 0;

	if (!dsim0->priv.lcdconnected)
		return 0;

	decon_hiber_block_exit(decon);

	mutex_lock(&dsim0->cmd_lock);
	mutex_lock(&dsim1->cmd_lock);
	if (!IS_DSIM_ON_STATE(dsim0) || !IS_DSIM_ON_STATE(dsim1)) {
		dsim_err("DSIM is not ready. D0.state(%d) D1.state(%d)\n",
				dsim0->state, dsim1->state);
		ret0 = -EINVAL;
		goto err_exit;
	}
	DPU_EVENT_LOG_CMD(&dsim0->sd, id, d0);
	dsim_write_data_dump(dsim0, id, d0, d1);
	DPU_EVENT_LOG_CMD(&dsim1->sd, id, d0);
	dsim_write_data_dump(dsim1, id, d0, d1);

	reinit_completion(&dsim0->ph_wr_comp);
	dsim_reg_clear_int(dsim0->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
	reinit_completion(&dsim1->ph_wr_comp);
	dsim_reg_clear_int(dsim1->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);

	/* Run write-fail dectector */
	mod_timer(&dsim0->cmd_timer, jiffies + MIPI_WR_TIMEOUT);
	mod_timer(&dsim1->cmd_timer, jiffies + MIPI_WR_TIMEOUT);

	switch (id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
	case MIPI_DSI_DSC_PRA:
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		dsim_reg_wr_tx_header(dsim0->id, id, d0, d1, false);
		dsim_reg_wr_tx_header(dsim1->id, id, d0, d1, false);
		must_wait0 = dsim_fifo_empty_needed(dsim0, id, d0);
		must_wait1 = dsim_fifo_empty_needed(dsim1, id, d0);
		break;

	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		dsim_reg_wr_tx_header(dsim0->id, id, d0, d1, true);
		dsim_reg_wr_tx_header(dsim1->id, id, d0, d1, true);
		must_wait0 = dsim_fifo_empty_needed(dsim0, id, d0);
		must_wait1 = dsim_fifo_empty_needed(dsim1, id, d0);
		break;

	/* long packet types of packet types for command. */
	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	case MIPI_DSI_DSC_PPS:
		dsim_long_data_wr(dsim0, d0, d1);
		dsim_reg_wr_tx_header(dsim0->id, id, d1 & 0xff,
				(d1 & 0xff00) >> 8, false);
		dsim_long_data_wr(dsim1, d0, d1);
		dsim_reg_wr_tx_header(dsim1->id, id, d1 & 0xff,
				(d1 & 0xff00) >> 8, false);
		must_wait0 = dsim_fifo_empty_needed(dsim0, id, *(u8 *)d0);
		must_wait1 = dsim_fifo_empty_needed(dsim1, id, *(u8 *)d0);
		break;

	default:
		dsim_info("data id %x is not supported.\n", id);
		ret0 = -EINVAL;
	}

	ret0 = dsim_wait_for_cmd_fifo_empty(dsim0, must_wait0);
	if (ret0 < 0) {
		dsim_err("ID(%2X): DSIM cmd wr timeout 0x%2x\n",
			id, (id == MIPI_DSI_GENERIC_LONG_WRITE || id == MIPI_DSI_DCS_LONG_WRITE) ? *(u8 *)d0 : (unsigned int)d0);
	}
	ret1 = dsim_wait_for_cmd_fifo_empty(dsim1, must_wait1);
	if (ret1 < 0) {
		dsim_err("ID(%2X): DSIM cmd wr timeout 0x%2x\n",
			id, (id == MIPI_DSI_GENERIC_LONG_WRITE || id == MIPI_DSI_DCS_LONG_WRITE) ? *(u8 *)d0 : (unsigned int)d0);
	}

err_exit:
	mutex_unlock(&dsim0->cmd_lock);
	mutex_unlock(&dsim1->cmd_lock);
	decon_hiber_unblock(decon);

	return (ret0 + ret1);
}

int dsim_reg_start_dual(struct dsim_device *dsim)
{
	if (dsim->state == DSIM_STATE_INIT) {
		dsim_reg_start(dsim->id);
	} else {
		if (dsim->id) {
			dsim_reg_start(0);
			dsim_reg_start(1);
		}
	}
	return 0;
}

#endif /* CONFIG_EXYNOS_DUAL_DSI */

int dsim_write_data(struct dsim_device *dsim, u32 id, unsigned long d0, u32 d1)
{
	int ret = 0;
	bool must_wait = true;
	struct decon_device *decon = get_decon_drvdata(0);

#if defined(CONFIG_EXYNOS_MASS_PANEL)
	if (!dsim->priv.lcdconnected)
		return 0;
#endif

	decon_hiber_block_exit(decon);

	mutex_lock(&dsim->cmd_lock);
	if (!IS_DSIM_ON_STATE(dsim)) {
		dsim_err("DSIM is not ready. state(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}
	DPU_EVENT_LOG_CMD(&dsim->sd, id, d0);
	dsim_write_data_dump(dsim, id, d0, d1);

	reinit_completion(&dsim->ph_wr_comp);
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);

	/* Run write-fail dectector */
	mod_timer(&dsim->cmd_timer, jiffies + MIPI_WR_TIMEOUT);

	switch (id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
	case MIPI_DSI_DSC_PRA:
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		dsim_reg_wr_tx_header(dsim->id, id, d0, d1, false);
		must_wait = dsim_fifo_empty_needed(dsim, id, d0);
		break;

	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		dsim_reg_wr_tx_header(dsim->id, id, d0, d1, true);
		must_wait = dsim_fifo_empty_needed(dsim, id, d0);
		break;

	/* long packet types of packet types for command. */
	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	case MIPI_DSI_DSC_PPS:
		dsim_long_data_wr(dsim, d0, d1);
		dsim_reg_wr_tx_header(dsim->id, id, d1 & 0xff,
				(d1 & 0xff00) >> 8, false);
		must_wait = dsim_fifo_empty_needed(dsim, id, *(u8 *)d0);
		break;

	default:
		dsim_info("data id %x is not supported.\n", id);
		ret = -EINVAL;
	}

	ret = dsim_wait_for_cmd_fifo_empty(dsim, must_wait);
	if (ret < 0) {
		dsim_err("ID(%2X): DSIM cmd wr timeout 0x%2x\n",
			id, (id == MIPI_DSI_GENERIC_LONG_WRITE || id == MIPI_DSI_DCS_LONG_WRITE) ? *(u8 *)d0 : (unsigned int)d0);
	}

err_exit:
	mutex_unlock(&dsim->cmd_lock);
	decon_hiber_unblock(decon);

	return ret;
}

int dsim_read_data(struct dsim_device *dsim, u32 id, u32 addr, u32 cnt, u8 *buf)
{
	u32 rx_fifo, rx_size = 0;
	int i, j, ret = 0;
	u32 rx_fifo_depth = DSIM_RX_FIFO_MAX_DEPTH;
	struct decon_device *decon = get_decon_drvdata(0);

#if defined(CONFIG_EXYNOS_MASS_PANEL)
	if (!dsim->priv.lcdconnected)
		return 0;
#endif

	decon_hiber_block_exit(decon);

	if (IS_DSIM_OFF_STATE(dsim)) {
		dsim_err("DSIM is not ready. state(%d)\n", dsim->state);
		decon_hiber_unblock(decon);
		return -EINVAL;
	}

	reinit_completion(&dsim->rd_comp);

	/* Init RX FIFO before read and clear DSIM_INTSRC */
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_RX_DATA_DONE);

	/* Set the maximum packet size returned */
	dsim_write_data(dsim,
		MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, cnt, 0);

	/* Read request */
	dsim_write_data(dsim, id, addr, 0);
	if (!wait_for_completion_timeout(&dsim->rd_comp, MIPI_RD_TIMEOUT)) {
		dsim_err("MIPI DSIM read Timeout! %2X, %2X, %d\n", id, addr, cnt);
		return -ETIMEDOUT;
	}

	mutex_lock(&dsim->cmd_lock);
	DPU_EVENT_LOG_CMD(&dsim->sd, id, (char)addr);

	do {
		rx_fifo = dsim_reg_get_rx_fifo(dsim->id);

		/* Parse the RX packet data types */
		switch (rx_fifo & 0xff) {
		case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
			ret = dsim_reg_rx_err_handler(dsim->id, rx_fifo);
			if (ret < 0) {
				__dsim_dump(dsim);
				goto exit;
			}
			break;
		case MIPI_DSI_RX_END_OF_TRANSMISSION:
			dsim_dbg("EoTp was received from LCD module.\n");
			break;
		case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
		case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
		case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
		case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
			dsim_dbg("Short Packet was received from LCD module.\n");
			for (i = 0; i < cnt; i++)
				buf[i] = (rx_fifo >> (8 + i * 8)) & 0xff;
			rx_size = cnt;
			break;
		case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
		case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
			dsim_dbg("Long Packet was received from LCD module.\n");
			rx_size = (rx_fifo & 0x00ffff00) >> 8;
			dsim_dbg("rx fifo : %8x, response : %x, rx_size : %d\n",
					rx_fifo, rx_fifo & 0xff, rx_size);
			if (rx_size > cnt) {
				dsim_err("rx size is invalid, rx_size: %d, cnt: %d\n", rx_size, cnt);
				rx_size = cnt;
			}
			/* Read data from RX packet payload */
			for (i = 0; i < rx_size >> 2; i++) {
				rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
				for (j = 0; j < 4; j++)
					buf[(i*4)+j] = (u8)(rx_fifo >> (j * 8)) & 0xff;
			}
			if (rx_size % 4) {
				rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
				for (j = 0; j < rx_size % 4; j++)
					buf[4 * i + j] =
						(u8)(rx_fifo >> (j * 8)) & 0xff;
			}
			break;
		default:
			dsim_err("Packet format is invaild.\n");
			__dsim_dump(dsim);
			ret = -EBUSY;
			goto exit;
		}
	} while (!dsim_reg_rx_fifo_is_empty(dsim->id) && --rx_fifo_depth);

	ret = rx_size;
	if (!rx_fifo_depth) {
		dsim_err("Check DPHY values about HS clk.\n");
		__dsim_dump(dsim);
		ret = -EBUSY;
	}
exit:
	mutex_unlock(&dsim->cmd_lock);
	decon_hiber_unblock(decon);

	return ret;
}

static void dsim_cmd_fail_detector(unsigned long arg)
{
	struct dsim_device *dsim = (struct dsim_device *)arg;
	struct decon_device *decon = get_decon_drvdata(0);

	decon_hiber_block(decon);

	dsim_dbg("%s +\n", __func__);
	if (IS_DSIM_OFF_STATE(dsim)) {
		dsim_err("%s: DSIM is not ready. state(%d)\n", __func__,
				dsim->state);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (!timer_pending(&dsim->cmd_timer)
			&& dsim_reg_header_fifo_is_empty(dsim->id)) {
		reinit_completion(&dsim->ph_wr_comp);
		dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
		goto exit;
	}

	__dsim_dump(dsim);

exit:
	decon_hiber_unblock(decon);
	dsim_dbg("%s -\n", __func__);
	return;
}

static void dsim_bts_print_info(struct bts_decon_info *info)
{
	int i;

	for (i = 0; i < BTS_DPP_MAX; ++i) {
		if (!info->dpp[i].used)
			continue;

		dsim_info("\t\tDPP[%d] b(%d) s(%d %d) d(%d %d %d %d) r(%d)\n",
				i, info->dpp[i].bpp,
				info->dpp[i].src_w, info->dpp[i].src_h,
				info->dpp[i].dst.x1, info->dpp[i].dst.x2,
				info->dpp[i].dst.y1, info->dpp[i].dst.y2,
				info->dpp[i].rotation);
	}
}

static void dsim_underrun_info(struct dsim_device *dsim)
{
#if defined(CONFIG_EXYNOS9810_BTS)
	struct decon_device *decon;
	int i;

	dsim_info("\tMIF(%lu), INT(%lu), DISP(%lu)\n",
			cal_dfs_get_rate(ACPM_DVFS_MIF),
			cal_dfs_get_rate(ACPM_DVFS_INT),
			cal_dfs_get_rate(ACPM_DVFS_DISP));

	for (i = 0; i < MAX_DECON_CNT; ++i) {
		decon = get_decon_drvdata(i);

		if (decon) {
			dsim_info("\tDECON%d: bw(%u %u), disp(%u %u), p(%u)\n",
					decon->id,
					decon->bts.prev_total_bw,
					decon->bts.total_bw,
					decon->bts.prev_max_disp_freq,
					decon->bts.max_disp_freq,
					decon->bts.peak);
			dsim_bts_print_info(&decon->bts.bts_info);
		}
	}
#endif
}

static irqreturn_t dsim_irq_handler(int irq, void *dev_id)
{
	unsigned int int_src;
	struct dsim_device *dsim = dev_id;
	struct decon_device *decon = get_decon_drvdata(0);

#ifdef CONFIG_EXYNOS_PD
	int active;
#endif
	spin_lock(&dsim->slock);

#ifdef CONFIG_EXYNOS_PD
	active = pm_runtime_active(dsim->dev);
	if (!active) {
		dsim_info("dsim power(%d), state(%d)\n", active, dsim->state);
		spin_unlock(&dsim->slock);
		return IRQ_HANDLED;
	}
#endif

	int_src = readl(dsim->res.regs + DSIM_INTSRC);

	if (int_src & DSIM_INTSRC_SFR_PH_FIFO_EMPTY) {
		del_timer(&dsim->cmd_timer);
		complete(&dsim->ph_wr_comp);
		dsim_dbg("dsim%d PH_FIFO_EMPTY irq occurs\n", dsim->id);
	}
	if (int_src & DSIM_INTSRC_RX_DATA_DONE)
		complete(&dsim->rd_comp);
	if (int_src & DSIM_INTSRC_FRAME_DONE)
		dsim_dbg("dsim%d framedone irq occurs\n", dsim->id);
	if (int_src & DSIM_INTSRC_ERR_RX_ECC) {
		dsim_err("RX ECC Multibit error was detected!\n");
		__dsim_dump(dsim);
	}

	if (int_src & DSIM_INTSRC_UNDER_RUN) {
		dsim->total_underrun_cnt++;
		dsim_info("dsim%d underrun irq occurs(%d)\n", dsim->id,
				dsim->total_underrun_cnt);
		dsim_underrun_info(dsim);
	}
	if (int_src & DSIM_INTSRC_VT_STATUS) {
		dsim_dbg("dsim%d vt_status(vsync) irq occurs\n", dsim->id);
		if (!dsim->id && decon) {
			decon->vsync.timestamp = ktime_get();
			wake_up_interruptible_all(&decon->vsync.wait);
		}
	}

	dsim_reg_clear_int(dsim->id, int_src);

	spin_unlock(&dsim->slock);

	return IRQ_HANDLED;
}

/* DPHY RESET is controlled by IP */
void dpu_sysreg_set_dphy(struct dsim_device *dsim, void __iomem *sysreg)
{
	u32 old = 0, val = 0, mask = 0;

	if (!sysreg) {
		dsim_err("%s: fail\n", __func__);
		return;
	}

	mask = val = dsim->res.ss_regs_bits;

	old = readl(sysreg + DISP_DPU_MIPI_PHY_CON);

	val = (old & ~mask) | (val & mask);

	writel(val, sysreg + DISP_DPU_MIPI_PHY_CON);
}

static void dsim_clocks_info(struct dsim_device *dsim)
{
}

static int dsim_get_clocks(struct dsim_device *dsim)
{
	dsim->res.aclk = devm_clk_get(dsim->dev, "aclk");
	if (IS_ERR_OR_NULL(dsim->res.aclk)) {
		dsim_err("failed to get aclk\n");
		return PTR_ERR(dsim->res.aclk);
	}

	return 0;
}

static int dsim_get_gpios(struct dsim_device *dsim)
{
	struct device *dev = dsim->dev;
	struct dsim_resources *res = &dsim->res;

	dsim_info("%s +\n", __func__);

	if (of_get_property(dev->of_node, "gpios", NULL) != NULL)  {
		/* panel reset */
		res->lcd_reset = of_get_gpio(dev->of_node, 0);
		if (res->lcd_reset < 0) {
			dsim_err("failed to get lcd reset GPIO");
			return -ENODEV;
		}
		res->lcd_power[0] = of_get_gpio(dev->of_node, 1);
		if (res->lcd_power[0] < 0) {
			res->lcd_power[0] = -1;
			dsim_info("This board doesn't support LCD power GPIO");
		}
		res->lcd_power[1] = of_get_gpio(dev->of_node, 2);
		if (res->lcd_power[1] < 0) {
			res->lcd_power[1] = -1;
			dsim_info("This board doesn't support 2nd LCD power GPIO");
		}
	}

	dsim_info("%s -\n", __func__);
	return 0;
}

static int dsim_reset_panel(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	dsim_dbg("%s +\n", __func__);

#if defined(CONFIG_EXYNOS_MASS_PANEL)
	call_panel_ops(dsim, reset, dsim);
#endif

	if (res->lcd_reset <= 0)
		return 0;

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_HIGH, "lcd_reset");
	if (ret < 0) {
		dsim_err("failed to get LCD reset GPIO\n");
		return -EINVAL;
	}

	usleep_range(5000, 6000);
	gpio_set_value(res->lcd_reset, 0);
	usleep_range(5000, 6000);
	gpio_set_value(res->lcd_reset, 1);

	gpio_free(res->lcd_reset);

	usleep_range(10000, 11000);

	dsim_dbg("%s -\n", __func__);
	return 0;
}

static int dsim_set_panel_power(struct dsim_device *dsim, bool on)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	dsim_dbg("%s(%d) +\n", __func__, on);

#if defined(CONFIG_EXYNOS_MASS_PANEL)
	if (on)
		call_panel_ops(dsim, poweron, dsim);
	else
		call_panel_ops(dsim, poweroff, dsim);
#endif

	if (on) {
		if (res->lcd_power[0] > 0) {
			ret = gpio_request_one(res->lcd_power[0],
					GPIOF_OUT_INIT_HIGH, "lcd_power0");
			if (ret < 0) {
				dsim_err("failed LCD power on\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[0]);
			usleep_range(10000, 11000);
		}

		if (res->lcd_power[1] > 0) {
			ret = gpio_request_one(res->lcd_power[1],
					GPIOF_OUT_INIT_HIGH, "lcd_power1");
			if (ret < 0) {
				dsim_err("failed 2nd LCD power on\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[1]);
			usleep_range(10000, 11000);
		}
	} else {
		if (res->lcd_reset > 0) {
			ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_LOW,
					"lcd_reset");
			if (ret < 0) {
				dsim_err("failed LCD reset off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_reset);
		}

		if (res->lcd_power[0] > 0) {
			ret = gpio_request_one(res->lcd_power[0],
					GPIOF_OUT_INIT_LOW, "lcd_power0");
			if (ret < 0) {
				dsim_err("failed LCD power off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[0]);
			usleep_range(5000, 6000);
		}

		if (res->lcd_power[1] > 0) {
			ret = gpio_request_one(res->lcd_power[1],
					GPIOF_OUT_INIT_LOW, "lcd_power1");
			if (ret < 0) {
				dsim_err("failed 2nd LCD power off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[1]);
			usleep_range(5000, 6000);
		}
	}

	dsim_dbg("%s(%d) -\n", __func__, on);

	return 0;
}

static void dsim_phy_status(struct dsim_device *dsim)
{
	struct phy *phy = NULL;
	struct regmap *reg = NULL;
	u32 offset = 0;
	u32 bits = 0;
	u32 val = 0;
	int ret = 0;

	phy = dsim->phy;
	reg = dsim->res.phy_iso_reg;
	offset = dsim->res.phy_iso_offset;
	bits = dsim->res.phy_iso_bits;

	if (!reg) {
		dsim_info("%s: phy_iso_reg invalid\n", __func__);
		return;
	}

	dsim_dbg("%s: PHY count : %d\n", __func__, phy->power_count);

	ret = regmap_read(reg, offset, &val);
	if (ret < 0) {
		dsim_info("%s: Can't read 0x%x\n", __func__, offset);
		return;
	}

	if ((val & bits) != bits) {
		dsim_err("PHY Iso bypass doesn't be enabled\n");
		if (phy->power_count) {
			regmap_update_bits(reg, offset, bits, bits);
			dsim_err("PHY Iso bypass should be set again\n");
		} else {
			dsim_err("Check PHY framework reference count number\n");
		}
	}
}

static int _dsim_enable(struct dsim_device *dsim, enum dsim_state state)
{
	int ret = 0;
	bool panel_ctrl;

	if (IS_DSIM_ON_STATE(dsim)) {
		dsim_warn("%s dsim already on(%s)\n",
				__func__, dsim_state_names[dsim->state]);
		dsim->state = state;
		return 0;
	}

	dsim_dbg("%s %s +\n", __func__, dsim_state_names[dsim->state]);

#if defined(CONFIG_EXYNOS_PD)
	pm_runtime_get_sync(dsim->dev);
#else
	dsim_runtime_resume(dsim->dev);
#endif

	/* Config link to DPHY configuration */
	dpu_sysreg_set_dphy(dsim, dsim->res.ss_regs);
	/* DPHY power on : iso release */
	DPU_EVENT_LOG(DPU_EVT_DSIM_PHY_ON, &dsim->sd, ktime_set(0, 0));
	phy_power_on(dsim->phy);
	dsim_phy_status(dsim);

	/* check whether the bootloader init has been done */
	if (dsim->state == DSIM_STATE_INIT) {
		if (dsim_reg_is_pll_stable(dsim->id)) {
			dsim_info("dsim%d PLL is stabled in bootloader, so skip DSIM link/DPHY init.\n", dsim->id);
			goto init_end;
		}
	}

#if defined(CONFIG_SOC_EXYNOS9810)
	/* choose OSC_CLK */
	dsim_reg_set_link_clock(dsim->id, 0);
#endif
	/* Enable DPHY reset : DPHY reset start */
	dsim_reg_dphy_resetn(dsim->id, 1);
	panel_ctrl = (dsim->state == DSIM_STATE_OFF) ? true : false;

	/* Panel power on */
	if (panel_ctrl)
		dsim_set_panel_power(dsim, 1);

	dsim_reg_sw_reset(dsim->id);

	dsim_reg_set_clocks(dsim->id, &dsim->clks, &dsim->lcd_info.dphy_pms, 1);

	dsim_reg_set_lanes(dsim->id, dsim->data_lane, 1);
	dsim_reg_dphy_resetn(dsim->id, 0); /* Release DPHY reset */
#if defined(CONFIG_SOC_EXYNOS9810)
	dsim_reg_set_link_clock(dsim->id, 1);	/* Selection to word clock */
#endif
	dsim_reg_set_esc_clk_on_lane(dsim->id, 1, dsim->data_lane);
	dsim_reg_enable_word_clock(dsim->id, 1);

	if (dsim_reg_init(dsim->id, &dsim->lcd_info, dsim->data_lane_cnt,
				&dsim->clks) < 0) {
		dsim_info("dsim_%d already enabled", dsim->id);
		ret = -EBUSY;
	} else {
 #if !defined(CONFIG_EXYNOS_COMMON_PANEL) || defined(CONFIG_OLD_DISP_TIMING)
		dsim_info("dsim_%d enabled", dsim->id);
		/* Panel reset should be set after LP-11 */
		if (panel_ctrl)
			dsim_reset_panel(dsim);
 #endif
	}

init_end:
#if defined(CONFIG_EXYNOS_DUAL_DSI)
	dsim_reg_start_dual(dsim);
#else
	dsim_reg_start(dsim->id);
#endif
	dsim->state = state;
	enable_irq(dsim->res.irq);

	dsim_dbg("%s %s -\n", __func__, dsim_state_names[dsim->state]);

	return ret;
}

static int dsim_enable(struct dsim_device *dsim)
{
	int ret;
	enum dsim_state prev_state = dsim->state;
	enum dsim_state next_state = DSIM_STATE_ON;

	if (prev_state == next_state) {
		dsim_warn("dsim-%d %s already %s state\n", dsim->id,
				__func__, dsim_state_names[dsim->state]);
		return 0;
	}

	dsim_info("dsim-%d %s +\n", dsim->id, __func__);
	ret = _dsim_enable(dsim, next_state);
	if (ret < 0) {
		dsim_err("dsim-%d failed to set %s (ret %d)\n",
				dsim->id, dsim_state_names[next_state], ret);
		goto out;
	}
	if (prev_state != DSIM_STATE_INIT) {
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
		call_panel_ops(dsim, sleepout, dsim);
#else
		call_panel_ops(dsim, displayon, dsim);
#endif
	}
	dsim_info("dsim-%d %s - (state:%s -> %s)\n", dsim->id, __func__,
			dsim_state_names[prev_state], dsim_state_names[dsim->state]);

out:
	return ret;
}

#ifdef CONFIG_SUPPORT_DOZE
static int dsim_doze(struct dsim_device *dsim)
{
	int ret;
	enum dsim_state prev_state = dsim->state;
	enum dsim_state next_state = DSIM_STATE_DOZE;

	if (prev_state == next_state) {
		dsim_warn("dsim-%d %s already %s state\n", dsim->id,
				__func__, dsim_state_names[dsim->state]);
		return 0;
	}

	dsim_info("dsim-%d %s +\n", dsim->id, __func__);
	ret = _dsim_enable(dsim, next_state);
	if (ret < 0) {
		dsim_err("dsim-%d failed to set %s (ret %d)\n",
				dsim->id, dsim_state_names[next_state], ret);
		goto out;
	}
	if (prev_state != DSIM_STATE_INIT)
		call_panel_ops(dsim, doze, dsim);
	dsim_info("dsim-%d %s - (state:%s -> %s)\n", dsim->id, __func__,
			dsim_state_names[prev_state], dsim_state_names[dsim->state]);

out:
	return ret;
}
#endif

static int _dsim_disable(struct dsim_device *dsim, enum dsim_state state)
{
	if (IS_DSIM_OFF_STATE(dsim)) {
		dsim_warn("%s dsim already off(%s)\n",
				__func__, dsim_state_names[dsim->state]);
		if (state == DSIM_STATE_OFF)
			dsim_set_panel_power(dsim, 0);
		dsim->state = state;
		return 0;
	}

	dsim_dbg("%s %s +\n", __func__, dsim_state_names[dsim->state]);

	/* Wait for current read & write CMDs. */
	mutex_lock(&dsim->cmd_lock);
	del_timer(&dsim->cmd_timer);
	dsim->state = state;
	mutex_unlock(&dsim->cmd_lock);

	dsim_reg_stop(dsim->id, dsim->data_lane);
	disable_irq(dsim->res.irq);

	/* HACK */
	DPU_EVENT_LOG(DPU_EVT_DSIM_PHY_OFF, &dsim->sd, ktime_set(0, 0));
	phy_power_off(dsim->phy);

	if (state == DSIM_STATE_OFF)
		dsim_set_panel_power(dsim, 0);

#if defined(CONFIG_EXYNOS_PD)
	pm_runtime_put_sync(dsim->dev);
#else
	dsim_runtime_suspend(dsim->dev);
#endif

	dsim_dbg("%s %s -\n", __func__, dsim_state_names[dsim->state]);

	return 0;
}

static int dsim_disable(struct dsim_device *dsim)
{
	int ret;
	enum dsim_state prev_state = dsim->state;
	enum dsim_state next_state = DSIM_STATE_OFF;

	if (prev_state == next_state) {
		dsim_warn("dsim-%d %s already %s state\n", dsim->id,
				__func__, dsim_state_names[dsim->state]);
		return 0;
	}

	dsim_info("dsim-%d %s +\n", dsim->id, __func__);
	if (dsim->lcd_info.mode != DECON_VIDEO_MODE)
		call_panel_ops(dsim, suspend, dsim);
	ret = _dsim_disable(dsim, next_state);
	if (ret < 0) {
		dsim_err("dsim-%d failed to set %s (ret %d)\n",
				dsim->id, dsim_state_names[next_state], ret);
		goto out;
	}
	dsim_info("dsim-%d %s - (state:%s -> %s)\n", dsim->id, __func__,
			dsim_state_names[prev_state], dsim_state_names[dsim->state]);

out:
	return ret;
}

#ifdef CONFIG_SUPPORT_DOZE
static int dsim_doze_suspend(struct dsim_device *dsim)
{
	int ret;
	enum dsim_state prev_state = dsim->state;
	enum dsim_state next_state = DSIM_STATE_DOZE_SUSPEND;

	if (prev_state == next_state) {
		dsim_warn("dsim-%d %s already %s state\n", dsim->id,
				__func__, dsim_state_names[dsim->state]);
		return 0;
	}

	dsim_info("dsim-%d %s +\n", dsim->id, __func__);
	call_panel_ops(dsim, doze_suspend, dsim);
	ret = _dsim_disable(dsim, next_state);
	if (ret < 0) {
		dsim_err("dsim-%d failed to set %s (ret %d)\n",
				dsim->id, dsim_state_names[next_state], ret);
		goto out;
	}
	dsim_info("dsim-%d %s - (state:%s -> %s)\n", dsim->id, __func__,
			dsim_state_names[prev_state], dsim_state_names[dsim->state]);

out:
	return ret;
}
#endif

static int dsim_enter_ulps(struct dsim_device *dsim)
{
	int ret = 0;

	DPU_EVENT_START();
	dsim_dbg("%s +\n", __func__);
	exynos_ss_printk("%s:state %d: active %d:+\n", __func__,
				dsim->state, pm_runtime_active(dsim->dev));

	if (!IS_DSIM_ON_STATE(dsim)) {
		ret = -EBUSY;
		goto err;
	}

	/* Wait for current read & write CMDs. */
	mutex_lock(&dsim->cmd_lock);
	dsim->state = DSIM_STATE_ULPS;
	mutex_unlock(&dsim->cmd_lock);

	/* disable interrupts */
	dsim_reg_set_int(dsim->id, 0);

	disable_irq(dsim->res.irq);
	ret = dsim_reg_stop_and_enter_ulps(dsim->id, dsim->lcd_info.ddi_type,
			dsim->data_lane);
	if (ret < 0)
		dsim_dump(dsim);

	DPU_EVENT_LOG(DPU_EVT_DSIM_PHY_OFF, &dsim->sd, ktime_set(0, 0));
	phy_power_off(dsim->phy);

#if defined(CONFIG_EXYNOS_PD)
	pm_runtime_put_sync(dsim->dev);
#else
	dsim_runtime_suspend(dsim->dev);
#endif

	DPU_EVENT_LOG(DPU_EVT_ENTER_ULPS, &dsim->sd, start);
err:
	dsim_dbg("%s -\n", __func__);
	exynos_ss_printk("%s:state %d: active %d:-\n", __func__,
				dsim->state, pm_runtime_active(dsim->dev));

	return ret;
}

static int dsim_exit_ulps(struct dsim_device *dsim)
{
	int ret = 0;

	DPU_EVENT_START();
	dsim_dbg("%s +\n", __func__);
	exynos_ss_printk("%s:state %d: active %d:+\n", __func__,
				dsim->state, pm_runtime_active(dsim->dev));

	if (dsim->state != DSIM_STATE_ULPS) {
		ret = -EBUSY;
		goto err;
	}

#if defined(CONFIG_EXYNOS_PD)
	pm_runtime_get_sync(dsim->dev);
#else
	dsim_runtime_resume(dsim->dev);
#endif
	dpu_sysreg_set_dphy(dsim, dsim->res.ss_regs);
	/* DPHY power on : iso release */
	DPU_EVENT_LOG(DPU_EVT_DSIM_PHY_ON, &dsim->sd, ktime_set(0, 0));
	phy_power_on(dsim->phy);
	dsim_phy_status(dsim);

	enable_irq(dsim->res.irq);

#if defined(CONFIG_SOC_EXYNOS9810)
	/* choose OSC_CLK */
	dsim_reg_set_link_clock(dsim->id, 0);
#endif
	/* Enable DPHY reset : DPHY reset start */
	dsim_reg_dphy_resetn(dsim->id, 1);
	/* DSIM Link SW reset */
	dsim_reg_sw_reset(dsim->id);

	dsim_reg_set_clocks(dsim->id, &dsim->clks,
			&dsim->lcd_info.dphy_pms, 1);

	dsim_reg_set_lanes(dsim->id, dsim->data_lane, 1);
	dsim_reg_dphy_resetn(dsim->id, 0); /* release DPHY reset */
#if defined(CONFIG_SOC_EXYNOS9810)
	dsim_reg_set_link_clock(dsim->id, 1);	/* Selection to word clock */
#endif

	dsim_reg_set_esc_clk_on_lane(dsim->id, 1, dsim->data_lane);
	dsim_reg_enable_word_clock(dsim->id, 1);

	if (dsim_reg_init(dsim->id, &dsim->lcd_info, dsim->data_lane_cnt,
				&dsim->clks) < 0 ) {
		dsim_info("dsim_%d already enabled", dsim->id);
		return -EBUSY;
	}
	ret = dsim_reg_exit_ulps_and_start(dsim->id, dsim->lcd_info.ddi_type,
			dsim->data_lane);
	if (ret < 0)
		dsim_dump(dsim);

	dsim->state = DSIM_STATE_ON;

	DPU_EVENT_LOG(DPU_EVT_EXIT_ULPS, &dsim->sd, start);
err:
	dsim_dbg("%s -\n", __func__);
	exynos_ss_printk("%s:state %d: active %d:-\n", __func__,
				dsim->state, pm_runtime_active(dsim->dev));

	return 0;
}

static int dsim_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct dsim_device *dsim = container_of(sd, struct dsim_device, sd);

	if (enable)
		return dsim_enable(dsim);
	else
		return dsim_disable(dsim);
}


#ifdef CONFIG_SUPPORT_DSU
static int set_dsim_dsu(struct dsim_device *dsim, struct dsu_info *dsu)
{
	int ret = 0;

	dsim_info("DSIM:INFO:%s:mode:%d, res:%d,%d,%d,%d\n",
		__func__, dsu->mode, dsu->left, dsu->top, dsu->right, dsu->bottom);

	call_panel_ops(dsim, dsu, dsim, dsu);

	dsim_reg_set_dsu(dsim->id, &dsim->lcd_info);

	return ret;
}
#endif

static int dsim_free_fb_resource(struct dsim_device *dsim)
{
	/* unmap */
	iovmm_unmap_oto(dsim->dev, dsim->fb_handover.phys_addr);

	/* unreserve memory */
	of_reserved_mem_device_release(dsim->dev);

	/* update state */
	dsim->fb_handover.reserved = false;
	dsim->fb_handover.phys_addr = 0xdead;
	dsim->fb_handover.phys_size = 0;

	return 0;
}

static int dsim_acquire_fb_resource(struct dsim_device *dsim)
{
	int ret = 0;

	/*
	 * If of_reserved_mem_device_init_by_idx returns error, it means
	 * framebuffer handover feature is disabled or reserved memory is
	 * not defined in DT.
	 *
	 * And phys_addr and phys_size is not initialized, becuase
	 * rmem_device_init callback is not called.
	 */
	ret = of_reserved_mem_device_init_by_idx(dsim->dev, dsim->dev->of_node, 0);
	if (ret) {
		dsim_warn("fb handover memory is not reserved(%d)\n", ret);
		dsim_warn("check whether fb handover memory is not reserved");
		dsim_warn("or DT definition is missed\n");
		dsim->fb_handover.reserved = false;
		return 0;
	} else {
		dsim->fb_handover.reserved = true;
	}

	/* phys_addr and phys_size must be aligned to page size */
	ret = iovmm_map_oto(dsim->dev, dsim->fb_handover.phys_addr,
			dsim->fb_handover.phys_size);
	if (ret) {
		dsim_err("failed one to one mapping: %d\n", ret);
		BUG();
	}

	return ret;
}

static long dsim_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct dsim_device *dsim = container_of(sd, struct dsim_device, sd);

	int ret = 0;

	switch (cmd) {
	case DSIM_IOC_GET_LCD_INFO:
		v4l2_set_subdev_hostdata(sd, &dsim->lcd_info);
		break;

	case DSIM_IOC_ENTER_ULPS:
		if ((unsigned long)arg)
			ret = dsim_enter_ulps(dsim);
		else
			ret = dsim_exit_ulps(dsim);
		break;

	case DSIM_IOC_DUMP:
		dsim_dump(dsim);
		break;

	case DSIM_IOC_GET_WCLK:
		v4l2_set_subdev_hostdata(sd, &dsim->clks.word_clk);
		break;

	case EXYNOS_DPU_GET_ACLK:
		return clk_get_rate(dsim->res.aclk);

#ifdef CONFIG_SUPPORT_DOZE
	case DSIM_IOC_DOZE:
		ret = dsim_doze(dsim);
		break;

	case DSIM_IOC_DOZE_SUSPEND:
		ret = dsim_doze_suspend(dsim);
		break;
#endif

#ifdef CONFIG_SUPPORT_DSU
	case DSIM_IOC_DSU:
		ret = set_dsim_dsu(dsim, (struct dsu_info *)arg);
		break;
#endif

#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	case DSIM_IOC_NOTIFY:
		call_panel_ops(dsim, notify, dsim, arg);
		break;

	case DSIM_IOC_SET_ERROR_CB:
		if (arg == NULL) {
			dsim_err("%s invalid arg\n", __func__);
			ret = -EINVAL;
			break;
		}
		dsim->error_cb_info.error_cb = ((struct disp_error_cb_info *)arg)->error_cb;
		dsim->error_cb_info.data = ((struct disp_error_cb_info *)arg)->data;
		call_panel_ops(dsim, set_error_cb, dsim);
		break;
#endif

	case DSIM_IOC_FREE_FB_RES:
		ret = dsim_free_fb_resource(dsim);
		break;

	default:
		dsim_err("unsupported ioctl");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_core_ops dsim_sd_core_ops = {
	.ioctl = dsim_ioctl,
};

static const struct v4l2_subdev_video_ops dsim_sd_video_ops = {
	.s_stream = dsim_s_stream,
};

static const struct v4l2_subdev_ops dsim_subdev_ops = {
	.core = &dsim_sd_core_ops,
	.video = &dsim_sd_video_ops,
};

static void dsim_init_subdev(struct dsim_device *dsim)
{
	struct v4l2_subdev *sd = &dsim->sd;

	v4l2_subdev_init(sd, &dsim_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = dsim->id;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "dsim-sd", dsim->id);
	v4l2_set_subdevdata(sd, dsim);
}

static int dsim_cmd_sysfs_write(struct dsim_device *dsim, bool on)
{
	int ret = 0;

	if (on)
		ret = dsim_write_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_SET_DISPLAY_ON, 0);
	else
		ret = dsim_write_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_SET_DISPLAY_OFF, 0);
	if (ret < 0)
		dsim_err("Failed to write test data!\n");
	else
		dsim_dbg("Succeeded to write test data!\n");

	return ret;
}

static int dsim_cmd_sysfs_read(struct dsim_device *dsim)
{
	int ret = 0;
	unsigned int id;
	u8 buf[4];

	/* dsim sends the request for the lcd id and gets it buffer */
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,
		MIPI_DCS_GET_DISPLAY_ID, DSIM_DDI_ID_LEN, buf);
	id = *(unsigned int *)buf;
	if (ret < 0)
		dsim_err("Failed to read panel id!\n");
	else
		dsim_info("Suceeded to read panel id : 0x%08x\n", id);

	return ret;
}

static ssize_t dsim_cmd_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t dsim_cmd_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long cmd;
	struct dsim_device *dsim = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &cmd);
	if (ret)
		return ret;

	switch (cmd) {
	case 1:
		ret = dsim_cmd_sysfs_read(dsim);
		call_panel_ops(dsim, dump, dsim);
		if (ret)
			return ret;
		break;
	case 2:
		ret = dsim_cmd_sysfs_write(dsim, true);
		dsim_info("Dsim write command, display on!!\n");
		if (ret)
			return ret;
		break;
	case 3:
		ret = dsim_cmd_sysfs_write(dsim, false);
		dsim_info("Dsim write command, display off!!\n");
		if (ret)
			return ret;
		break;
	default :
		dsim_info("unsupportable command\n");
		break;
	}

	return count;
}
static DEVICE_ATTR(cmd_rw, 0644, dsim_cmd_sysfs_show, dsim_cmd_sysfs_store);

int dsim_create_cmd_rw_sysfs(struct dsim_device *dsim)
{
	int ret = 0;

	ret = device_create_file(dsim->dev, &dev_attr_cmd_rw);
	if (ret)
		dsim_err("failed to create command read & write sysfs\n");

	return ret;
}

void parse_lcd_info(struct device_node *node, struct decon_lcd *lcd_info)
{
	u32 res[14];
	unsigned int mres_num = 1;
	u32 mres_w[3] = {0, };
	u32 mres_h[3] = {0, };
	u32 mres_dsc_w[3] = {0, };
	u32 mres_dsc_h[3] = {0, };
	u32 mres_dsc_en[3] = {0, };
	u32 hdr_num = 0;
	u32 hdr_type[HDR_CAPA_NUM] = {0, };
	u32 hdr_mxl = 0;
	u32 hdr_mal = 0;
	u32 hdr_mnl = 0;
	u32 vt_compensation;
	int k;

	of_property_read_u32(node, "mode", &lcd_info->mode);
	dsim_info("%s mode\n", lcd_info->mode ? "command" : "video");

	of_property_read_u32_array(node, "resolution", res, 2);
	lcd_info->xres = res[0];
	lcd_info->yres = res[1];
	dsim_info("LCD(%s) resolution: xres(%d), yres(%d)\n",
			of_node_full_name(node), res[0], res[1]);

	of_property_read_u32_array(node, "size", res, 2);
	lcd_info->width = res[0];
	lcd_info->height = res[1];
	dsim_dbg("LCD size: width(%d), height(%d)\n", res[0], res[1]);

	of_property_read_u32(node, "timing,refresh", &lcd_info->fps);
	dsim_dbg("LCD refresh rate(%d)\n", lcd_info->fps);

	of_property_read_u32_array(node, "timing,h-porch", res, 3);
	lcd_info->hbp = res[0];
	lcd_info->hfp = res[1];
	lcd_info->hsa = res[2];
	dsim_dbg("hbp(%d), hfp(%d), hsa(%d)\n", res[0], res[1], res[2]);

	of_property_read_u32_array(node, "timing,v-porch", res, 3);
	lcd_info->vbp = res[0];
	lcd_info->vfp = res[1];
	lcd_info->vsa = res[2];
	dsim_dbg("vbp(%d), vfp(%d), vsa(%d)\n", res[0], res[1], res[2]);

	of_property_read_u32(node, "timing,dsi-hs-clk", &lcd_info->hs_clk);
	dsim_dbg("requested hs clock(%d)\n", lcd_info->hs_clk);

#if defined(CONFIG_EXYNOS_DSIM_DITHER)
	of_property_read_u32_array(node, "timing,pmsk", res, 14);
#else
	of_property_read_u32_array(node, "timing,pmsk", res, 4);
#endif
	lcd_info->dphy_pms.p = res[0];
	lcd_info->dphy_pms.m = res[1];
	lcd_info->dphy_pms.s = res[2];
	lcd_info->dphy_pms.k = res[3];
	dsim_dbg("p(%d), m(%d), s(%d), k(%d)\n", res[0], res[1], res[2], res[3]);
#if defined(CONFIG_EXYNOS_DSIM_DITHER)
	lcd_info->dphy_pms.mfr = res[4];
	lcd_info->dphy_pms.mrr = res[5];
	lcd_info->dphy_pms.sel_pf = res[6];
	lcd_info->dphy_pms.icp = res[7];
	lcd_info->dphy_pms.afc_enb = res[8];
	lcd_info->dphy_pms.extafc = res[9];
	lcd_info->dphy_pms.feed_en = res[10];
	lcd_info->dphy_pms.fsel = res[11];
	lcd_info->dphy_pms.fout_mask = res[12];
	lcd_info->dphy_pms.rsel = res[13];
	dsim_dbg(" mfr(%d), mrr(0x%x), sel_pf(%d), icp(%d)\n",
			res[4], res[5], res[6], res[7]);
	dsim_dbg(" afc_enb(%d), extafc(%d), feed_en(%d), fsel(%d)\n",
			res[8], res[9], res[10], res[11]);
	dsim_dbg(" fout_mask(%d), rsel(%d)\n", res[12], res[13]);
#endif
	of_property_read_u32(node, "timing,dsi-escape-clk",
			&lcd_info->esc_clk);
	dsim_dbg("requested escape clock(%d)\n", lcd_info->esc_clk);

	of_property_read_u32(node, "mic_en", &lcd_info->mic_enabled);
	dsim_info("mic enabled (%d)\n", lcd_info->mic_enabled);

	of_property_read_u32(node, "type_of_ddi", &lcd_info->ddi_type);
	dsim_dbg("ddi type(%d)\n", lcd_info->ddi_type);

	of_property_read_u32(node, "dsc_en", &lcd_info->dsc_enabled);
	dsim_info("dsc is %s\n", lcd_info->dsc_enabled ? "enabled" : "disabled");

	if (lcd_info->dsc_enabled) {
		of_property_read_u32(node, "dsc_cnt", &lcd_info->dsc_cnt);
		dsim_info("dsc count(%d)\n", lcd_info->dsc_cnt);
		of_property_read_u32(node, "dsc_slice_num",
				&lcd_info->dsc_slice_num);
		dsim_info("dsc slice count(%d)\n", lcd_info->dsc_slice_num);
		of_property_read_u32(node, "dsc_slice_h",
				&lcd_info->dsc_slice_h);
		dsim_info("dsc slice height(%d)\n", lcd_info->dsc_slice_h);
	}

	of_property_read_u32(node, "data_lane", &lcd_info->data_lane);
	dsim_info("using data lane count(%d)\n", lcd_info->data_lane);

	of_property_read_u32(node, "mres_en", &lcd_info->dt_lcd_mres.mres_en);
	dsim_info("mres_en(%d)\n", lcd_info->dt_lcd_mres.mres_en);
	lcd_info->mres_mode = 1; /* 1=WQHD, 2=FHD, 3=HD */
	lcd_info->dt_lcd_mres.mres_number = mres_num; /* default = 1 */

	if (lcd_info->dt_lcd_mres.mres_en) {
		of_property_read_u32(node, "mres_number", &mres_num);
		lcd_info->dt_lcd_mres.mres_number = mres_num;
		dsim_info("mres_number(%d)\n", mres_num);

		of_property_read_u32_array(node, "mres_width", mres_w, mres_num);
		of_property_read_u32_array(node, "mres_height", mres_h, mres_num);
		of_property_read_u32_array(node, "mres_dsc_width", mres_dsc_w, mres_num);
		of_property_read_u32_array(node, "mres_dsc_height", mres_dsc_h, mres_num);
		of_property_read_u32_array(node, "mres_dsc_en", mres_dsc_en, mres_num);

		switch (mres_num) {
		case 3:
			lcd_info->dt_lcd_mres.res_info[2].width = mres_w[2];
			lcd_info->dt_lcd_mres.res_info[2].height = mres_h[2];
			lcd_info->dt_lcd_mres.res_info[2].dsc_en = mres_dsc_en[2];
			lcd_info->dt_lcd_mres.res_info[2].dsc_width = mres_dsc_w[2];
			lcd_info->dt_lcd_mres.res_info[2].dsc_height = mres_dsc_h[2];
		case 2:
			lcd_info->dt_lcd_mres.res_info[1].width = mres_w[1];
			lcd_info->dt_lcd_mres.res_info[1].height = mres_h[1];
			lcd_info->dt_lcd_mres.res_info[1].dsc_en = mres_dsc_en[1];
			lcd_info->dt_lcd_mres.res_info[1].dsc_width = mres_dsc_w[1];
			lcd_info->dt_lcd_mres.res_info[1].dsc_height = mres_dsc_h[1];
		case 1:
			lcd_info->dt_lcd_mres.res_info[0].width = mres_w[0];
			lcd_info->dt_lcd_mres.res_info[0].height = mres_h[0];
			lcd_info->dt_lcd_mres.res_info[0].dsc_en = mres_dsc_en[0];
			lcd_info->dt_lcd_mres.res_info[0].dsc_width = mres_dsc_w[0];
			lcd_info->dt_lcd_mres.res_info[0].dsc_height = mres_dsc_h[0];
			break;
		default:
			lcd_info->dt_lcd_mres.res_info[0].width = lcd_info->width;
			lcd_info->dt_lcd_mres.res_info[0].height = lcd_info->height;
			dsim_warn("check multi-resolution configurations at DT\n");
			break;
		}
		dsim_info("[LCD multi(%d)-resolution info] 1st(%dx%d), 2nd(%dx%d), 3rd(%dx%d)\n",
				mres_num, mres_w[0], mres_h[0],
				mres_w[1], mres_h[1], mres_w[2], mres_h[2]);
	} else {
		lcd_info->dt_lcd_mres.res_info[0].width = lcd_info->width;
		lcd_info->dt_lcd_mres.res_info[0].height = lcd_info->height;
	}

	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE) {
		of_property_read_u32_array(node, "cmd_underrun_lp_ref",
				lcd_info->cmd_underrun_lp_ref,
				lcd_info->dt_lcd_mres.mres_number);
		for (k = 0; k < lcd_info->dt_lcd_mres.mres_number; k++)
			dsim_info("mres[%d] cmd_underrun_lp_ref(%d)\n", k,
					lcd_info->cmd_underrun_lp_ref[k]);
	} else {
		of_property_read_u32(node, "vt_compensation", &vt_compensation);
		lcd_info->vt_compensation = vt_compensation;
		dsim_info("vt_compensation(%d)\n", lcd_info->vt_compensation);
	}

	/* HDR info */
	of_property_read_u32(node, "hdr_num", &hdr_num);
	lcd_info->dt_lcd_hdr.hdr_num = hdr_num;
	dsim_info("hdr_num(%d)\n", hdr_num);

	if (hdr_num != 0) {
		of_property_read_u32_array(node, "hdr_type", hdr_type, hdr_num);
		for (k = 0; k < hdr_num; k++) {
			lcd_info->dt_lcd_hdr.hdr_type[k] = hdr_type[k];
			dsim_info("hdr_type[%d] = %d\n", k, hdr_type[k]);
		}

		of_property_read_u32(node, "hdr_max_luma", &hdr_mxl);
		of_property_read_u32(node, "hdr_max_avg_luma", &hdr_mal);
		of_property_read_u32(node, "hdr_min_luma", &hdr_mnl);
		lcd_info->dt_lcd_hdr.hdr_max_luma = hdr_mxl;
		lcd_info->dt_lcd_hdr.hdr_max_avg_luma = hdr_mal;
		lcd_info->dt_lcd_hdr.hdr_min_luma = hdr_mnl;
		dsim_info("hdr_max_luma(%d), hdr_max_avg_luma(%d), hdr_min_luma(%d)\n",
				hdr_mxl, hdr_mal, hdr_mnl);
	}
}

#if !defined(CONFIG_EXYNOS_COMMON_PANEL)
static void dsim_parse_lcd_info(struct dsim_device *dsim)
{
	struct device_node *node;

	node = of_parse_phandle(dsim->dev->of_node, "lcd_info", 0);

	parse_lcd_info(node, &dsim->lcd_info);

	dsim->clks.hs_clk = dsim->lcd_info.hs_clk;
	dsim->clks.esc_clk = dsim->lcd_info.esc_clk;
	dsim->data_lane_cnt = dsim->lcd_info.data_lane;
}
#endif

static int dsim_parse_dt(struct dsim_device *dsim, struct device *dev)
{
	if (IS_ERR_OR_NULL(dev->of_node)) {
		dsim_err("no device tree information\n");
		return -EINVAL;
	}

	dsim->id = of_alias_get_id(dev->of_node, "dsim");
	dsim_info("dsim(%d)(%s) probe start..\n", dsim->id, of_node_full_name(dev->of_node));

	dsim->phy = devm_phy_get(dev, "dsim_dphy");
	if (IS_ERR_OR_NULL(dsim->phy)) {
		dsim_err("failed to get phy\n");
		return PTR_ERR(dsim->phy);
	}

	dsim->dev = dev;

#if !defined(CONFIG_EXYNOS_COMMON_PANEL)
	dsim_get_gpios(dsim);

	dsim_parse_lcd_info(dsim);
#endif

	return 0;
}

static void dsim_register_panel(struct dsim_device *dsim)
{
#if IS_ENABLED(CONFIG_EXYNOS_COMMON_PANEL)
	dsim->panel_ops = &common_mipi_lcd_driver;
#elif defined(CONFIG_EXYNOS_MASS_PANEL)
	dsim->panel_ops = mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3HA2K)
	dsim->panel_ops = &s6e3ha2k_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3HF4)
	dsim->panel_ops = &s6e3hf4_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3HA6)
	dsim->panel_ops = &s6e3ha6_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3HA8)
	dsim->panel_ops = &s6e3ha8_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_EMUL_DISP)
	dsim->panel_ops = &emul_disp_mipi_lcd_driver;
#else
	dsim->panel_ops = &s6e3ha2k_mipi_lcd_driver;
#endif
}

static int dsim_get_data_lanes(struct dsim_device *dsim)
{
	int i;

	if (dsim->data_lane_cnt > MAX_DSIM_DATALANE_CNT) {
		dsim_err("%d data lane couldn't be supported\n",
				dsim->data_lane_cnt);
		return -EINVAL;
	}

	dsim->data_lane = DSIM_LANE_CLOCK;
	for (i = 1; i < dsim->data_lane_cnt + 1; ++i)
		dsim->data_lane |= 1 << i;

	dsim_info("%s: lanes(0x%x)\n", __func__, dsim->data_lane);

	return 0;
}

static int dsim_init_resources(struct dsim_device *dsim, struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dsim_err("failed to get mem resource\n");
		return -ENOENT;
	}
	dsim_info("res: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);

	dsim->res.regs = devm_ioremap_resource(dsim->dev, res);
	if (!dsim->res.regs) {
		dsim_err("failed to remap DSIM SFR region\n");
		return -EINVAL;
	}

#if defined(CONFIG_SOC_EXYNOS9810)
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dsim_err("failed to get mem resource\n");
		return -ENOENT;
	}
	dsim_info("dphy res: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);

	dsim->res.phy_regs = devm_ioremap_resource(dsim->dev, res);
	if (!dsim->res.phy_regs) {
		dsim_err("failed to remap DSIM DPHY SFR region\n");
		return -EINVAL;
	}
#endif

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dsim_err("failed to get irq resource\n");
		return -ENOENT;
	}

	dsim->res.irq = res->start;
	ret = devm_request_irq(dsim->dev, res->start,
			dsim_irq_handler, 0, pdev->name, dsim);
	if (ret) {
		dsim_err("failed to install DSIM irq\n");
		return -EINVAL;
	}

	disable_irq(dsim->res.irq);

	dsim->res.ss_regs = dpu_get_sysreg_addr();
	if (IS_ERR_OR_NULL(dsim->res.ss_regs)) {
		dsim_err("failed to get sysreg addr\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(dsim->phy->dev.of_node, "sysreg_bits", &dsim->res.ss_regs_bits);
	if (ret < 0) {
		dsim_err("sysreg_bits of_property_read_u32 fail\n");
		return -EINVAL;
	}

	dsim->res.phy_iso_reg = syscon_regmap_lookup_by_phandle(dsim->phy->dev.of_node, "samsung,pmu-syscon");
	if (IS_ERR_OR_NULL(dsim->res.phy_iso_reg)) {
		dsim_err("samsung,pmu-syscon syscon_regmap_lookup_by_phandle fail\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(dsim->phy->dev.of_node, "isolation", &dsim->res.phy_iso_offset);
	if (ret < 0) {
		dsim_err("isolation of_property_read_u32 fail\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(dsim->phy->dev.of_node, "isolation_bits", &dsim->res.phy_iso_bits);
	if (ret < 0) {
		dsim_err("isolation_bits of_property_read_u32 fail\n");
		return -EINVAL;
	}

	return 0;
}

static int dsim_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct dsim_device *dsim = NULL;

	dsim = devm_kzalloc(dev, sizeof(struct dsim_device), GFP_KERNEL);
	if (!dsim) {
		dsim_err("failed to allocate dsim device.\n");
		ret = -ENOMEM;
		goto err;
	}

	ret = dsim_parse_dt(dsim, dev);
	if (ret)
		goto err_dt;

	dsim_drvdata[dsim->id] = dsim;
	ret = dsim_get_clocks(dsim);
	if (ret)
		goto err_dt;

	spin_lock_init(&dsim->slock);
	mutex_init(&dsim->cmd_lock);
	init_completion(&dsim->ph_wr_comp);
	init_completion(&dsim->rd_comp);

	ret = dsim_init_resources(dsim, pdev);
	if (ret)
		goto err_dt;

	dsim_init_subdev(dsim);
	platform_set_drvdata(pdev, dsim);
	dsim_register_panel(dsim);
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	call_panel_ops(dsim, init, dsim);
#endif
	setup_timer(&dsim->cmd_timer, dsim_cmd_fail_detector,
			(unsigned long)dsim);

	pm_runtime_enable(dev);
	if (dsim->lcd_info.mode == DECON_VIDEO_MODE)
		dsim_acquire_fb_resource(dsim);

#if defined(CONFIG_ION_EXYNOS)
	ret = iovmm_activate(dev);
	if (ret) {
		dsim_err("failed to activate iovmm\n");
		goto err_dt;
	}
	iovmm_set_fault_handler(dev, dpu_sysmmu_fault_handler, NULL);
#endif

	ret = dsim_get_data_lanes(dsim);
	if (ret)
		goto err_dt;

	/* HACK */
	phy_init(dsim->phy);
	dsim->state = DSIM_STATE_INIT;
	dsim_enable(dsim);

	/* TODO: If you want to enable DSIM BIST mode. you must turn on LCD here */

#if !defined(BRINGUP_DSIM_BIST)
	call_panel_ops(dsim, probe, dsim);
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	call_panel_ops(dsim, sleepout, dsim);
#endif
#else
	/* TODO: This is for dsim BIST mode in zebu emulator. only for test*/
	call_panel_ops(dsim, displayon, dsim);
	dsim_set_bist(dsim->id, true);
#endif
	//dsim_set_bist(dsim->id, true);

	/* for debug */
	/* dsim_dump(dsim); */

	dsim_clocks_info(dsim);
	dsim_create_cmd_rw_sysfs(dsim);

	dsim_info("dsim%d driver(%s mode) has been probed.\n", dsim->id,
		dsim->lcd_info.mode == DECON_MIPI_COMMAND_MODE ? "cmd" : "video");

	return 0;

err_dt:
	kfree(dsim);
err:
	return ret;
}

static int dsim_remove(struct platform_device *pdev)
{
	struct dsim_device *dsim = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	mutex_destroy(&dsim->cmd_lock);
	dsim_info("dsim%d driver removed\n", dsim->id);

	return 0;
}

static void dsim_shutdown(struct platform_device *pdev)
{
	struct dsim_device *dsim = platform_get_drvdata(pdev);

	dsim_info("%s + dsim%d state:%d\n", __func__, dsim->id, dsim->state);
	DPU_EVENT_LOG(DPU_EVT_DSIM_SHUTDOWN, &dsim->sd, ktime_set(0, 0));

	/* dsim_disable(dsim); */

	dsim_info("%s -\n", __func__);
}

static int dsim_runtime_suspend(struct device *dev)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);

	DPU_EVENT_LOG(DPU_EVT_DSIM_SUSPEND, &dsim->sd, ktime_set(0, 0));
	dsim_dbg("%s +\n", __func__);
	clk_disable_unprepare(dsim->res.aclk);
	dsim_dbg("%s -\n", __func__);
	return 0;
}

static int dsim_runtime_resume(struct device *dev)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);

	DPU_EVENT_LOG(DPU_EVT_DSIM_RESUME, &dsim->sd, ktime_set(0, 0));
	dsim_dbg("%s: +\n", __func__);
	clk_prepare_enable(dsim->res.aclk);
	dsim_dbg("%s -\n", __func__);
	return 0;
}

static const struct of_device_id dsim_of_match[] = {
#if defined(CONFIG_SOC_EXYNOS9810)
	{ .compatible = "samsung,exynos9-dsim" },
#else
	{ .compatible = "samsung,exynos8-dsim" },
#endif
	{},
};
MODULE_DEVICE_TABLE(of, dsim_of_match);

static const struct dev_pm_ops dsim_pm_ops = {
	.runtime_suspend	= dsim_runtime_suspend,
	.runtime_resume		= dsim_runtime_resume,
};

static struct platform_driver dsim_driver __refdata = {
	.probe			= dsim_probe,
	.remove			= dsim_remove,
	.shutdown		= dsim_shutdown,
	.driver = {
		.name		= DSIM_MODULE_NAME,
		.owner		= THIS_MODULE,
		.pm		= &dsim_pm_ops,
		.of_match_table	= of_match_ptr(dsim_of_match),
		.suppress_bind_attrs = true,
	}
};

static int __init dsim_init(void)
{
	int ret = platform_driver_register(&dsim_driver);
	if (ret)
		pr_err("dsim driver register failed\n");

	return ret;
}
late_initcall(dsim_init);

static void __exit dsim_exit(void)
{
	platform_driver_unregister(&dsim_driver);
}

module_exit(dsim_exit);

/*
 * rmem_device_init is called in of_reserved_mem_device_init_by_idx function
 * when reserved memory is required.
 */
static int rmem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);

	dsim_info("%s +\n", __func__);
	dsim->fb_handover.phys_addr = rmem->base;
	dsim->fb_handover.phys_size = rmem->size;
	dsim_info("%s -\n", __func__);

	return 0;
}

/*
 * rmem_device_release is called in of_reserved_mem_device_release function
 * when reserved memory is no longer required.
 */
static void rmem_device_release(struct reserved_mem *rmem, struct device *dev)
{
	struct page *first = phys_to_page(PAGE_ALIGN(rmem->base));
	struct page *last = phys_to_page((rmem->base + rmem->size) & PAGE_MASK);
	struct page *page;

	pr_info("%s: base=%pa, size=%pa, first=%pa, last=%pa\n",
			__func__, &rmem->base, &rmem->size, first, last);

	for (page = first; page != last; page++) {
		__ClearPageReserved(page);
		set_page_count(page, 1);
		__free_pages(page, 0);
		adjust_managed_page_count(page, 1);
	}
}

static const struct reserved_mem_ops rmem_ops = {
	.device_init    = rmem_device_init,
	.device_release = rmem_device_release,
};

static int __init fb_handover_setup(struct reserved_mem *rmem)
{
	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);

	rmem->ops = &rmem_ops;
	return 0;
}
RESERVEDMEM_OF_DECLARE(fb_handover, "exynos,fb_handover", fb_handover_setup);

MODULE_AUTHOR("Yeongran Shin <yr613.shin@samsung.com>");
MODULE_DESCRIPTION("Samusung EXYNOS DSIM driver");
MODULE_LICENSE("GPL");
