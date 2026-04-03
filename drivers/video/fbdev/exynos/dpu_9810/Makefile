#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_DPP) += dpp.o
dpp-y := dpp_drv.o
obj-$(CONFIG_EXYNOS_MIPI_DSIM) += dsim.o
dsim-y += dsim_drv.o
obj-$(CONFIG_EXYNOS_DISPLAYPORT) += displayport.o
displayport-y += displayport_drv.o displayport_edid.o displayport_hdcp13.o displayport_reg.o secdp_unit_test.o secdp_aux_control.o
obj-$(CONFIG_SOC_EXYNOS9810) += dsim_reg.o dpp_reg.o decon_reg.o
obj-$(CONFIG_EXYNOS_DECON_9810) += decon.o
obj-$(CONFIG_DECON_EVENT_LOG) += event_log.o
obj-$(CONFIG_EXYNOS9810_BTS) += bts.o
obj-$(CONFIG_SUPPORT_DSU) += dsu.o
#obj-$(CONFIG_SUPPORT_DOZE) += decon_doze.o dsim_doze.o
decon-y += decon_core.o decon_dsi.o decon_displayport.o helper.o win_update.o cursor.o
obj-y += panels/
obj-y += dp_logger/
