obj-$(CONFIG_EXYNOS_COMMON_PANEL) += common_mipi_lcd.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA2K)	+= s6e3ha2k_mipi_lcd.o s6e3ha2k_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HF4)	+= s6e3hf4_mipi_lcd.o s6e3hf4_lcd_ctrl.o
#obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA6)	+= s6e3ha6_mipi_lcd.o s6e3ha6_lcd_ctrl.o
#obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA8)	+= s6e3ha8_mipi_lcd.o s6e3ha8_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_EMUL_DISP) += emul_disp_mipi_lcd.o emul_disp_lcd_ctrl.o

ifdef CONFIG_EXYNOS_MASS_PANEL
obj-y += dsim_panel.o ../decon_board.o ../decon_notify.o ../decon_abd.o
obj-$(CONFIG_DISPLAY_USE_INFO) += dpui.o
obj-$(CONFIG_EXYNOS_DECON_MDNIE) += mdnie.o

obj-$(and $(CONFIG_EXYNOS_DECON_MDNIE),$(CONFIG_EXYNOS_LCD_ENG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_mdnie.o
obj-$(and $(CONFIG_EXYNOS_LCD_ENG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_dpu.o dd_param.o

obj-$(CONFIG_EXYNOS_DECON_LCD_ANA6705_A51)	+= ana6705_a51_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FA9_ANOTE)	+= s6e3fa9_anote_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_HX83102E_GTACTIVE3)	+= hx83102e_gtactive3_lcd_ctrl.o
endif
