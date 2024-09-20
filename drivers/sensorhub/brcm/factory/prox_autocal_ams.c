#include "../ssp.h"

#define	VENDOR		"AMS"

#if defined(CONFIG_SENSORS_SSP_TMD4903)
#define CHIP_ID		"TMD4903"
#elif defined(CONFIG_SENSORS_SSP_TMD4904)
#define CHIP_ID		"TMD4904"
#elif defined(CONFIG_SENSORS_SSP_TMD4905)
#define CHIP_ID		"TMD4905"
#elif defined(CONFIG_SENSORS_SSP_TMD4906)
#define CHIP_ID		"TMD4906"
#elif defined(CONFIG_SENSORS_SSP_TMD3725)
#define CHIP_ID		"TMD3725"
#else
#define CHIP_ID		"UNKNOWN"
#endif

#define PROX_ADC_BITS_NUM		14

#define PROX_CAL_FILE_PATH	"/efs/FactoryApp/gyro_cal_data"
#define PROX_CAL_FILE_INDEX			25

/*************************************************************************/
/* Functions                                                             */
/*************************************************************************/

static u16 get_proximity_rawdata(struct ssp_data *data)
{
	u16 uRowdata = 0;
	char chTempbuf[4] = { 0 };

	s32 dMsDelay = 20;

	memcpy(&chTempbuf[0], &dMsDelay, 4);

	if (data->bProximityRawEnabled == false) {
		send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 4);
		msleep(200);
		uRowdata = data->buf[PROXIMITY_RAW].prox_raw[0];
		send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
			chTempbuf, 4);
	} else {
		uRowdata = data->buf[PROXIMITY_RAW].prox_raw[0];
	}

	return uRowdata;
}

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static ssize_t proximity_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t proximity_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t proximity_probe_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	bool probe_pass_fail = FAIL;

	if (data->uSensorState & (1 << PROXIMITY_SENSOR))
		probe_pass_fail = SUCCESS;
	else
		probe_pass_fail = FAIL;

	pr_info("[SSP]: %s - All sensor 0x%llx, prox_sensor %d\n",
		__func__, data->uSensorState, probe_pass_fail);

	return snprintf(buf, PAGE_SIZE, "%d\n", probe_pass_fail);
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: ProxThresh = hi - %u\n", data->uProxHiThresh);

	return sprintf(buf, "%u\n", data->uProxHiThresh);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if (uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM, uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxHiThresh = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hi - %u\n", __func__, data->uProxHiThresh);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: ProxThresh = lo - %u\n", data->uProxLoThresh);

	return sprintf(buf, "%u\n", data->uProxLoThresh);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if (uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM, uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxLoThresh = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : lo - %u\n", __func__, data->uProxLoThresh);

	return size;
}

static ssize_t proximity_thresh_detect_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: ProxThresh = hidetect - %u\n", data->uProxHiThresh_detect);

	return sprintf(buf, "%u\n", data->uProxHiThresh_detect);
}

static ssize_t proximity_thresh_detect_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if (uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM, uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxHiThresh_detect = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : hidetect - %u\n", __func__, data->uProxHiThresh_detect);

	return size;
}

static ssize_t proximity_thresh_detect_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: ProxThresh = lodetect - %u\n", data->uProxLoThresh_detect);

	return sprintf(buf, "%u\n", data->uProxLoThresh_detect);
}

static ssize_t proximity_thresh_detect_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 uNewThresh;
	int iRet, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	iRet = kstrtou16(buf, 10, &uNewThresh);
	if (iRet < 0)
		pr_err("[SSP]: %s - kstrto16 failed.(%d)\n", __func__, iRet);
	else {
		if (uNewThresh & prox_non_bits_mask)
			pr_err("[SSP]: %s - allow %ubits.(%d)\n", __func__, PROX_ADC_BITS_NUM, uNewThresh);
		else {
			uNewThresh &= prox_bits_mask;
			data->uProxLoThresh_detect = uNewThresh;
		}
	}

	ssp_dbg("[SSP]: %s - new prox threshold : lodetect - %u\n", __func__, data->uProxLoThresh_detect);

	return size;
}


