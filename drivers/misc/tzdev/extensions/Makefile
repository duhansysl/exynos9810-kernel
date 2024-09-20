obj-$(CONFIG_TZDEV_BOOST)	+= boost.o
obj-$(CONFIG_TZDEV_HOTPLUG)	+= hotplug.o
obj-$(CONFIG_TZIRS)		+= irs.o
obj-$(CONFIG_TZTUI)		+= tui_fts.o
obj-$(CONFIG_TZ_NWFS)		+= fsdev.o
obj-$(CONFIG_TZ_SCMA)		+= scma.o

ifeq ($(CONFIG_SOC_EXYNOS9830), y)
obj-$(CONFIG_ION_FD2PHYS)		+= ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_2
else ifeq ($(CONFIG_SOC_EXYNOS9820), y)
obj-$(CONFIG_ION_FD2PHYS)		+= ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_2
else ifeq ($(CONFIG_SOC_EXYNOS9810), y)
obj-$(CONFIG_ION_FD2PHYS)		+= ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_2
else ifeq ($(CONFIG_SOC_EXYNOS9630), y)
obj-$(CONFIG_ION_FD2PHYS)		+= ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_1
else ifeq ($(CONFIG_SOC_EXYNOS9610), y)
obj-$(CONFIG_ION_FD2PHYS)		+= ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_1
else ifeq ($(CONFIG_ARCH_RANCHU), y)
obj-$(CONFIG_ION_FD2PHYS)		+= ion_fd2phys_v1.o
else
obj-$(CONFIG_ION_FD2PHYS)		+= ion_fd2phys.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_1
endif
