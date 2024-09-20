/*
 * Copyright (c) 2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_ion.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/exynos_iovmm.h>
#include <linux/ion.h>
#include <ion_priv.h>
#include <linux/scatterlist.h>
#include "../../../video/fbdev/exynos/dpu_9810/decon.h"
#include <t-base-tui.h>
#include "dciTui.h"
#include "tlcTui.h"
#include "tui-hal.h"
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
#if defined(CONFIG_TOUCHSCREEN_SEC_TS) || defined(CONFIG_TOUCHSCREEN_SEC_TS_Y771)
#include "../../../../../input/touchscreen/sec_ts/sec_ts.h"
#endif
#if defined(CONFIG_TOUCHSCREEN_MELFAS_MSS100)
#include "../../../../../input/touchscreen/melfas/mss100/melfas_mss100.h"
#endif
#endif

/* I2C register for reset */
#define HSI2C7_PA_BASE_ADDRESS	0x14E10000
#define HSI2C_CTL		0x00
#define HSI2C_TRAILIG_CTL	0x08
#define HSI2C_FIFO_STAT		0x30
#define HSI2C_CONF		0x40
#define HSI2C_TRANS_STATUS	0x50

#define HSI2C_SW_RST		(1u << 31)
#define HSI2C_FUNC_MODE_I2C	(1u << 0)
#define HSI2C_MASTER		(1u << 3)
#define HSI2C_TRAILING_COUNT	(0xf)
#define HSI2C_AUTO_MODE		(1u << 31)
#define HSI2C_RX_FIFO_EMPTY	(1u << 24)
#define HSI2C_TX_FIFO_EMPTY	(1u << 8)
#define HSI2C_FIFO_EMPTY	(HSI2C_RX_FIFO_EMPTY | HSI2C_TX_FIFO_EMPTY)
#define TUI_MEMPOOL_SIZE	0

//extern struct switch_dev tui_switch;
bool tui_cover_mode_on;
extern phys_addr_t hal_tui_video_space_alloc(void);
extern int decon_lpd_block_exit(struct decon_device *decon);

static int tsp_irq_num = 718;	// default value

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
static void tui_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

void trustedui_set_tsp_irq(int irq_num)
{
	tsp_irq_num = irq_num;
	pr_info("%s called![%d]\n", __func__, irq_num);
}
#endif

struct tui_mempool {
	void *va;
	phys_addr_t pa;
	size_t size;
};

struct tui_mempool g_tuiMemPool;

