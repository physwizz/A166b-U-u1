/*#define DEBUG*/
#include <linux/module.h>
#include <linux/debugfs.h>
#include <asm/ioctls.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/version.h>

#include "aw_device.h"
#include "aw_dsp.h"
#include "aw_log.h"
/*#include "aw_afe.h"*/

static DEFINE_MUTEX(g_aw_dsp_msg_lock);
static DEFINE_MUTEX(g_aw_dsp_lock);


#define AW_MSG_ID_ENABLE_CALI		(0x00000001)
#define AW_MSG_ID_ENABLE_HMUTE		(0x00000002)
#define AW_MSG_ID_F0_Q			(0x00000003)
#define AW_MSG_ID_DIRECT_CUR_FLAG	(0x00000006)
#define AW_MSG_ID_SPK_STATUS		(0x00000007)
#define AW_MSG_ID_VERSION		(0x00000008)
#define AW_MSG_ID_VERSION_NEW		(0x00000012)
#define AW_MSG_ID_PREDICTED_TEMP_L	(0x0000001C)
#define AW_MSG_ID_PREDICTED_TEMP_R	(0x0000001D)
#define AW_MSG_ID_AMBIENT_TEMP_L	(0x00000016)
#define AW_MSG_ID_AMBIENT_TEMP_R	(0x00000017)
#define AW_MSG_ID_SURFACE_TEMP		(0x00000021)
#define AW_MSG_ID_BATTERY_CAPACITY	(0x00000027)

#define AW_AMBIENT_TEMP_ACCURACY	(20)
/*dsp params id*/
#define AW_MSG_ID_RX_SET_ENABLE		(0x10013D11)
#define AW_MSG_ID_PARAMS		(0x10013D12)
#define AW_MSG_ID_TX_SET_ENABLE		(0x10013D13)
#define AW_MSG_ID_VMAX_L		(0X10013D17)
#define AW_MSG_ID_VMAX_R		(0X10013D18)
#define AW_MSG_ID_CALI_CFG_L		(0X10013D19)
#define AW_MSG_ID_CALI_CFG_R		(0x10013d1A)
#define AW_MSG_ID_RE_L			(0x10013d1B)
#define AW_MSG_ID_RE_R			(0X10013D1C)
#define AW_MSG_ID_NOISE_L		(0X10013D1D)
#define AW_MSG_ID_NOISE_R		(0X10013D1E)
#define AW_MSG_ID_F0_L			(0X10013D1F)
#define AW_MSG_ID_F0_R			(0X10013D20)
#define AW_MSG_ID_REAL_DATA_L		(0X10013D21)
#define AW_MSG_ID_REAL_DATA_R		(0X10013D22)

#define AFE_MSG_ID_MSG_0	(0X10013D2A)
#define AFE_MSG_ID_MSG_1	(0X10013D2B)
#define AFE_MSG_ID_MSG_2	(0X10013D36)
#define AFE_MSG_ID_MSG_3	(0X10013D33)
#define AW_MSG_ID_PARAMS		(0x10013D12)
#define AW_MSG_ID_PARAMS_1		(0x10013D2D)
#define AW_MSG_ID_PARAMS_2		(0x10013D41)
#define AW_MSG_ID_PARAMS_3		(0x10013D35)
#define AW_MSG_ID_PARAMS_DEFAULT	(0x10013D37)

#define AW_MSG_ID_SPIN		(0x10013D2E)

int g_tx_topo_id = AW_TX_DEFAULT_TOPO_ID;
int g_rx_topo_id = AW_RX_DEFAULT_TOPO_ID;
int g_tx_port_id = AW_TX_DEFAULT_PORT_ID;
int g_rx_port_id = AW_RX_DEFAULT_PORT_ID;

enum {
	MSG_PARAM_ID_0 = 0,
	MSG_PARAM_ID_1,
	MSG_PARAM_ID_2,
	MSG_PARAM_ID_3,
	MSG_PARAM_ID_MAX,
};

static uint32_t afe_param_msg_id[MSG_PARAM_ID_MAX] = {
	AFE_MSG_ID_MSG_0,
	AFE_MSG_ID_MSG_1,
	AFE_MSG_ID_MSG_2,
	AFE_MSG_ID_MSG_3,
};

/***************dsp communicate**************/
#ifdef AW_QCOM_PLATFORM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 1))
#include <dsp/msm_audio_ion.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6audio-v2.h>
#include <dsp/q6adm-v2.h>
#else
#include <linux/msm_audio_ion.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/q6adm-v2.h>
#include <sound/adsp_err.h>
#endif
#endif

#define AW_COPP_MODULE_ID (0X10013D02)			/*SKT module id*/
#define AW_COPP_PARAMS_ID_AWDSP_ENABLE (0X10013D14)	/*SKT enable param id*/

#ifdef AW_MTK_PLATFORM_WITH_DSP
extern int mtk_spk_send_ipi_buf_to_dsp(void *data_buffer, uint32_t data_size);
extern int mtk_spk_recv_ipi_buf_from_dsp(int8_t *buffer, int16_t size, uint32_t *buf_len);
#elif defined AW_ABOX_PLATFORM
extern int aw882xx_abox_write(const char *buf, int length);
extern int aw882xx_abox_read(char *buf, int length);
extern int aw882xx_abox_recover_algo_packet(void);
#elif defined AW_QCOM_PLATFORM
extern int afe_get_topology(int port_id);
extern int aw_send_afe_cal_apr(uint32_t param_id,
	void *buf, int cmd_size, bool write);
extern int aw_send_afe_rx_module_enable(void *buf, int size);
extern int aw_send_afe_tx_module_enable(void *buf, int size);
#else

static int aw_send_afe_cal_apr(uint32_t param_id,
	void *buf, int cmd_size, bool write)
{
	return 0;
}
static int aw_send_afe_rx_module_enable(void *buf, int size)
{
	return 0;
}
static int aw_send_afe_tx_module_enable(void *buf, int size)
{
	return 0;
}
static int afe_get_topology(int port_id)
{
	return 0;
}
#endif

#if 0
int aw882xx_abox_write(const char *buf, int length)
{
	return 0;
}
int aw882xx_abox_read(char *buf, int length)
{
	return 0;
}
int aw882xx_abox_recover_algo_packet(void)
{
	return 0;
}
int aw882xx_abox_sync_algo_param_load(void)
{
	return 0;
}
int aw882xx_abox_read_specific_data(char *buf, int length, int abox_id)
{
	return 0;
}
#endif

