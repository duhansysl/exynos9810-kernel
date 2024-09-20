# Makefile for the Himax touchscreen drivers.

ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

obj-$(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)   		+=	himax_debug.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)   		+=	himax_inspection.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INCELL_CORE)   	+=	himax_ic_incell_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_ONCELL_CORE)   	+=	himax_ic_oncell_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xH)   	+=	himax_ic_HX852xH.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xG)   	+=	himax_ic_HX852xG.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)   	+=	himax_ic_HX83102.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83191)   	+=	himax_ic_HX83191.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)   	+=	himax_ic_HX83112.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83111)   	+=	himax_ic_HX83111.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83103)   	+=	himax_ic_HX83103.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_COMMON)   		+=	himax_common.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_I2C) 			+= himax_platform.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_SPI) 			+= himax_platform_SPI.o

ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
ccflags-y += -D__EMBEDDED_FW__

ld_array_start_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_start
ld_array_start_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_start_str))))
obj_array_start_sym = _binary___Himax_firmware_bin_start

ld_array_size_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_size
ld_array_size_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_size_str))))
obj_array_size_sym = _binary___Himax_firmware_bin_size

ld_array_end_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_end
ld_array_end_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_end_str))))
obj_array_end_sym = _binary___Himax_firmware_bin_end

$(src)/Himax_firmware.o: $(src)/Himax_firmware.bin FORCE
	$(LD) $(LDFLAGS) -r -b binary $(srctree)/$(src)/Himax_firmware.bin -o $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_start_sym)=$(obj_array_start_sym) $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_size_sym)=$(obj_array_size_sym) $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_end_sym)=$(obj_array_end_sym) $(objtree)/$(obj)/Himax_firmware.o

endif

ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui