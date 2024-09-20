#ifndef _LINUX_WACOM_I2C_H_
#define _LINUX_WACOM_I2C_H_

#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif

#define CMD_RESULT_WORD_LEN	20
struct wacom_g5_platform_data {
	int irq_gpio;
	int pdct_gpio;
	int fwe_gpio;
	int boot_addr;
	int irq_type;
	int pdct_type;
	int x_invert;
	int y_invert;
	int xy_switch;
	bool use_dt_coord;
	u32 origin[2];
	int max_x;
	int max_y;
	int max_pressure;
	int max_x_tilt;
	int max_y_tilt;
	int max_height;
	const char *fw_path;
#ifdef CONFIG_SEC_FACTORY
	const char *fw_fac_path;
#endif
	u32 ic_type;
	u32 module_ver;
	bool use_garage;
	u32 table_swap;
	bool use_vddio;
	u32 bringup;

	u32	area_indicator;
	u32	area_navigation;
	u32	area_edge;
};

#endif /* _LINUX_WACOM_I2C_H */