#ifdef AW_QCOM_PLATFORM
extern void aw_set_port_id(int tx_port_id, int rx_port_id);
#else
static void aw_set_port_id(int tx_port_id, int rx_port_id) {
	return;
}
#endif

static int aw_adm_param_enable(int port_id, int module_id, int param_id, int enable)
{
#if 0
	/*for v3*/
	int copp_idx = 0;
	uint32_t enable_param;
	struct param_hdr_v3 param_hdr;
	int rc = 0;

	pr_debug("%s port_id %d, module_id 0x%x, enable %d\n",
			__func__, port_id, module_id, enable);

	copp_idx = adm_get_default_copp_idx(port_id);
	if (copp_idx < 0 || copp_idx >= MAX_COPPS_PER_PORT) {
			pr_err("%s: Invalid copp_num: %d\n", __func__, copp_idx);
			return -EINVAL;
	}

	if (enable < 0 || enable > 1) {
			pr_err("%s: Invalid value for enable %d\n", __func__, enable);
			return -EINVAL;
	}

	pr_debug("%s port_id %d, module_id 0x%x, copp_idx 0x%x, enable %d\n",
			__func__, port_id, module_id, copp_idx, enable);

	memset(&param_hdr, 0, sizeof(param_hdr));
	param_hdr.module_id = module_id;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = param_id;
	param_hdr.param_size = sizeof(enable_param);
	enable_param = enable;

	rc = adm_pack_and_set_one_pp_param(port_id, copp_idx, param_hdr,
					(uint8_t *) &enable_param);
	if (rc)
		pr_err("%s: Failed to set enable of module(%d) instance(%d) to %d, err %d\n",
				__func__, module_id, INSTANCE_ID_0, enable, rc);
	return rc;
#endif
	return 0;
}

static int aw_get_msg_num(int dev_ch, int *msg_num)
{
	switch (dev_ch) {
	case AW_DEV_CH_PRI_L:
		*msg_num = MSG_PARAM_ID_0;
		break;
	case AW_DEV_CH_PRI_R:
		*msg_num = MSG_PARAM_ID_0;
		break;
	case AW_DEV_CH_SEC_L:
		*msg_num = MSG_PARAM_ID_1;
		break;
	case AW_DEV_CH_SEC_R:
		*msg_num = MSG_PARAM_ID_1;
		break;
	case AW_DEV_CH_TERT_L:
		*msg_num = MSG_PARAM_ID_2;
		break;
	case AW_DEV_CH_TERT_R:
		*msg_num = MSG_PARAM_ID_2;
		break;
	case AW_DEV_CH_QUAT_L:
		*msg_num = MSG_PARAM_ID_3;
		break;
	case AW_DEV_CH_QUAT_R:
		*msg_num = MSG_PARAM_ID_3;
		break;
	default:
		aw_pr_err("can not find msg num, channel %d ", dev_ch);
		return -EINVAL;
	}

	aw_pr_dbg("msg num[%d] ", *msg_num);
	return 0;
}

static void aw_dsp_get_check_sum(int pos, void *data, int size, int *check_sum)
{
	int i = 0;
	int sum_data = 0;

	for (i = pos; i < size / sizeof(uint8_t); i++)
		sum_data += *((uint8_t *)data + sizeof(uint8_t) * i);


	*check_sum = sum_data;
}

#ifdef AW_MTK_PLATFORM_WITH_DSP
/*****************mtk dsp communication function start**********************/
static int aw_mtk_write_data_to_dsp(int param_id, void *data, int size)
{
	int32_t *dsp_data = NULL;
	aw_dsp_msg_t *hdr = NULL;
	int ret = 0;

	dsp_data = kzalloc(sizeof(aw_dsp_msg_t) + size, GFP_KERNEL);
	if (!dsp_data) {
		aw_pr_err("kzalloc dsp_msg error\n");
		return -ENOMEM;
	}

	hdr = (aw_dsp_msg_t *)dsp_data;
	hdr->type = AW_DSP_MSG_TYPE_DATA;
	hdr->opcode_id = param_id;
	hdr->version = AW_DSP_MSG_HDR_VER;

	memcpy(((char *)dsp_data) + sizeof(aw_dsp_msg_t),
		data, size);

	ret = mtk_spk_send_ipi_buf_to_dsp(dsp_data,
				sizeof(aw_dsp_msg_t) + size);
	if (ret < 0) {
		aw_pr_err("write data failed");
		kfree(dsp_data);
		dsp_data = NULL;
		return ret;
	}

	kfree(dsp_data);
	dsp_data = NULL;
	return ret;
}

static int aw_mtk_read_data_from_dsp(int param_id, void *data, int size)
{
	int ret = 0;
	aw_dsp_msg_t hdr;

	hdr.type = AW_DSP_MSG_TYPE_CMD;
	hdr.opcode_id = param_id;
	hdr.version = AW_DSP_MSG_HDR_VER;

	mutex_lock(&g_aw_dsp_msg_lock);
	ret = mtk_spk_send_ipi_buf_to_dsp(&hdr, sizeof(aw_dsp_msg_t));
	if (ret < 0) {
		aw_pr_err("send cmd failed");
		goto dsp_msg_failed;
	}

	ret = mtk_spk_recv_ipi_buf_from_dsp(data, size, &size);
	if (ret < 0) {
		aw_pr_err("get data failed");
		goto dsp_msg_failed;
	}
	mutex_unlock(&g_aw_dsp_msg_lock);
	return 0;

dsp_msg_failed:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;
}

/*****************mtk dsp communication function end**********************/

/*****************abox dsp communication function start**********************/

#elif defined AW_ABOX_PLATFORM
static int aw_abox_write_data_to_dsp(int param_id, void *data, int size)
{
	int32_t *dsp_data = NULL;
	aw_dsp_msg_t *hdr = NULL;
	int ret;

	dsp_data = kzalloc(sizeof(aw_dsp_msg_t) + size, GFP_KERNEL);
	if (!dsp_data) {
		pr_err("%s: kzalloc dsp_msg error\n", __func__);
		return -ENOMEM;
	}

	hdr = (aw_dsp_msg_t *)dsp_data;
	hdr->type = AW_DSP_MSG_TYPE_DATA;
	hdr->opcode_id = param_id;
	hdr->version = AW_DSP_MSG_HDR_VER;

	memcpy(((char *)dsp_data) + sizeof(aw_dsp_msg_t),
		data, size);

	ret = aw882xx_abox_write((const char *)dsp_data,
				sizeof(aw_dsp_msg_t) + size);
	if (ret < 0) {
		pr_err("%s:write data failed\n", __func__);
		kfree(dsp_data);
		dsp_data = NULL;
		return ret;
	}

	kfree(dsp_data);
	dsp_data = NULL;
	return 0;
}

