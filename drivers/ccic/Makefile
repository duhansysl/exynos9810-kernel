#
# Makefile for usbpd devices
#

ifeq ($(CONFIG_CCIC_S2MU107), y)
	obj-$(CONFIG_CCIC_NOTIFIER)	+= s2mu107_misc.o
else ifeq ($(CONFIG_CCIC_S2MU106), y)
	obj-$(CONFIG_CCIC_NOTIFIER)	+= s2mu107_misc.o
else
	obj-$(CONFIG_CCIC_NOTIFIER)	+= ccic_core.o ccic_misc.o
endif

obj-$(CONFIG_CCIC_S2MU004)	+= s2mu004-usbpd.o
obj-$(CONFIG_CCIC_S2MU106)	+= s2mu106-usbpd.o
obj-$(CONFIG_CCIC_S2MU107)	+= s2mu107-usbpd.o
obj-$(CONFIG_CCIC_S2MU106)	+= s2mu106-usbpd.o
obj-$(CONFIG_CCIC_S2MU205)	+= s2mu205-usbpd.o
obj-$(CONFIG_USE_CCIC)		+= usbpd.o usbpd_cc.o ccic_sysfs.o
obj-$(CONFIG_USE_CCIC)		+= usbpd_policy.o usbpd_manager.o pdic_notifier.o
obj-$(CONFIG_CCIC_S2MM005)		+= s2mm005_fw.o s2mm005_cc.o s2mm005_pd.o s2mm005.o
obj-$(CONFIG_CCIC_NOTIFIER)		+= ccic_notifier.o 
obj-$(CONFIG_CCIC_ALTERNATE_MODE)	+= ccic_alternate.o ccic_misc.o
obj-$(CONFIG_CCIC_MAX77705)		+= max77705_cc.o max77705_pd.o sec_pd.o max77705_usbc.o max77705_alternate.o max77705_sysfs.o
obj-$(CONFIG_CCIC_MAX77705_DEBUG)	+= max77705_debug.o
