obj-$(CONFIG_CAMERA_FLASH_LM3560_OBJ) += fimc-is-flash-lm3560.o
obj-$(CONFIG_CAMERA_FLASH_RT5033_OBJ) += fimc-is-flash-rt5033.o
obj-$(CONFIG_CAMERA_FLASH_S2MPB02_OBJ) += fimc-is-flash-s2mpb02.o
obj-$(CONFIG_CAMERA_FLASH_GPIO_OBJ) += fimc-is-flash-gpio.o
obj-$(CONFIG_CAMERA_FLASH_KTD2692_OBJ) += fimc-is-flash-ktd2692.o
obj-$(CONFIG_CAMERA_FLASH_S2MU106_OBJ) += fimc-is-flash-s2mu106.o
obj-$(CONFIG_CAMERA_FLASH_S2MU107_OBJ) += fimc-is-flash-s2mu107.o
obj-$(CONFIG_CAMERA_FLASH_RT8547_OBJ) += fimc-is-flash-rt8547.o
obj-$(CONFIG_CAMERA_FLASH_I2C_OBJ) += fimc-is-helper-flash-i2c.o \
				      fimc-is-flash-i2c.o

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/vendor
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/hardware/api
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/sensor/module_framework
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/sensor
ifeq ($(CONFIG_COMPANION_DIRECT_USE),y)
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/vendor/mcd/preprocessor
endif