#define COUNT_OF_ION_HANDLE (4)
static struct ion_client *client;
static struct ion_handle *handle[COUNT_OF_ION_HANDLE];

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
#if KERNEL_VERSION(3, 9, 0) >= LINUX_VERSION_CODE
static int is_device_ok(struct device *fbdev, void *p)
#else
static int is_device_ok(struct device *fbdev, const void *p)
#endif
{
	return 1;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static struct device *get_fb_dev(void)
{
	struct device *fbdev = NULL;

	/* get the first framebuffer device */
	/* [TODO] Handle properly when there are more than one framebuffer */
	fbdev = class_find_device(fb_class, NULL, NULL, is_device_ok);
	if (fbdev == NULL) {
		pr_debug("ERROR cannot get framebuffer device\n");
		return NULL;
	}
	return fbdev;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static struct fb_info *get_fb_info(struct device *fbdev)
{
	struct fb_info *fb_info;

	if (!fbdev->p) {
		pr_debug("ERROR framebuffer device has no private data\n");
		return NULL;
	}

	fb_info = (struct fb_info *)dev_get_drvdata(fbdev);
	if (!fb_info) {
		pr_debug("ERROR framebuffer device has no fb_info\n");
		return NULL;
	}

	return fb_info;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static int fb_tui_protection(void)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct decon_win *win;
	struct decon_device *decon;
	int ret = -ENODEV;

	fbdev = get_fb_dev();
	if (!fbdev) {
		pr_debug("get_fb_dev failed\n");
		return ret;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		pr_debug("get_fb_info failed\n");
		return ret;
	}

	win = fb_info->par;
	decon = win->decon;

	lock_fb_info(fb_info);
	ret = decon_tui_protection(true);
	unlock_fb_info(fb_info);
	return ret;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static int fb_tui_unprotection(void)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct decon_win *win;
	struct decon_device *decon;
	int ret = -ENODEV;

	fbdev = get_fb_dev();
	if (!fbdev) {
		pr_err("get_fb_dev failed\n");
		return ret;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		pr_err("get_fb_info failed\n");
		return ret;
	}

	win = fb_info->par;
	if (win == NULL) {
		pr_err("get win failed\n");
		return ret;
	}

	decon = win->decon;
	if (decon == NULL) {
		pr_err("get decon failed\n");
		return ret;
	}

	lock_fb_info(fb_info);
	ret = decon_tui_protection(false);
	unlock_fb_info(fb_info);

	return ret;
}
#endif

uint32_t hal_tui_init(void)
{
	return 0;
}

void hal_tui_exit(void)
{
}

uint32_t hal_tui_alloc(struct tui_alloc_buffer_t *allocbuffer,
		       size_t allocsize, uint32_t number)
{
	int ret = TUI_DCI_ERR_INTERNAL_ERROR;
	ion_phys_addr_t phys_addr;
	int i = 0;

	if (tui_cover_mode_on == true) {
		pr_info("[%s:%d] cover closed during tui open session!!\n",
			__func__, __LINE__);
		return ret;
	}

	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);

	client = exynos_ion_client_create("TUI module");
	if (IS_ERR(client)) {
		pr_err("failed to ion_client_create\n");
		return ret;
	}

	for (i = 0; i < number; i++) {
		handle[i] = ion_alloc(client, SZ_16M + SZ_1M, SZ_256K,
				      EXYNOS_ION_HEAP_VIDEO_STREAM_MASK, 0);
		if (IS_ERR_OR_NULL(handle[i])) {
			pr_err("[%s:%d] ION memory allocation fail.[err:%lx]\n",
			       __func__, __LINE__, PTR_ERR(handle[i]));
			//panic("[yurak] alloc fail panic\n");
			hal_tui_free();
			return ret;
		}
		//ion_phys(client, handle[i], (unsigned long *)&phys_addr, &phy_size);
		phys_addr = sg_phys(handle[i]->buffer->sg_table->sgl);
		pr_info("[%s:%d] phys_addr:%lx\n", __func__, __LINE__,
			phys_addr);
		allocbuffer[i].pa = (uint64_t) phys_addr;
		pr_info("[%s:%d] TUI buffer alloc idx:%d, pa:%lx\n", __func__,
			__LINE__, i, phys_addr);
	}

	ret = TUI_DCI_OK;

	return ret;
}

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
void tui_i2c_reset(void)
{
	void __iomem *i2c_reg;
	u32 tui_i2c;
	u32 i2c_conf;

	i2c_reg = ioremap(HSI2C7_PA_BASE_ADDRESS, SZ_4K);
	tui_i2c = readl(i2c_reg + HSI2C_CTL);
	tui_i2c |= HSI2C_SW_RST;
	writel(tui_i2c, i2c_reg + HSI2C_CTL);

	tui_i2c = readl(i2c_reg + HSI2C_CTL);
	tui_i2c &= ~HSI2C_SW_RST;
	writel(tui_i2c, i2c_reg + HSI2C_CTL);

	writel(0x4c4c4c00, i2c_reg + 0x0060);
	writel(0x26004c4c, i2c_reg + 0x0064);
	writel(0x99, i2c_reg + 0x0068);

	i2c_conf = readl(i2c_reg + HSI2C_CONF);
	writel((HSI2C_FUNC_MODE_I2C | HSI2C_MASTER), i2c_reg + HSI2C_CTL);

	writel(HSI2C_TRAILING_COUNT, i2c_reg + HSI2C_TRAILIG_CTL);
	writel(i2c_conf | HSI2C_AUTO_MODE, i2c_reg + HSI2C_CONF);

	iounmap(i2c_reg);
}
#endif

static uint32_t prepare_enable_i2c_clock(void)
{

	struct clk *i2c_clock;
	//struct clk *i2c_pclock;

	i2c_clock = clk_get(NULL, "clk_peric_usi03");
	if (IS_ERR(i2c_clock)) {
		pr_err("Can't get i2c clock\n");
		return -1;
	}
/*
	i2c_pclock = clk_get(NULL, "usi09-pclk");
	if (IS_ERR(i2c_pclock)) {
		pr_err("Can't get i2c clock\n");
	return -1;
	}
*/
	clk_prepare_enable(i2c_clock);
	//clk_prepare_enable(i2c_pclock);
	pr_info("[TUI] I2C clock prepared.\n");

	clk_put(i2c_clock);
	//clk_put(i2c_pclock);

	return 0;
}

static uint32_t disable_unprepare_i2c_clock(void)
{

	struct clk *i2c_clock;
	//struct clk *i2c_pclock;

	i2c_clock = clk_get(NULL, "clk_peric_usi03");
	if (IS_ERR(i2c_clock)) {
		pr_err("Can't get i2c clock\n");
		return -1;
	}
/*
	i2c_pclock = clk_get(NULL, "usi09-pclk");
	if (IS_ERR(i2c_Pclock)) {
		pr_err("Can't get i2c clock\n");
	return -1;
	}
*/
	clk_disable_unprepare(i2c_clock);
	//clk_disable_unprepare(i2c_pclock);
	pr_info("[TUI] I2C clock released.\n");

	clk_put(i2c_clock);
	//clk_put(i2c_pclock);

	return 0;
}

void hal_tui_free(void)
{
	int i;

	for (i = 0; i < COUNT_OF_ION_HANDLE; i++) {
		if (!IS_ERR_OR_NULL(handle[i])) {
			pr_info("[%s:%d] TUI buffer free idx:%d\n", __func__,
				__LINE__, i);
			ion_free(client, handle[i]);
			handle[i] = NULL;
		}
	}

	ion_client_destroy(client);
}

uint32_t hal_tui_deactivate(void)
{
	int ret = TUI_DCI_OK;

	//switch_set_state(&tui_switch, TRUSTEDUI_MODE_VIDEO_SECURED);
	pr_info("Disable touch!\n");
	disable_irq(tsp_irq_num);

	pr_info("tsp_irq_num =%d\n", tsp_irq_num);

#if defined(CONFIG_TOUCHSCREEN_SEC_TS) || defined(CONFIG_TOUCHSCREEN_SEC_TS_Y771)
	tui_delay(5);
	trustedui_mode_on();
	tui_delay(5);
	trustedui_set_mask(TRUSTEDUI_MODE_INPUT_SECURED);
	tui_delay(90);
#elif defined(CONFIG_TOUCHSCREEN_MELFAS_MSS100)
	tui_delay(5);
	mms_trustedui_mode_on();
	tui_delay(5);
	trustedui_set_mask(TRUSTEDUI_MODE_INPUT_SECURED);
	tui_delay(90);
#else
	trustedui_set_mask(TRUSTEDUI_MODE_INPUT_SECURED);
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("blanking!\n");

	trustedui_blank_set_counter(0);

	ret = fb_tui_protection();
	if (ret != 0) {
		pr_info("blanking ERR! fb_tui_protection ret = %d\n", ret);

		//TSP enable
		//switch_set_state(&tui_switch, TRUSTEDUI_MODE_OFF);
		enable_irq(tsp_irq_num);

		/* Clear linux TUI flag */
		trustedui_set_mode(TRUSTEDUI_MODE_OFF);

		return TUI_DCI_ERR_INTERNAL_ERROR;
	}
#endif // #CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	prepare_enable_i2c_clock();

	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED);

	pr_info("Ready to use TUI!\n");

	return TUI_DCI_OK;
}

