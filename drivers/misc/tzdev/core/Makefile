ccflags-$(CONFIG_TZDEV_DEPLOY_TZAR) += -D"_STR(s)=\#s" -D"KBUILD_SRC=_STR($(KBUILD_SRC))"

obj-$(CONFIG_TZDEV)		+= cdev.o
obj-$(CONFIG_TZDEV)		+= cred.o
obj-$(CONFIG_TZDEV)		+= event.o
obj-$(CONFIG_TZDEV)		+= iwio.o
obj-$(CONFIG_TZDEV)		+= iwservice.o
obj-$(CONFIG_TZDEV)		+= iwsock.o
obj-$(CONFIG_TZDEV)		+= log.o
obj-$(CONFIG_TZDEV)		+= mem.o
obj-$(CONFIG_TZDEV)		+= notifier.o
obj-$(CONFIG_TZDEV)		+= ree_time.o
obj-$(CONFIG_TZDEV)		+= sysdep.o
obj-$(CONFIG_TZDEV)		+= tzdev.o
obj-$(CONFIG_TZDEV)		+= umem.o
obj-$(CONFIG_TZDEV)		+= uiwsock.o
obj-$(CONFIG_TZLOG)		+= iwlog.o

obj-$(CONFIG_TZDEV_DEPLOY_TZAR)	+= deploy_tzar.o

obj-$(CONFIG_TZDEV_KTHREADS_NOTIFIER)	+= kthread_pool_notifier.o
obj-$(CONFIG_TZDEV_KTHREADS_SMPBOOT)	+= kthread_pool_smpboot.o

ifeq ($(CONFIG_MSM_SCM), y)
obj-$(CONFIG_TZDEV)		+= qc_platform.o
obj-$(CONFIG_TZDEV)		+= qc_clocks.o
else
obj-$(CONFIG_TZDEV)		+= platform.o
endif
