obj-$(CONFIG_B43)		+= b43/
obj-$(CONFIG_B43LEGACY)		+= b43legacy/

obj-$(CONFIG_BRCMFMAC)	+= brcm80211/
obj-$(CONFIG_BRCMSMAC)	+= brcm80211/

ifneq ($(CONFIG_BCM4375),)
obj-$(CONFIG_BCM4375)   += bcmdhd_101_16/
obj-$(CONFIG_SOC_EXYNOS9810) += bcmdhd_101_16/dhd_custom_memprealloc.o
obj-$(CONFIG_SOC_EXYNOS9810) += bcmdhd_101_16/dhd_custom_exynos.o
endif
