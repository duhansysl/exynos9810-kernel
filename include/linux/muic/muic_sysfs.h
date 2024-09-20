#ifndef MUIC_SYSFS_H
#define MUIC_SYSFS_H

#ifdef CONFIG_MUIC_SYSFS
extern struct device *muic_device_create(void *drvdata, const char *fmt);
extern void muic_device_destroy(dev_t devt);
int muic_sysfs_init(struct muic_platform_data *muic_pdata);
void muic_sysfs_deinit(struct muic_platform_data *muic_pdata);
#else
static inline struct device *muic_device_create(void *drvdata, const char *fmt)
{
	pr_err("No rule to make muic sysfs\n");
	return NULL;
}
static inline void muic_device_destroy(dev_t devt)
{
	return;
}
#endif

#endif /* MUIC_SYSFS_H */