uint32_t hal_tui_activate(void)
{
	int ret = TUI_DCI_OK;

	// Protect NWd
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED |
			     TRUSTEDUI_MODE_INPUT_SECURED);

	disable_unprepare_i2c_clock();

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unblanking\n");
	ret = fb_tui_unprotection();
	if (ret != 0) {
		pr_info("unblanking ERR! fb_tui_unprotection ret = %d retry!!!\n", ret);
		//tui_delay(500);
		ret = fb_tui_unprotection();
		if (ret == -ENODEV)
			pr_info("unblanking retry ERR!!! fb_tui_unprotection ret = %d\n", ret);
	}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unsetting TUI flag (blank counter=%d)",
		trustedui_blank_get_counter());
#endif

	//switch_set_state(&tui_switch, TRUSTEDUI_MODE_OFF);
	enable_irq(tsp_irq_num);

#if defined(CONFIG_TOUCHSCREEN_SEC_TS) || defined(CONFIG_TOUCHSCREEN_SEC_TS_Y771)
	tui_delay(5);
	trustedui_mode_off();
	tui_delay(95);
#endif

	/* Clear linux TUI flag */
	trustedui_set_mode(TRUSTEDUI_MODE_OFF);

	pr_info("Success to exit TUI!\n");

	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
uint32_t hal_tui_process_cmd(struct tui_hal_cmd_t *cmd,
			     struct tui_hal_rsp_t *rsp)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
uint32_t hal_tui_notif(void)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
void hal_tui_post_start(struct tlc_tui_response_t *rsp)
{
	pr_info("%s\n", __func__);
}
