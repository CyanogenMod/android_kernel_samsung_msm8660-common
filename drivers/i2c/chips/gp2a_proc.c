#include <linux/err.h>
//#include <linux/m_adc.h>
#include <linux/m_adcproc.h>
#include <linux/pmic8058-xoadc.h>

u32 lightsensor_get_adc(int channel)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;
#ifdef CONFIG_SEC_DEBUG_PM8058_ADC
	static int retry_cnt;
#endif
	pr_debug("%s: called for %d\n", __func__, channel);
	ret = adc_channel_open(channel, &h);
	if (ret) {
		pr_err("%s: couldnt open channel %d ret=%d\n",
					__func__, channel, ret);
		goto out;
	}
	init_completion(&conv_complete_evt);
	ret = adc_channel_request_conv(h, &conv_complete_evt);
	if (ret) {
		pr_err("%s: couldnt request conv channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = wait_for_completion_timeout(&conv_complete_evt, 10*HZ);
	if (!ret) {
		pr_err("%s: wait interrupted channel %d ret=%d\n",
						__func__, channel, ret);
#ifdef CONFIG_SEC_DEBUG_PM8058_ADC_VERBOSE
		adc_dbg_info_timer(0);
#else
		pm8058_xoadc_clear_recentQ(h);
		adc_channel_close(h);
#ifdef CONFIG_SEC_DEBUG_PM8058_ADC
		if(retry_cnt++ >= 10)
			adc_dbg_info_timer(0);
#endif
#endif
		goto out;
	}
#ifdef CONFIG_SEC_DEBUG_PM8058_ADC
	retry_cnt = 0;
#endif
	ret = adc_channel_read_result(h, &adc_chan_result);
	if (ret) {
		pr_err("%s: couldnt read result channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_close(h);
	if (ret) {
		pr_err("%s: couldnt close channel %d ret=%d\n",
						__func__, channel, ret);
	}

	pr_debug("%s: done for %d\n", __func__, channel);
//	return adc_chan_result.physical;
	return adc_chan_result.measurement;

out:
	pr_debug("%s: done for %d\n", __func__, channel);
	return -EINVAL;

}