static int aw_abox_read_data_from_dsp(int param_id, void *data, int size)
{
	int ret;
	aw_dsp_msg_t hdr;

	hdr.type = AW_DSP_MSG_TYPE_CMD;
	hdr.opcode_id = param_id;
	hdr.version = AW_DSP_MSG_HDR_VER;

	mutex_lock(&g_aw_dsp_msg_lock);
	ret = aw882xx_abox_write((const char *)&hdr, sizeof(aw_dsp_msg_t));
	if (ret < 0) {
		pr_err("%s:send cmd failed\n", __func__);
		goto dsp_msg_failed;
	}

	ret = aw882xx_abox_read((char *)data, size);
	if (ret < 0) {
		pr_err("%s:get data failed\n", __func__);
		goto dsp_msg_failed;
	}
	mutex_unlock(&g_aw_dsp_msg_lock);
	return 0;

dsp_msg_failed:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;
}
/*****************abox dsp communication function end**********************/

#else
/*****************qcom dsp communication function start**********************/
static int aw_afe_get_topology(uint32_t param_id)
{
	if (param_id == AW_MSG_ID_TX_SET_ENABLE)
		return afe_get_topology(g_tx_port_id);
	else
		return afe_get_topology(g_rx_port_id);
}

static int aw_check_dsp_ready(uint32_t param_id)
{
	int ret;

	ret = aw_afe_get_topology(param_id);

	aw_pr_dbg("topo_id 0x%x ", ret);

	if (param_id == AW_MSG_ID_TX_SET_ENABLE) {
		if (ret != g_tx_topo_id)
			return false;
		else
			return true;
	} else {
		if (ret != g_rx_topo_id)
			return false;
		else
			return true;
	}
}

static int aw_qcom_write_data_to_dsp(uint32_t param_id, void *data, int size)
{
	int ret;
	int try = 0;

	mutex_lock(&g_aw_dsp_lock);
	while (try < AW_DSP_TRY_TIME) {
		if (aw_check_dsp_ready(param_id)) {
			ret = aw_send_afe_cal_apr(param_id, data, size, true);
			mutex_unlock(&g_aw_dsp_lock);
			return ret;
		} else {
			try++;
			usleep_range(AW_10000_US, AW_10000_US + 10);
			aw_pr_info("afe topo not ready try again");
		}
	}
	mutex_unlock(&g_aw_dsp_lock);

	return -EINVAL;
}

static int aw_qcom_read_data_from_dsp(uint32_t param_id, void *data, int size)
{
	int ret;
	int try = 0;

	mutex_lock(&g_aw_dsp_lock);
	while (try < AW_DSP_TRY_TIME) {
		if (aw_check_dsp_ready(param_id)) {
			ret = aw_send_afe_cal_apr(param_id, data, size, false);
			mutex_unlock(&g_aw_dsp_lock);
			return ret;
		} else {
			try++;
			usleep_range(AW_10000_US, AW_10000_US + 10);
			aw_pr_info("afe topo not ready try again");
		}
	}
	mutex_unlock(&g_aw_dsp_lock);

	return -EINVAL;
}
#endif

/******************* afe module communication function ************************/
static int aw_dsp_set_afe_rx_module_enable(void *buf, int size)
{
#ifdef AW_MTK_PLATFORM_WITH_DSP
	return aw_mtk_write_data_to_dsp(AW_MSG_ID_RX_SET_ENABLE, buf, size);
#elif defined AW_ABOX_PLATFORM
	return aw_abox_write_data_to_dsp(AW_MSG_ID_RX_SET_ENABLE, buf, size);
#else
	return aw_send_afe_rx_module_enable(buf, size);
#endif
}

static int aw_dsp_set_afe_tx_module_enable(void *buf, int size)
{
#ifdef AW_MTK_PLATFORM_WITH_DSP
	return aw_mtk_write_data_to_dsp(AW_MSG_ID_TX_SET_ENABLE, buf, size);
#elif defined AW_ABOX_PLATFORM
	return aw_abox_write_data_to_dsp(AW_MSG_ID_TX_SET_ENABLE, buf, size);
#else
	return aw_send_afe_tx_module_enable(buf, size);
#endif
}

static int aw_dsp_get_afe_rx_module_enable(void *buf, int size)
{
#ifdef AW_MTK_PLATFORM_WITH_DSP
	return aw_mtk_read_data_from_dsp(AW_MSG_ID_RX_SET_ENABLE, buf, size);
#elif defined AW_ABOX_PLATFORM
	return aw_abox_read_data_from_dsp(AW_MSG_ID_RX_SET_ENABLE, buf, size);
#else
	return aw_qcom_read_data_from_dsp(AW_MSG_ID_RX_SET_ENABLE, buf, size);
#endif
}

static int aw_dsp_get_afe_tx_module_enable(void *buf, int size)
{
#ifdef AW_MTK_PLATFORM_WITH_DSP
	return aw_mtk_read_data_from_dsp(AW_MSG_ID_TX_SET_ENABLE, buf, size);
#elif defined AW_ABOX_PLATFORM
	return aw_abox_read_data_from_dsp(AW_MSG_ID_TX_SET_ENABLE, buf, size);
#else
	return aw_qcom_read_data_from_dsp(AW_MSG_ID_TX_SET_ENABLE, buf, size);
#endif
}

/******************* read/write msg communication function ***********************/
static int aw_read_msg_from_dsp(struct aw_device *aw_dev, 
	uint32_t msg_id, char *data_ptr, unsigned int data_size)
{
	int ret = 0;
	int msg_num = -EINVAL;
	aw_dsp_msg_t hdr[2];

	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_pr_err("already power off, do not read msg");
		return -EINVAL;
	}

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	hdr[0].type = AW_DSP_MSG_TYPE_DATA;
	hdr[0].opcode_id = afe_param_msg_id[msg_num];
	hdr[0].version = AW_DSP_MSG_HDR_VER;
	hdr[1].type = AW_DSP_MSG_TYPE_CMD;
	hdr[1].opcode_id = msg_id;
	hdr[1].version = AW_DSP_MSG_HDR_VER;

	mutex_lock(&g_aw_dsp_msg_lock);
