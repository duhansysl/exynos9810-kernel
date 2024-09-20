/*
 * STMicroelectronics icm42605m spi driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Giuseppe Barba <giuseppe.barba@st.com>
 * Mario Tesi <mario.tesi@st.com>
 * v 1.2.2
 * Licensed under the GPL-2.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/types.h>

#include "icm42605m_common.h"

#define SENSORS_SPI_READ 0x80

static int icm42605m_spi_read(struct icm42605m_data *cdata, u8 reg_addr, int len,
			    u8 *data, bool b_lock)
{
	int err;
	struct spi_message msg;

	struct spi_transfer xfers[] = {
		{
			.tx_buf = cdata->tb.tx_buf,
			.bits_per_word = 8,
			.len = 1,
		},
		{
			.rx_buf = cdata->tb.rx_buf,
			.bits_per_word = 8,
			.len = len,
		}
	};

	if (b_lock)
		mutex_lock(&cdata->bank_registers_lock);

	mutex_lock(&cdata->tb.buf_lock);
	cdata->tb.tx_buf[0] = reg_addr | SENSORS_SPI_READ;

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);

	err = spi_sync(to_spi_device(cdata->dev), &msg);
	if (err)
		goto acc_spi_read_error;

	memcpy(data, cdata->tb.rx_buf, len*sizeof(u8));
	mutex_unlock(&cdata->tb.buf_lock);
	if (b_lock)
		mutex_unlock(&cdata->bank_registers_lock);

	return 0;

acc_spi_read_error:
	mutex_unlock(&cdata->tb.buf_lock);
	if (b_lock)
		mutex_unlock(&cdata->bank_registers_lock);
    
	return err;
}

static int icm42605m_spi_write(struct icm42605m_data *cdata, u8 reg_addr, int len,
				 u8 *data, bool b_lock)
{
	int err;
	int i;
	struct spi_message msg;

	struct spi_transfer xfers = {
		.tx_buf = cdata->tb.tx_buf,
		.bits_per_word = 8,
		.len = len + 1,
	};

	if (len >= ICM42605M_RX_MAX_LENGTH)
		return -ENOMEM;

	for(i = 0;i < len; i++) {
		if (b_lock)
			mutex_lock(&cdata->bank_registers_lock);
		mutex_lock(&cdata->tb.buf_lock);
		cdata->tb.tx_buf[0] = reg_addr + i;
		memcpy(&cdata->tb.tx_buf[1], data + i, 1);
		spi_message_init(&msg);
		spi_message_add_tail(&xfers, &msg);
		err = spi_sync(to_spi_device(cdata->dev), &msg);
		mutex_unlock(&cdata->tb.buf_lock);
		if (b_lock)
			mutex_unlock(&cdata->bank_registers_lock);
	}

	return 0;
}

static const struct icm42605m_transfer_function icm42605m_tf_spi = {
	.write = icm42605m_spi_write,
	.read = icm42605m_spi_read,
};

static int icm42605m_spi_probe(struct spi_device *spi)
{
	int err;

	struct icm42605m_data *cdata;

	cdata = kzalloc(sizeof(*cdata), GFP_KERNEL);
	if (!cdata)
		return -ENOMEM;

	cdata->dev = &spi->dev;
	cdata->name = spi->modalias;
	cdata->tf = &icm42605m_tf_spi;
	spi_set_drvdata(spi, cdata);

	err = icm42605m_common_probe(cdata, spi->irq, BUS_SPI);
	if (err < 0)
		goto free_data;

	return 0;

free_data:
	kfree(cdata);

	return err;
}

static int icm42605m_spi_remove(struct spi_device *spi)
{
	/* TODO: check the function */
	struct icm42605m_data *cdata = spi_get_drvdata(spi);

	icm42605m_common_remove(cdata);
	dev_info(cdata->dev, "%s: removed\n", ICM42605M_DEV_NAME);
	kfree(cdata);

	return 0;
}


static void icm42605m_spi_shutdown(struct spi_device *spi)
{
	struct icm42605m_data *cdata = spi_get_drvdata(spi);

	icm42605m_common_shutdown(cdata);
	dev_info(cdata->dev, "%s: shutdowned\n", ICM42605M_DEV_NAME);
}

#ifdef CONFIG_PM
static int icm42605m_suspend(struct device *dev)
{
	struct icm42605m_data *cdata = spi_get_drvdata(to_spi_device(dev));

	return icm42605m_common_suspend(cdata);
}

static int icm42605m_resume(struct device *dev)
{
	struct icm42605m_data *cdata = spi_get_drvdata(to_spi_device(dev));

	return icm42605m_common_resume(cdata);
}

static const struct dev_pm_ops icm42605m_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(icm42605m_suspend, icm42605m_resume)
};

#define ICM42605M_PM_OPS		(&icm42605m_pm_ops)
#else /* CONFIG_PM */
#define ICM42605M_PM_OPS		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static const struct spi_device_id icm42605m_ids[] = {
	{ ICM42605M_DEV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, icm42605m_ids);

static const struct of_device_id icm42605m_id_table[] = {
	{ .compatible = "tdk,icm42605m", },
	{ },
};
MODULE_DEVICE_TABLE(of, icm42605m_id_table);
#endif

static struct spi_driver icm42605m_spi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = ICM42605M_DEV_NAME,
		.pm = ICM42605M_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = icm42605m_id_table,
#endif
	},
	.probe    = icm42605m_spi_probe,
	.remove   = icm42605m_spi_remove,
	.shutdown = icm42605m_spi_shutdown,
	.id_table = icm42605m_ids,
};

module_spi_driver(icm42605m_spi_driver);

MODULE_DESCRIPTION("TDK Invensense icm42605m spi driver");
MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL v2");
