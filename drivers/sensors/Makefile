#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_SENSORS_CORE)          += sensors_core.o

# combo sensor

# optical_sensor
obj-$(CONFIG_SENSORS_TMD3725)       += tmd3725.o

# grip_sensor
obj-$(CONFIG_SENSORS_A96T3X6)		+= a96t3x6.o
obj-$(CONFIG_SENSORS_A96T3X6_WIFI)		+= a96t3x6_wifi.o

# magnetic_sensor
obj-$(CONFIG_SENSORS_AK09918C)	    += ak09916c.o

# accelerometer_sensor
obj-$(CONFIG_SENSORS_ICM42605M_SPI)        += icm42605m_spi.o
obj-$(CONFIG_SENSORS_ICM42605M)            += icm42605m_common.o