#ifdef AW_MTK_PLATFORM_WITH_DSP
	ret = mtk_spk_send_ipi_buf_to_dsp(&hdr, 2 * sizeof(aw_dsp_msg_t));
#elif defined AW_ABOX_PLATFORM
	ret = aw882xx_abox_write((const char *)&hdr, 2 * sizeof(aw_dsp_msg_t));
#else
	ret = aw_qcom_write_data_to_dsp(afe_param_msg_id[msg_num],
				&hdr[1], sizeof(aw_dsp_msg_t));
#endif
	if (ret < 0) {
		aw_pr_err("msg_id:0x%x, send cmd failed", msg_id);
		goto dsp_msg_failed;
	}
#ifdef AW_MTK_PLATFORM_WITH_DSP
	ret = mtk_spk_recv_ipi_buf_from_dsp(data_ptr, data_size, &data_size);
#elif defined AW_ABOX_PLATFORM
	ret = aw882xx_abox_read((char *)data_ptr, data_size);
#else
	ret = aw_qcom_read_data_from_dsp(afe_param_msg_id[msg_num],
			data_ptr, (int)data_size);
#endif
	if (ret < 0) {
		aw_pr_err("msg_id:0x%x, read data failed", msg_id);
		goto dsp_msg_failed;
	}


	mutex_unlock(&g_aw_dsp_msg_lock);
	return 0;

dsp_msg_failed:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;
}

static int aw_read_msg_from_dsp_v_1_0_0_0(struct aw_device *aw_dev,
		uint32_t params_id, char *data_ptr, unsigned int len, int num)
{
	int ret = 0;
	int sum_data = 0;
	int32_t *dsp_msg = NULL;
	aw_msg_hdr_t write_hdr;
	aw_msg_hdr_t *read_hdr = NULL;
#ifdef AW_MTK_PLATFORM_WITH_DSP
	int real_len = 0;
#endif
	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_pr_err("already power off, do not read msg");
		return -EINVAL;
	}

	memset(&write_hdr, 0, sizeof(aw_msg_hdr_t));
	write_hdr.version = AW_DSP_MSG_VER;
	write_hdr.type = DSP_MSG_TYPE_WRITE_CMD;
	write_hdr.params_id = params_id;
	write_hdr.channel = aw_dev->channel;
	write_hdr.num = num;
	write_hdr.data_size = len / num;

	aw_dsp_get_check_sum(sizeof(int32_t), &write_hdr, sizeof(aw_msg_hdr_t), &write_hdr.checksum);

	mutex_lock(&g_aw_dsp_msg_lock);
#ifdef AW_MTK_PLATFORM_WITH_DSP
	ret = mtk_spk_send_ipi_buf_to_dsp(&write_hdr, sizeof(aw_msg_hdr_t));
#elif defined AW_ABOX_PLATFORM
	ret = aw882xx_abox_write((const char *)&write_hdr,sizeof(aw_msg_hdr_t));
#else
	ret = aw_qcom_write_data_to_dsp(AW_MSG_ID_PARAMS_DEFAULT, &write_hdr, sizeof(aw_msg_hdr_t));
#endif
	if (ret < 0) {
		aw_pr_err("write data to dsp failed");
		goto write_hdr_error;
	}

	dsp_msg = kzalloc(sizeof(aw_msg_hdr_t) + len, GFP_KERNEL);
	if (!dsp_msg) {
		ret = -ENOMEM;
		goto kalloc_msg_error;
	}

#ifdef AW_MTK_PLATFORM_WITH_DSP
	ret = mtk_spk_recv_ipi_buf_from_dsp((char *)dsp_msg, sizeof(aw_msg_hdr_t) + len, &real_len);
#elif defined AW_ABOX_PLATFORM
	ret = aw882xx_abox_read((char *)dsp_msg, sizeof(aw_msg_hdr_t) + len);
#else
	ret = aw_qcom_read_data_from_dsp(AW_MSG_ID_PARAMS_DEFAULT, (char *)dsp_msg,
		sizeof(aw_msg_hdr_t) + len);
#endif

	if (ret < 0) {
		aw_pr_err("read data from dsp failed %d", ret);
		goto read_msg_error;
	}

	read_hdr = (aw_msg_hdr_t *)dsp_msg;
	if (read_hdr->type != DSP_MSG_TYPE_READ_DATA) {
		aw_pr_err("read_hdr type = %d not read data!", read_hdr->type);
		ret = -EINVAL;
		goto read_msg_error;
	}

	aw_dsp_get_check_sum(sizeof(int32_t), dsp_msg, sizeof(aw_msg_hdr_t) + len, &sum_data);

	if (sum_data != read_hdr->checksum) {
		aw_pr_err("aw_dsp_msg check sum error!");
		aw_pr_err("read_hdr->checksum=%d sum_data=%d", read_hdr->checksum, sum_data);
		ret = -EINVAL;
		goto read_msg_error;
	}

	memcpy(data_ptr, ((char *)dsp_msg) + sizeof(aw_msg_hdr_t), len);

read_msg_error:
	kfree(dsp_msg);
	dsp_msg = NULL;
write_hdr_error:
kalloc_msg_error:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;

}

int aw_dsp_read_dsp_msg(struct aw_device *aw_dev, uint32_t msg_id, char *data_ptr, unsigned int size)
{
	int ret = 0;

	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_pr_err("already power off, do not read msg");
		return -EINVAL;
	}

	if (aw_dev->channel < AW_DEV_CH_TERT_L) {
		ret = aw_read_msg_from_dsp(aw_dev, msg_id, data_ptr, size);
	} else {
		ret = aw_read_msg_from_dsp_v_1_0_0_0(aw_dev, msg_id, data_ptr,
			size, AW_DSP_CHANNEL_DEFAULT_NUM);
	}

	return ret;
}

static int aw_write_msg_to_dsp(struct aw_device *aw_dev,
	uint32_t msg_id, char *data_ptr, unsigned int data_size)
{
	int ret = 0;
	int msg_num = -EINVAL;
	int32_t *dsp_msg = NULL;
	aw_dsp_msg_t *hdr = NULL;
	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_pr_err("already power off, do not write msg");
		return -EINVAL;
	}

	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	dsp_msg = kzalloc(sizeof(aw_dsp_msg_t) + data_size,
			GFP_KERNEL);
	if (!dsp_msg)
		return -ENOMEM;

	hdr = (aw_dsp_msg_t *)dsp_msg;
	hdr->type = AW_DSP_MSG_TYPE_DATA;
	hdr->opcode_id = msg_id;
	hdr->version = AW_DSP_MSG_HDR_VER;

	memcpy(((char *)dsp_msg) + sizeof(aw_dsp_msg_t),
			data_ptr, data_size);