static ssize_t proximity_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", get_proximity_rawdata(data));
}

static ssize_t proximity_default_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;
	u8 buffer[8] = {0,};
	int trim;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_PROX_GET_TRIM;
	msg->length = 2;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);
		return FAIL;
	}

	if (buffer[1] == 0xff)
		trim = (0xff - buffer[0]) * (-1);
	else
		trim = buffer[0];

	pr_info("[SSP] %s - %d, 0x%x, 0x%x\n", __func__, trim, buffer[1], buffer[0]);

	return snprintf(buf, PAGE_SIZE, "%d\n", trim);
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[PROXIMITY_RAW].prox_raw[1],
		data->buf[PROXIMITY_RAW].prox_raw[2],
		data->buf[PROXIMITY_RAW].prox_raw[3]);
}

/*
 *	Proximity Raw Sensor Register/Unregister
 */
static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char chTempbuf[4] = { 0 };
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	s32 dMsDelay = 20;

	memcpy(&chTempbuf[0], &dMsDelay, 4);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;
	if (atomic64_read(&data->aSensorEnable) & (1 << PROXIMITY_SENSOR)) {
		if (dEnable) {
			send_instruction(data, ADD_SENSOR, PROXIMITY_RAW, chTempbuf, 4);
			data->bProximityRawEnabled = true;
		} else {
			send_instruction(data, REMOVE_SENSOR, PROXIMITY_RAW,
				chTempbuf, 4);
			data->bProximityRawEnabled = false;
		}
	}

	return size;
}

static ssize_t barcode_emul_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->bBarcodeEnabled);
}

static ssize_t barcode_emul_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable)
		set_proximity_barcode_enable(data, true);
	else
		set_proximity_barcode_enable(data, false);

	return size;
}

#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static ssize_t proximity_setting_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t proximity_setting_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	u8 val[2];
	char *token;
	char *str;
	struct ssp_msg *msg;
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s - %s\n", __func__, buf);

	//parsing
	str = (char *)buf;
	token = strsep(&str, "\n");
	if (token == NULL) {
		pr_err("[SSP] %s : too few arguments (2 needed)", __func__);
			return -EINVAL;
	}

	iRet = kstrtou8(token, 10, &val[0]);
	if (iRet < 0) {
		pr_err("[SSP] %s : kstrtou8 error %d", __func__, iRet);
		return iRet;
	}

	token = strsep(&str, "\n");
	if (token == NULL) {
		pr_err("[SSP] %s : too few arguments (2 needed)", __func__);
			return -EINVAL;
	}

	iRet = kstrtou8(token, 16, &val[1]);
	if (iRet < 0) {
		pr_err("[SSP] %s : kstrtou8 error %d", __func__, iRet);
		return iRet;
	}

	pr_info("[SSP] %s - index = %d value = 0x%x\n", __func__, val[0], val[1]);

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_PROX_SETTING;
	msg->length = 2;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(2, GFP_KERNEL);

	msg->free_buffer = 1;
	memcpy(msg->buffer, val, 2);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	return size;
}
#endif

static ssize_t proximity_alert_thresh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->uProxAlertHiThresh);
}

