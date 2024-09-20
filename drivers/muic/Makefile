#
# Makefile for muic devices
#

obj-$(CONFIG_MUIC_NOTIFIER)		+= muic_notifier.o
obj-$(CONFIG_MUIC_MANAGER)		+= muic_manager.o
obj-$(CONFIG_MUIC_CORE)			+= muic_core.o
obj-$(CONFIG_MUIC_SYSFS)		+= muic_sysfs.o
obj-$(CONFIG_MUIC_S2MU004)		+= muic_core.o s2mu004-muic.o s2mu004-muic-sysfs.o
obj-$(CONFIG_MUIC_S2MU106)		+= s2mu106-muic.o
obj-$(CONFIG_MUIC_S2MU205)		+= s2mu205-muic.o
obj-$(CONFIG_MUIC_S2MU107)		+= s2mu107-muic.o
obj-$(CONFIG_HV_MUIC_S2MU004_AFC)	+= s2mu004-muic-afc.o
obj-$(CONFIG_HV_MUIC_S2MU106_AFC)	+= s2mu106-muic-afc.o
obj-$(CONFIG_HV_MUIC_S2MU107_AFC)	+= s2mu107-muic-afc.o
ifdef CONFIG_MUIC_SUPPORT_CCIC
obj-$(CONFIG_MUIC_S2MU005)		+= s2mu005-muic-typec.o
else
obj-$(CONFIG_MUIC_S2MU005)		+= muic-core.o s2mu005-muic.o
endif
obj-$(CONFIG_MUIC_UNIVERSAL)	+= universal/
obj-$(CONFIG_MUIC_MAX77705)		+= muic-core.o max77705-muic.o
obj-$(CONFIG_HV_MUIC_MAX77705_AFC)	+= max77705-muic-afc.o
obj-$(CONFIG_MUIC_MAX77705_CCIC)	+= max77705-muic-ccic.o
obj-$(CONFIG_MUIC_SM5504_POGO)		+= sm5504-muic-pogo.o