#ifdef AW_MTK_PLATFORM_WITH_DSP
	ret = aw_mtk_write_data_to_dsp(afe_param_msg_id[msg_num], (void *)dsp_msg,
					sizeof(aw_dsp_msg_t) + data_size);
#elif defined AW_ABOX_PLATFORM
	ret = aw_abox_write_data_to_dsp(afe_param_msg_id[msg_num], (void *)dsp_msg,
					sizeof(aw_dsp_msg_t) + data_size);
#else
	ret = aw_qcom_write_data_to_dsp(afe_param_msg_id[msg_num], (void *)dsp_msg,
					sizeof(aw_dsp_msg_t) + data_size);
#endif
	if (ret < 0) {
		aw_pr_err("msg_id:0x%x, write data failed", msg_id);
		kfree(dsp_msg);
		dsp_msg = NULL;
		return ret;
	}

	kfree(dsp_msg);
	dsp_msg = NULL;
	return ret;
}

static int aw_write_msg_to_dsp_v_1_0_0_0(struct aw_device *aw_dev,
		uint32_t params_id, void *data, unsigned int len, int num)
{
	int ret = 0;
	int32_t *dsp_msg = NULL;
	aw_msg_hdr_t *hdr = NULL;

	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_pr_err("already power off, do not write msg");
		return -EINVAL;
	}

	mutex_lock(&g_aw_dsp_msg_lock);
	dsp_msg = kzalloc(sizeof(aw_msg_hdr_t) + len, GFP_KERNEL);
	if (!dsp_msg) {
		aw_pr_err("kzalloc dsp_msg error");
		ret = -ENOMEM;
		goto kalloc_error;
	}
	hdr = (aw_msg_hdr_t *)dsp_msg;
	hdr->version = AW_DSP_MSG_VER;
	hdr->type = DSP_MSG_TYPE_WRITE_DATA;
	hdr->params_id = params_id;
	hdr->channel = aw_dev->channel;
	hdr->num = num;
	hdr->data_size = len / num;

	memcpy(((char *)dsp_msg) + sizeof(aw_msg_hdr_t), data, len);

	aw_dsp_get_check_sum(sizeof(int32_t), dsp_msg, sizeof(aw_msg_hdr_t) + len, &hdr->checksum);

#ifdef AW_MTK_PLATFORM_WITH_DSP
	ret = mtk_spk_send_ipi_buf_to_dsp(dsp_msg, sizeof(aw_msg_hdr_t) + len);
#elif defined AW_ABOX_PLATFORM
	ret = aw882xx_abox_write((const char *)dsp_msg,sizeof(aw_msg_hdr_t) + len);
#else
	ret = aw_qcom_write_data_to_dsp(AW_MSG_ID_PARAMS_DEFAULT, dsp_msg, sizeof(aw_msg_hdr_t) + len);
#endif
	if (ret < 0) {
		aw_pr_err("write data failed %d", ret);
		goto write_msg_error;
	}

write_msg_error:
	kfree(dsp_msg);
	dsp_msg = NULL;
kalloc_error:
	mutex_unlock(&g_aw_dsp_msg_lock);
	return ret;

}

int aw_dsp_write_dsp_msg(struct aw_device *aw_dev, uint32_t msg_id, char *data_ptr, unsigned int size)
{
	int ret = 0;
	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_pr_err("already power off, do not write msg");
		return -EINVAL;
	}

	if (aw_dev->channel < AW_DEV_CH_TERT_L) {
		ret = aw_write_msg_to_dsp(aw_dev, msg_id, data_ptr, size);
	} else {
		ret = aw_write_msg_to_dsp_v_1_0_0_0(aw_dev, msg_id, data_ptr,
			size, AW_DSP_CHANNEL_DEFAULT_NUM);
	}

	return ret;
}
/******************* read/write data communication function ***********************/
static int aw_read_data_from_dsp(uint32_t param_id, void *data, int size)
{
#ifdef AW_MTK_PLATFORM_WITH_DSP
	return aw_mtk_read_data_from_dsp(param_id, data, size);
#elif defined AW_ABOX_PLATFORM
	return aw_abox_read_data_from_dsp(param_id, data, size);
#else
	return aw_qcom_read_data_from_dsp(param_id, data, size);
#endif
}

static int aw_write_data_to_dsp(uint32_t param_id, void *data, int size)
{
#ifdef AW_MTK_PLATFORM_WITH_DSP
	return aw_mtk_write_data_to_dsp(param_id, data, size);
#elif defined AW_ABOX_PLATFORM
	return aw_abox_write_data_to_dsp(param_id, data, size);
#else
	return aw_qcom_write_data_to_dsp(param_id, data, size);
#endif
}

int aw_dsp_recover_algo_packet(void)
{
#ifdef AW_ABOX_PLATFORM
	return aw882xx_abox_recover_algo_packet();
#else
	return 0;
#endif
}

/************************* dsp communication function *****************************/
int aw_dsp_set_afe_module_en(int type, int enable)
{
	int ret = 0;

	switch (type) {
	case AW_RX_MODULE:
		ret = aw_dsp_set_afe_rx_module_enable(&enable, sizeof(int32_t));
		break;
	case AW_TX_MODULE:
		ret = aw_dsp_set_afe_tx_module_enable(&enable, sizeof(int32_t));
		break;
	default:
		aw_pr_err("unsupported type %d", type);
		return -EINVAL;
	}

	return ret;
}

int aw_dsp_get_afe_module_en(int type, int *status)
{
	int ret = 0;

	switch (type) {
	case AW_RX_MODULE:
		ret = aw_dsp_get_afe_rx_module_enable(status, sizeof(int32_t));
		break;
	case AW_TX_MODULE:
		ret = aw_dsp_get_afe_tx_module_enable(status, sizeof(int32_t));
		break;
	default:
		aw_pr_err("unsupported type %d", type);
		return -EINVAL;
	}

	return ret;
}