static ssize_t prox_light_get_dhr_sensor_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;
	u8 buffer[18] = {0, };
	u8 chipId = 0;
	u16 threshold_high = 0;
	u16 threshold_low = 0;
	u16 threshold_detect_high = 0;
	u16 threshold_detect_low = 0;
	u8 p_drive_current = 0;
	u8 persistent_time = 0;
	u8 p_pulse = 0;
	u8 p_gain = 0;
	u8 p_time = 0;
	u8 p_pulse_length = 0;
	u8 l_atime = 0;
	int offset = 0;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_GET_PROXIMITY_LIGHT_DHR_SENSOR_INFO;
	msg->length = 18;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);
		return FAIL;
	}

	chipId = buffer[0];
	threshold_high = ((((u16) buffer[2]) << 8) & 0xff00) | ((((u16) buffer[1])) & 0x00ff);
	threshold_low = ((((u16) buffer[4]) << 8) & 0xff00) | ((((u16) buffer[3])) & 0x00ff);
	threshold_detect_high = ((((u16) buffer[6]) << 8) & 0xff00) | ((((u16) buffer[5])) & 0x00ff);
	threshold_detect_low = ((((u16) buffer[8]) << 8) & 0xff00) | ((((u16) buffer[7])) & 0x00ff);
	p_drive_current = buffer[9];
	persistent_time = buffer[10];
	p_pulse = buffer[11];
	p_gain = buffer[12];
	p_time = buffer[13];
	p_pulse_length = buffer[14];
	l_atime = buffer[15];

	if (buffer[17] == 0xff)
		offset = (0xff - buffer[16]) * (-1);
	else
		offset = buffer[16];

	pr_info("[SSP] %s - %02x, %d, %d, %d, %d, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %d", __func__,
		chipId, threshold_high, threshold_low, threshold_detect_high, threshold_detect_low,
		p_drive_current, persistent_time, p_pulse, p_gain, p_time, p_pulse_length, l_atime, offset);

	return snprintf(buf, PAGE_SIZE, "\"THD\":\"%d %d %d %d\","\
		"\"PDRIVE_CURRENT\":\"%02x\","\
		"\"PERSIST_TIME\":\"%02x\","\
		"\"PPULSE\":\"%02x\","\
		"\"PGAIN\":\"%02x\","\
		"\"PTIME\":\"%02x\","\
		"\"PPLUSE_LEN\":\"%02x\","\
		"\"ATIME\":\"%02x\","\
		"\"POFFSET\":\"%d\"\n",
		threshold_high, threshold_low, threshold_detect_high, threshold_detect_low,
		p_drive_current, persistent_time, p_pulse, p_gain, p_time, p_pulse_length, l_atime, offset);
}

int load_prox_cal_from_nvm(int *cal_data, int size)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	if (size < 2 * sizeof(int)) {
		pr_err("[SSP]: %s - invalid size(%d)", __func__, size);
		return -EIO;
	}
	pr_info("[SSP] %s ", __func__);


	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(PROX_CAL_FILE_PATH, O_RDONLY, 0660);
	if (IS_ERR(cal_filp)) {
		ret = PTR_ERR(cal_filp);
		if (ret != -ENOENT)
			pr_err("[SSP]: %s - Can't open cancellation file\n",
				__func__);
		goto exit;
	}

	cal_filp->f_pos = PROX_CAL_FILE_INDEX;
	ret = vfs_read(cal_filp, (u8 *)cal_data, size, &cal_filp->f_pos);
	if (ret != size) {
		memset(cal_data, 0, size);
		pr_err("[SSP]: %s - Can't read the cancel data\n", __func__);
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
exit:
	set_fs(old_fs);

	pr_info("[SSP] %s: flag %d, threshold %d\n", __func__, cal_data[0], cal_data[1]);

	return ret;
}

int proximity_save_calibration(int *cal_data, int size)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	if (size < 2 * sizeof(int)) {
		pr_err("[SSP]: %s - invalid size(%d)", __func__, size);
		return -EIO;
	}

	pr_info("[SSP] %s ", __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(PROX_CAL_FILE_PATH, O_CREAT | O_WRONLY | O_NOFOLLOW | O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open calibration file\n", __func__);
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return -EIO;
	}

	cal_filp->f_pos = PROX_CAL_FILE_INDEX;

	ret = vfs_write(cal_filp, (char *)cal_data, size, &cal_filp->f_pos);
	if (ret < 0) {
		pr_err("[SSP]: %s - Can't write prox cal to file\n", __func__);
		ret = -EIO;
	}
	
	pr_info("[SSP] %s: flag %d, threshold %d\n", __func__, cal_data[0], cal_data[1]);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int set_prox_cal_to_ssp(struct ssp_data *data)
{
	int ret = 0;
	int prox_cal[2] = {0, };
	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << PROXIMITY_SENSOR))) {
		pr_info("[SSP] %s - Skip this function!!!, proximity sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return -EIO;
	}

	pr_info("[SSP] %s ", __func__);

	ret = load_prox_cal_from_nvm(prox_cal, sizeof(prox_cal));
	
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_MCU_SET_PROX_CAL;
	msg->length = sizeof(prox_cal);
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(sizeof(prox_cal), GFP_KERNEL);
	msg->free_buffer = 1;
	memcpy(msg->buffer, &prox_cal, sizeof(prox_cal));

	ret = ssp_spi_async(data, msg);

	if (ret != SUCCESS) {
		pr_err("[SSP] %s - fail %d\n", __func__, ret);
	}

	pr_info("[SSP] %s : flag: %d, threshold %d", __func__, prox_cal[0], prox_cal[1]);

	return ret;
}

