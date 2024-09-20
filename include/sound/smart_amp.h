#ifndef _SMART_AMP_H
#define _SMART_AMP_H

#include <linux/types.h>
#include <sound/samsung/abox.h>
#include <sound/soc.h>
#include <linux/of.h>

#define SMART_AMP 

/* Below should be same as in aDSP code */
#define TI_SMARTPA_VENDOR_ID	1234

#define MAX_DSP_PARAM_INDEX		600

#define TAS_PAYLOAD_SIZE 	14
#define TAS_GET_PARAM		1
#define TAS_SET_PARAM		0

#define MAX_CHANNELS 	2
#define LEFT			0
#define RIGHT			1
#define CHANNEL0		1
#define CHANNEL1		2

#define TRUE		1
#define FALSE		0

#define SLAVE1		0x98
#define SLAVE2		0x9A
#define SLAVE3		0x9C
#define SLAVE4		0x9E

#define TAS_SA_IS_SPL_IDX(X)	((((X) >= 3810) && ((X) < 3899)) ? 1 : 0)
#define TAS_CALC_PARAM_IDX(INDEX, LENGTH, CHANNEL)		((INDEX ) | (LENGTH << 16) | (CHANNEL << 24))

#define TAS_SA_GET_F0          3810
#define TAS_SA_GET_Q           3811
#define TAS_SA_GET_TV          3812
#define TAS_SA_GET_RE          3813
#define TAS_SA_CALIB_INIT      3814
#define TAS_SA_CALIB_DEINIT    3815
#define TAS_SA_SET_RE          3816
#define TAS_SA_SET_PROFILE     3819
#define TAS_SA_GET_STATUS      3821
#define TAS_SA_SET_SPKID       3822
#define TAS_SA_SET_TCAL        3823
#define TAS_SA_EXC_TEMP_STAT   3824
#define TAS_SA_IV_VBAT_FMT     3825
#define TAS_SA_VALID_INIT      		3831
#define TAS_SA_VALID_DEINIT	   		3832
#define TAS_SA_GET_VALID_STATUS     3833

#define TAS_SA_GET_RE_LOW		398
#define TAS_SA_GET_RE_HIGH		399

#define VALIDATION_SUCCESS	0xC00DC00D
#define WR_DATA					0
#define RD_DATA					1
typedef enum {
	IV_SENSE_FORMAT_NO_VBAT = 0,
	IV_SENSE_FORMAT_12_BIT_WITH_8BIT_VBAT = 1,
} ti_smartamp_iv_vbat_format_t;

struct afe_smartamp_set_params_t {
	uint32_t  payload[TAS_PAYLOAD_SIZE];
} __packed;

struct ti_smartpa_data {
	uint32_t payload[TAS_PAYLOAD_SIZE];
};

int ti_smartpa_read(void *prm_data, int offset, int size);
int ti_smartpa_write(void *prm_data, int offset, int size);

int afe_smartamp_algo_ctrl(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length);

void tas25xx_parse_algo_dt(struct device_node *np);
bool tas25xx_set_iv_bit_fomat(int iv_data_with, int vbat, int update_now);
void tas25xx_send_algo_calibration(void);
void tas25xx_update_big_data(void);
void smartamp_add_algo(uint8_t channels);
void smartamp_remove_algo(void);

#endif /*_SMART_AMP_H*/