int aw_dsp_read_te(struct aw_device *aw_dev, int32_t *te)
{
	int ret = 0;
	int32_t data[8] = {0}; /*[re:r0:Te:r0_te]*/


	ret = aw_dsp_read_dsp_msg(aw_dev, AW_MSG_ID_SPK_STATUS,
				(char *)data, sizeof(int32_t) * 8);
	if (ret) {
		aw_dev_err(aw_dev->dev, " read Te failed ");
		return ret;
	}

	if ((aw_dev->channel % 2) == 0)
		*te = data[2];
	else
		*te = data[6];

	aw_dev_dbg(aw_dev->dev, "read Te %d", *te);
	return ret;
}

int aw_dsp_read_st(struct aw_device *aw_dev, int32_t *r0, int32_t *te)
{
	int ret = 0;
	int32_t data[8] = {0}; /*[re:r0:Te:r0_te]*/


	ret = aw_dsp_read_dsp_msg(aw_dev, AW_MSG_ID_SPK_STATUS,
				(char *)data, sizeof(int32_t) * 8);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read spk st failed");
		return ret;
	}

	if ((aw_dev->channel % 2) == 0) {
		*r0 = AW_DSP_RE_TO_SHOW_RE(data[0]);
		*te = data[2];
	} else {
		*r0 = AW_DSP_RE_TO_SHOW_RE(data[4]);
		*te = data[6];
	}
	aw_dev_dbg(aw_dev->dev, "read Re %d , Te %d", *r0, *te);
	return ret;
}

int aw_dsp_read_spin(int *spin_mode)
{
	int ret = 0;
	int32_t spin = 0;

	ret = aw_read_data_from_dsp(AW_MSG_ID_SPIN, &spin, sizeof(int32_t));
	if (ret) {
		*spin_mode = 0;
		aw_pr_err("read spin failed ");
		return ret;
	}
	*spin_mode = spin;
	aw_pr_dbg("read spin done");
	return ret;
}

int aw_dsp_write_spin(int spin_mode)
{
	int ret = 0;
	int32_t spin = spin_mode;

	if (spin >= AW_SPIN_MAX) {
		aw_pr_err("spin [%d] unsupported ", spin);
		return -EINVAL;
	}

	ret = aw_write_data_to_dsp(AW_MSG_ID_SPIN, &spin, sizeof(int32_t));
	if (ret) {
		aw_pr_err("write spin failed ");
		return ret;
	}
	aw_pr_dbg("write spin done");
	return ret;
}

int aw_dsp_read_r0(struct aw_device *aw_dev, int32_t *r0)
{
	uint32_t msg_id = -EINVAL;
	int ret = 0;
	int32_t data[6] = {0};


	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_REAL_DATA_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_REAL_DATA_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_read_dsp_msg(aw_dev, msg_id, (char *)data, sizeof(int32_t) * 6);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read real re failed ");
		return ret;
	}

	*r0 = AW_DSP_RE_TO_SHOW_RE(data[0]);
	aw_dev_dbg(aw_dev->dev, "read r0 %d\n", *r0);
	return ret;
}

int aw_dsp_read_cali_data(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	uint32_t msg_id = -EINVAL;
	int ret = 0;


	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_REAL_DATA_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_REAL_DATA_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_read_dsp_msg(aw_dev, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read cali dara failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "read cali_data");
	return ret;
}

int aw_dsp_get_dc_status(struct aw_device *aw_dev)
{
	int ret = 0;
	int32_t data[2] = {0};


	ret = aw_dsp_read_dsp_msg(aw_dev, AW_MSG_ID_DIRECT_CUR_FLAG, (char *)data, sizeof(int32_t) * 2);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read dc flag failed");
		return ret;
	}

	if ((aw_dev->channel % 2) == 0)
		ret = data[0];
	else
		ret = data[1];

	aw_dev_dbg(aw_dev->dev, "read direct current status:%d", ret);
	return ret;
}

int aw_dsp_read_f0_q(struct aw_device *aw_dev, int32_t *f0, int32_t *q)
{
	int ret = 0;
	int32_t data[4] = {0};


	ret = aw_dsp_read_dsp_msg(aw_dev, AW_MSG_ID_F0_Q, (char *)data, sizeof(int32_t) * 4);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read f0 & q failed");
		return ret;
	}

	if ((aw_dev->channel % 2) == 0) {
		*f0 = data[0];
		*q  = data[1];
	} else {
		*f0 = data[2];
		*q  = data[3];
	}
	aw_dev_dbg(aw_dev->dev, "read f0 is %d, q is %d", *f0, *q);
	return ret;
}

int aw_dsp_read_f0(struct aw_device *aw_dev, int32_t *f0)
{
	int ret = 0;
	uint32_t msg_id = -EINVAL;


	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_F0_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_F0_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_read_dsp_msg(aw_dev, msg_id, (char *)f0, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "read f0 failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "read f0");
	return ret;
}

int aw_dsp_cali_en(struct aw_device *aw_dev, bool is_enable)
{
	int ret = 0;


	ret = aw_dsp_write_dsp_msg(aw_dev, AW_MSG_ID_ENABLE_CALI, (char *)&is_enable, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "write cali en failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write cali_en[%d]", is_enable);
	return ret;
}

int aw_dsp_hmute_en(struct aw_device *aw_dev, bool is_hmute)
{
	int ret = 0;
	int32_t hmute = is_hmute;


	ret = aw_dsp_write_dsp_msg(aw_dev, AW_MSG_ID_ENABLE_HMUTE, (char *)&hmute, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "write hmue failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write hmute[%d]", is_hmute);
	return ret;
}

int aw_dsp_read_cali_re(struct aw_device *aw_dev, int32_t *cali_re)
{
	int ret = 0;
	uint32_t msg_id = -EINVAL;
	int32_t read_re = 0;

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_RE_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_RE_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_read_dsp_msg(aw_dev, msg_id, (char *)&read_re, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "read cali re failed ");
		return ret;
	}
	*cali_re = AW_DSP_RE_TO_SHOW_RE(read_re);
	aw_dev_dbg(aw_dev->dev, "read cali re done");
	return ret;
}

int aw_dsp_write_cali_re(struct aw_device *aw_dev, int32_t cali_re)
{
	int ret = 0;
	uint32_t msg_id = -EINVAL;
	int32_t local_re = AW_SHOW_RE_TO_DSP_RE(cali_re);


	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_RE_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_RE_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_write_dsp_msg(aw_dev, msg_id, (char *)&local_re, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "write cali re failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write cali re done");
	return ret;
}