static ssize_t proximity_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int cal_data[2] = {0, };
	
	load_prox_cal_from_nvm(cal_data, sizeof(cal_data));

	pr_info("[SSP] %s ", __func__);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", cal_data[0], cal_data[1]);
}

static ssize_t proximity_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet = 0;
	int64_t enable = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	int cal_data[2] = {0, };
	

	if (!(data->uSensorState & (1 << PROXIMITY_SENSOR))) {
		pr_info("[SSP] %s - Skip this function!!!, proximity sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return -EINVAL;
	}

	iRet = kstrtoll(buf, 10, &enable);

	if (iRet < 0)
		return iRet;

	pr_info("[SSP] %s - enable %d", __func__, enable);
	
	if (enable) {
		// ToDo: start proximity calibration
		iRet = ssp_send_cmd(data, MSG2SSP_AP_PROX_CAL_START, 0);

	} else {
		iRet = proximity_save_calibration(cal_data, sizeof(cal_data));
		if (iRet < 0)
			pr_err("[SSP] %s : initilaize fail", __func__); 
	}

	return size;
}

static ssize_t proximity_offset_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", 1);
}

static DEVICE_ATTR(vendor, 0440, proximity_vendor_show, NULL);
static DEVICE_ATTR(name, 0440, proximity_name_show, NULL);
static DEVICE_ATTR(prox_probe, 0440, proximity_probe_show, NULL);
static DEVICE_ATTR(thresh_high, 0660, proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, 0660, proximity_thresh_low_show, proximity_thresh_low_store);
static DEVICE_ATTR(thresh_detect_high, 0660, proximity_thresh_detect_high_show, proximity_thresh_detect_high_store);
static DEVICE_ATTR(thresh_detect_low, 0660, proximity_thresh_detect_low_show, proximity_thresh_detect_low_store);
static DEVICE_ATTR(prox_trim, 0440, proximity_default_trim_show, NULL);
static DEVICE_ATTR(raw_data, 0440, proximity_raw_data_show, NULL);
static DEVICE_ATTR(prox_avg, 0660, proximity_avg_show, proximity_avg_store);

static DEVICE_ATTR(barcode_emul_en, 0660, barcode_emul_enable_show, barcode_emul_enable_store);
static DEVICE_ATTR(prox_cal, 0660, proximity_cal_show, proximity_cal_store);
static DEVICE_ATTR(prox_offset_pass, 0440, proximity_offset_pass_show, NULL);

#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static DEVICE_ATTR(setting, 0660, proximity_setting_show, proximity_setting_store);
#endif

static DEVICE_ATTR(prox_alert_thresh, 0440, proximity_alert_thresh_show, NULL);
static DEVICE_ATTR(dhr_sensor_info, 0440, prox_light_get_dhr_sensor_info_show, NULL);

static struct device_attribute *prox_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_prox_probe,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_thresh_detect_high,
	&dev_attr_thresh_detect_low,
	&dev_attr_prox_trim,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_barcode_emul_en,
#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
	&dev_attr_setting,
#endif
	&dev_attr_prox_alert_thresh,
	&dev_attr_dhr_sensor_info,
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
	NULL,
};

void initialize_prox_factorytest(struct ssp_data *data)
{
	sensors_register(data->prox_device, data,
		prox_attrs, "proximity_sensor");
}

void remove_prox_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->prox_device, prox_attrs);
}
