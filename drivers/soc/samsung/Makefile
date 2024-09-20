#
# Exynos SoC Specific drivers
#

ccflags-y := $(KBUILD_FP_SENSOR_CFLAGS)

obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o
obj-$(CONFIG_EXYNOS_PM_DOMAINS) += pm_domains.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-smc.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-el3_mon.o
# CAL
obj-$(CONFIG_CAL_IF)		+= cal-if/
obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o
obj-$(CONFIG_EXYNOS_PD)        += exynos-pd.o exynos-pd-dbg.o
obj-$(CONFIG_EXYNOS_RGT)        += exynos-rgt.o
obj-$(CONFIG_EXYNOS_FLEXPMU_DBG)        += exynos-flexpmu-dbg.o

# ACPM
obj-$(CONFIG_EXYNOS_ACPM)       += acpm/

obj-$(CONFIG_EXYNOS_PM_DVS)        += exynos-pm-dvs.o

obj-$(CONFIG_ECT)		+= ect_parser.o
obj-$(CONFIG_EXYNOS_BCM)	+= exynos-bcm.o

# USI
obj-$(CONFIG_USI) += usi.o

# reboot
obj-$(CONFIG_EXYNOS_REBOOT)	+= exynos-reboot.o

#PM
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pm.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-handler.o

# USI_V2
obj-$(CONFIG_USI_V2) += usi_v2.o

#CPUIDLE_PROFILER
obj-$(CONFIG_ARM64_EXYNOS_CPUIDLE)	+= cpuidle_profiler.o

#CPUHOTPLUG
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-cpu_hotplug.o
obj-$(CONFIG_EXYNOS_HOTPLUG_GOVERNOR)	+= exynos-hotplug_governor.o

# DVFS
obj-$(CONFIG_EXYNOS_DVFS_MANAGER)	+= exynos-dm.o
obj-$(CONFIG_EXYNOS_MCINFO)	+= exynos-mcinfo.o

# OCP
obj-$(CONFIG_EXYNOS_OCP)	+= exynos-ocp.o

# CP
obj-$(CONFIG_SEC_SIPC_MODEM_IF) += pmu-cp.o
obj-$(CONFIG_SEC_MODEM_IF)  	+= pmu-cp.o

# Exynos Secure Log
obj-$(CONFIG_EXYNOS_SECURE_LOG)	+= exynos-seclog.o

# SDM
obj-$(CONFIG_EXYNOS_SDM) += exynos-sdm.o

# HDCP
obj-$(CONFIG_EXYNOS_HDCP2)  += exynos-hdcp/

# secmem
obj-$(CONFIG_ARCH_EXYNOS)	+= secmem.o