int aw_dsp_write_params(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	int ret = 0;
	uint32_t msg_id = -EINVAL;
	int msg_num = -EINVAL;


	ret = aw_get_msg_num(aw_dev->channel, &msg_num);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get msg_num failed");
		return ret;
	}

	if (msg_num == MSG_PARAM_ID_0) {
		msg_id = AW_MSG_ID_PARAMS;
	} else if (msg_num == MSG_PARAM_ID_1) {
		msg_id = AW_MSG_ID_PARAMS_1;
	} else if (msg_num == MSG_PARAM_ID_2) {
		msg_id = AW_MSG_ID_PARAMS_2;
	} else if (msg_num == MSG_PARAM_ID_3) {
		msg_id = AW_MSG_ID_PARAMS_3;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport msg num");
		return ret;
	}

	ret = aw_write_data_to_dsp(msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "write params failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write params done");
	return ret;
}

int aw_dsp_read_vmax(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	int ret = 0;
	uint32_t msg_id = -EINVAL;


	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_VMAX_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_VMAX_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_read_dsp_msg(aw_dev, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read vmax failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "read vmax done");
	return ret;
}

int aw_dsp_write_vmax(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	int ret = 0;
	uint32_t msg_id = -EINVAL;

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_VMAX_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_VMAX_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_write_dsp_msg(aw_dev, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "write vmax failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write vmax done");
	return ret;
}

int aw_dsp_write_ambient_temperature(struct aw_device *aw_dev, int *temp, int smooth_en)
{
	uint32_t msg_id;
	int ret;
	int temperature = 0;
	int temp_value[2] = { 0 };
	uint32_t data_len = sizeof(temp_value);

	temperature = *temp;

	msg_id = AW_MSG_ID_AMBIENT_TEMP_L;
	temp_value[0] = smooth_en;
	temp_value[1] = (temperature << AW_AMBIENT_TEMP_ACCURACY) / 10;

	ret = aw_dsp_write_dsp_msg(aw_dev, msg_id, (char *)&temp_value[0], data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "write ambient temp failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write ambient temp [%d] with smooth_en[%d] done", temp_value[1], temp_value[0]);
	return 0;

}

static int32_t aw_data_around(int64_t data_value, int32_t data_width)
{
	int32_t temp_value = 0;
	int32_t ret_value = 0;

	temp_value = (data_value * 10) >> data_width;
	if (temp_value % 10 > 4)
		ret_value = temp_value / 10 + 1;
	else
		ret_value = temp_value / 10;

	return ret_value;
}

int aw_dsp_read_predict_temperature(unsigned char dev_index, int *temperature)
{
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int32_t temp_data[PREDICTED_TEMP_NUM] = { 0 };
	int32_t chan_temperature = 0;
	int32_t temp_max = 0;
	int32_t temp_num = 0;
	uint32_t msg_id = 0;
	int ret = 0;
	int i = 0;

	if (temperature == NULL) {
		aw_pr_err("temperature is NULL");
		return -EINVAL;
	}

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	if (dev_index == AW_DEV_CH_PRI_L) {
		msg_id = AW_MSG_ID_PREDICTED_TEMP_L;
	} else if (dev_index == AW_DEV_CH_PRI_R) {
		msg_id = AW_MSG_ID_PREDICTED_TEMP_R;
	} else {
		aw_pr_info("unsurpported dev_index:%d", dev_index);
		return -EINVAL;
	}

	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel == dev_index) {
			ret = aw_dsp_read_dsp_msg(local_dev, msg_id, (char *)&(temp_data[0]), sizeof(temp_data));
			if (ret) {
				aw_dev_err(local_dev->dev, "read temperature failed ");
				return ret;
			}
			temp_num = ABS_VALUE(temp_data[0]);
			if(temp_num > 0 && temp_num < PREDICTED_TEMP_NUM) {
				for (i = 1; i <= temp_num; i++) {
				if(temp_data[i] > temp_max) {
					temp_max = temp_data[i];
					}
				}
			} else {
				aw_pr_err("read chan[%d] temperature invalid", dev_index);
				return -EINVAL;
			}
			chan_temperature = aw_data_around((int64_t)temp_max, 20);
			*temperature = chan_temperature;

			aw_pr_info("read chan[%d] temperature:%d done", dev_index, *temperature);
			return 0;
		}
	}

	aw_pr_err(" unsupport [%d]", dev_index);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(aw_dsp_read_predict_temperature);

int aw_dsp_write_surface_temperature(unsigned char dev_index, int temperature)
{
	int temp_value[2] = { 0 };
	int chan_temperature = 0;
	uint32_t data_len = sizeof(temp_value);
	uint32_t msg_id = 0;
	int ret = -1;
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;

	chan_temperature = temperature;

	msg_id = AW_MSG_ID_SURFACE_TEMP;
	temp_value[0] = chan_temperature;
	temp_value[1] = chan_temperature;

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel == dev_index) {
			ret = aw_write_msg_to_dsp_v_1_0_0_0(local_dev, msg_id, (char *)&temp_value[0],
							data_len, AW_DSP_CHANNEL_DEFAULT_NUM);
			if (ret) {
				aw_pr_err("write ambient temp failed ");
				return ret;
			}

			aw_pr_info("write chan[%d] temperature:%d done", dev_index, temp_value[0]);
			return 0;
		}
	}

	aw_pr_err(" unsupport [%d]", dev_index);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(aw_dsp_write_surface_temperature);

int aw_dsp_read_surface_temperature(unsigned char dev_index, int *temperature)
{
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int32_t temp_data[2] = { 0 };
	int32_t chan_temperature = 0;
	uint32_t msg_id = 0;
	int ret = 0;

	if (temperature == NULL) {
		aw_pr_err("temperature is NULL");
		return -EINVAL;
	}

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	msg_id = AW_MSG_ID_SURFACE_TEMP;

	list_for_each (pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel == dev_index) {
			ret = aw_read_msg_from_dsp_v_1_0_0_0(local_dev, msg_id, (char *)&(temp_data[0]),
						sizeof(temp_data), AW_DSP_CHANNEL_DEFAULT_NUM);
			if (ret) {
				aw_dev_err(local_dev->dev, "read temperature failed ");
				return ret;
			}

			if (dev_index == AW_DEV_CH_PRI_L) {
				chan_temperature = temp_data[0];
			} else if (dev_index == AW_DEV_CH_PRI_R) {
				chan_temperature = temp_data[1];
			} else {
				aw_pr_info("unsurpported dev_index:%d", dev_index);
				return -EINVAL;
			}

			*temperature = chan_temperature;

			aw_pr_info("read chan[%d] temperature:%d done", dev_index, *temperature);
			return 0;
		}
	}

	aw_pr_err(" unsupport [%d]", dev_index);
	return -EINVAL;
}

int aw_dsp_write_battery_capacity(struct aw_device *aw_dev, int capacity, int smooth_en)
{
	uint32_t msg_id = 0;
	int ret = -1;
	int set_value[2] = { 0 };
	uint32_t data_len = sizeof(set_value);
	msg_id = AW_MSG_ID_BATTERY_CAPACITY;
	set_value[0] = capacity;
	set_value[1] = smooth_en;
	ret = aw_write_msg_to_dsp_v_1_0_0_0(aw_dev, msg_id, (char *)&set_value,
							data_len, AW_DSP_CHANNEL_DEFAULT_NUM);
	if (ret) {
		aw_dev_err(aw_dev->dev, "write battery capacity failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write capacit [%d] with smooth_en[%d] done", capacity, smooth_en);
	return 0;
}
EXPORT_SYMBOL_GPL(aw_dsp_write_battery_capacity);
int aw_dsp_noise_en(struct aw_device *aw_dev, bool is_noise)
{
	int ret = 0;
	int32_t noise = is_noise;
	uint32_t msg_id = -EINVAL;

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_NOISE_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_NOISE_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_write_dsp_msg(aw_dev, msg_id, (char *)&noise, sizeof(int32_t));
	if (ret) {
		aw_dev_err(aw_dev->dev, "write noise failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "write noise[%d] done", noise);
	return ret;

}

int aw_dsp_read_cali_cfg(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	int ret = 0;
	uint32_t msg_id = -EINVAL;


	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_CALI_CFG_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_CALI_CFG_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_read_dsp_msg(aw_dev, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "read cali_cfg failed ");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "read cali_cfg done");
	return ret;
}

int aw_dsp_write_cali_cfg(struct aw_device *aw_dev, char *data, unsigned int data_len)
{
	int ret = 0;
	uint32_t msg_id = -EINVAL;

	if (aw_dev->channel == AW_DEV_CH_PRI_L ||
			aw_dev->channel == AW_DEV_CH_SEC_L ||
				aw_dev->channel == AW_DEV_CH_TERT_L ||
					aw_dev->channel == AW_DEV_CH_QUAT_L) {
		msg_id = AW_MSG_ID_CALI_CFG_L;
	} else if (aw_dev->channel == AW_DEV_CH_PRI_R ||
			aw_dev->channel == AW_DEV_CH_SEC_R ||
				aw_dev->channel == AW_DEV_CH_TERT_R ||
					aw_dev->channel == AW_DEV_CH_QUAT_R) {
		msg_id = AW_MSG_ID_CALI_CFG_R;
	} else {
		aw_dev_err(aw_dev->dev, "unsupport dev channel");
		return -EINVAL;
	}

	ret = aw_dsp_write_dsp_msg(aw_dev, msg_id, data, data_len);
	if (ret) {
		aw_dev_err(aw_dev->dev, "write cali_cfg failed ");
		return ret;
	}

	aw_dev_dbg(aw_dev->dev, "write cali_cfg done");
	return ret;
}


int aw_dsp_set_copp_module_en(bool enable)
{
	int ret = 0;

	ret = aw_adm_param_enable(g_rx_port_id, AW_COPP_MODULE_ID,
			AW_COPP_PARAMS_ID_AWDSP_ENABLE, enable);
	if (ret)
		return -EINVAL;

	aw_pr_info("set skt %s", enable == 1 ? "enable" : "disable");
	return ret;
}

int aw_get_algo_version(struct aw_device *aw_dev, char *algo_ver_buf)
{
	int ret = 0;
	unsigned int algo_ver = 0;
	char *algo_data = NULL;

	ret = aw_dsp_read_dsp_msg(aw_dev, AW_MSG_ID_VERSION,
				(char *)&algo_ver, sizeof(uint32_t));
	if ((ret < 0) || (algo_ver == 0)) {
		ret = aw_dsp_read_dsp_msg(aw_dev, AW_MSG_ID_VERSION_NEW,
					algo_ver_buf, ALGO_VERSION_MAX);
		if (ret < 0)
			return ret;
	} else {
		algo_data = (char *)&algo_ver;
		snprintf(algo_ver_buf, ALGO_VERSION_MAX, "aw_algo_v%d.%d.%d.%d",
				algo_data[3], algo_data[2], algo_data[1], algo_data[0]);
	}

	aw_dev_dbg(aw_dev->dev, "%s", algo_ver_buf);
	return ret;

}

void aw_device_parse_topo_id_dt(struct aw_device *aw_dev)
{
	int ret = 0;

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-tx-topo-id", &g_tx_topo_id);
	if (ret < 0) {
		g_tx_topo_id = AW_TX_DEFAULT_TOPO_ID;
		aw_dev_info(aw_dev->dev, "read aw-tx-topo-id failed,use default");
	}

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-rx-topo-id", &g_rx_topo_id);
	if (ret < 0) {
		g_rx_topo_id = AW_RX_DEFAULT_TOPO_ID;
		aw_dev_info(aw_dev->dev, "read aw-rx-topo-id failed,use default");
	}

	aw_dev_info(aw_dev->dev, "tx-topo-id: 0x%x, rx-topo-id: 0x%x",
						g_tx_topo_id, g_rx_topo_id);
}

void aw_device_parse_port_id_dt(struct aw_device *aw_dev)
{
	int ret = 0;

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-tx-port-id", &g_tx_port_id);
	if (ret < 0) {
		g_tx_port_id = AW_TX_DEFAULT_PORT_ID;
		aw_dev_info(aw_dev->dev, "read aw-tx-port-id failed,use default");
	}

	ret = of_property_read_u32(aw_dev->dev->of_node, "aw-rx-port-id", &g_rx_port_id);
	if (ret < 0) {
		g_rx_port_id = AW_RX_DEFAULT_PORT_ID;
		aw_dev_info(aw_dev->dev, "read aw-rx-port-id failed,use default");
	}

	aw_set_port_id(g_tx_port_id, g_rx_port_id);
	aw_dev_info(aw_dev->dev, "tx-port-id: 0x%x, rx-port-id: 0x%x",
						g_tx_port_id, g_rx_port_id);

}

