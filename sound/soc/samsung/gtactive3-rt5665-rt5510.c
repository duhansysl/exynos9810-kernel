/*
 *  Exynos9810-RT5665 Audio Machine driver.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/input-event-codes.h>
#include <linux/debugfs.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/mfd/madera/core.h>
#include <linux/extcon/extcon-madera.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <sound/jack.h>
#include <sound/samsung/sec_audio_debug.h>

#include <soc/samsung/exynos-pmu.h>
#include <sound/samsung/abox.h>
#include "../codecs/rt5665.h"
#include "jack_rt5665_sysfs_cb.h"

#if defined(CONFIG_SND_SOC_DBMDX)
#include <sound/dbmdx-export.h>
#endif

#define EXYNOS_PMU_PMU_DEBUG_OFFSET		(0x0A00)
#define RT5665_DAI_OFFSET			(16)

struct clk_conf {
	int id;
	const char *name;
	int source;
	int rate;

	bool valid;
};

#define CODEC_SAMPLE_RATE_48KHZ		48000
#define CODEC_PLL_48KHZ				24576000
#define CODEC_SAMPLE_RATE_192KHZ	192000
#define CODEC_PLL_192KHZ			49152000

struct rt5665_drvdata {
	struct device *dev;
	struct snd_soc_codec *codec;
	struct snd_soc_jack rt5665_headset;
	int aifrate;
	bool use_external_jd;

	int abox_vss_state;
	unsigned int pcm_state;
	struct wake_lock wake_lock;
	unsigned int wake_lock_switch;

	int uaifp_count;
	int uaifc_count;
};

static struct snd_soc_card exynos9810_audio;
static struct rt5665_drvdata exynos9810_drvdata;

static const struct snd_soc_ops rdma_ops = {
};

static const struct snd_soc_ops wdma_ops = {
};

static int exynos9810_rt5665_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret, codec_pll_in, codec_pll_out;
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	drvdata->pcm_state |= (1 << dai_link->id);

	if ((drvdata->uaifp_count == 0) && (drvdata->uaifc_count == 0)) {
		dev_info(card->dev, "%s: BCLK Enable\n", __func__);
		snd_soc_dai_set_tristate(rtd->cpu_dai, 0);
	}

	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			&& (drvdata->uaifp_count == 0)) {
		drvdata->uaifp_count++;
	} else if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			&& (drvdata->uaifc_count == 0)) {
		drvdata->uaifc_count++;
	} else {
		dev_err(card->dev, "%s: cnt check stream %d, uaifp_cnt %d, uaifc_cnt %d\n",
			__func__, substream->stream, drvdata->uaifp_count, drvdata->uaifc_count);
	}
	dev_info(card->dev, "%s: uaifp_cnt %d, uaifc_cnt %d\n", __func__,
			drvdata->uaifp_count, drvdata->uaifc_count);

	dev_info(card->dev, "%s: %s-%d %dch, %dHz, %dbit, %dbytes (pcm_state: 0x%07x)\n",
			__func__, rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params),
			params_width(params), params_buffer_bytes(params),
			drvdata->pcm_state);

	if (params_rate(params) == CODEC_SAMPLE_RATE_192KHZ)
		codec_pll_in = params_rate(params) * params_width(params) * 2;
	else
		codec_pll_in = params_rate(params) * params_width(params) * 2;
	codec_pll_out = params_rate(params) * 512;

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5665_PLL1_S_BCLK1, codec_pll_in, codec_pll_out);
	if (ret < 0) {
		dev_err(card->dev, "codec_dai RT5665_PLL1_S_BCLK1 not set\n");
		return ret;
	}
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5665_SCLK_S_PLL1, codec_pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "codec_dai RT5665_PLL1_S_PLL1 not set\n");
		return ret;
	}

	return ret;
}

static int exynos9810_rt5665_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	drvdata->pcm_state &= (~(1 << dai_link->id));

	dev_info(card->dev, "%s: %s-%d (pcm_state: 0x%07x)\n",
			__func__, rtd->dai_link->name, substream->stream,
			drvdata->pcm_state);

	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			&& (drvdata->uaifp_count > 0)) {
		drvdata->uaifp_count--;
	} else if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			&& (drvdata->uaifc_count > 0)) {
		drvdata->uaifc_count--;
	} else {
		dev_err(card->dev, "%s: cnt check stream %d, uaifp_cnt %d, uaifc_cnt %d\n",
			__func__, substream->stream, drvdata->uaifp_count, drvdata->uaifc_count);
	}

	if ((drvdata->uaifp_count == 0) && (drvdata->uaifc_count == 0)) {
		dev_info(card->dev, "%s: BCLK Disable\n", __func__);
		snd_soc_dai_set_tristate(rtd->cpu_dai, 1);
	}

	dev_info(card->dev, "%s: uaifp_cnt %d, uaifc_cnt %d\n", __func__,
			drvdata->uaifp_count, drvdata->uaifc_count);

	return 0;
}

static int exynos9810_rt5665_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct rt5665_drvdata *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_jack *jack;

	dev_info(codec->dev, "%s\n", __func__);

	ret = snd_soc_card_jack_new(&exynos9810_audio, "Headset Jack",
								SND_JACK_HEADSET, &ctx->rt5665_headset, NULL, 0);
	if (ret) {
		dev_err(rtd->dev, "Headset Jack creation failed %d\n", ret);
		return ret;
	}

	jack = &ctx->rt5665_headset;
	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_MEDIA);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);
	rt5665_set_jack_detect(codec, &ctx->rt5665_headset);

	return ret;
}

static int exynos9810_rt5510_amp_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	dev_info(codec->dev, "%s\n", __func__);

	snd_soc_dapm_ignore_suspend(dapm, "aif_playback");
	snd_soc_dapm_ignore_suspend(dapm, "aif_capture");
	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_ops uaif0_ops = {
	.hw_params = exynos9810_rt5665_hw_params,
	.hw_free = exynos9810_rt5665_hw_free,
};

static const struct snd_soc_ops uaif_ops = {
};

static int dsif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx_slot[] = {0, 1};

	/* bclk ratio 64 for DSD64, 128 for DSD128 */
	snd_soc_dai_set_bclk_ratio(cpu_dai, 64);

	/* channel map 0 1 if left is first, 1 0 if right is first */
	snd_soc_dai_set_channel_map(cpu_dai, 2, tx_slot, 0, NULL);
	return 0;
}

static const struct snd_soc_ops dsif_ops = {
	.hw_params = dsif_hw_params,
};

static int exynos9810_late_probe(struct snd_soc_card *card)
{
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *aif1_dai;
	struct snd_soc_codec *codec;
	struct snd_soc_component *cpu;

	rtd = snd_soc_get_pcm_runtime(card, card->dai_link[0].name);
	aif1_dai = rtd->cpu_dai;
	cpu = aif1_dai->component;

	rtd = snd_soc_get_pcm_runtime(card, card->dai_link[RT5665_DAI_OFFSET].name);
	aif1_dai = rtd->codec_dai;
	codec = aif1_dai->codec;
	drvdata->codec = codec;

	snd_soc_dapm_ignore_suspend(&card->dapm, "VOUTPUT");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUT1");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUT2");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VOUTPUTCALL");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUTCALL");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADSETMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "RECEIVER");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPEAKER");
	snd_soc_dapm_ignore_suspend(&card->dapm, "BLUETOOTH MIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "BLUETOOTH SPK");
	snd_soc_dapm_ignore_suspend(&card->dapm, "MAINMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SUBMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADSETMIC");
#if defined(CONFIG_SND_SOC_SAMSUNG_VTS)
	snd_soc_dapm_ignore_suspend(&card->dapm, "VTS Virtual Output");
#endif
	snd_soc_dapm_ignore_suspend(&card->dapm, "FM");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF1_1 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF3 Capture");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));

	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA0 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA1 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA2 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA3 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA4 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA5 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA6 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA7 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA0 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA1 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA2 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA3 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA4 Capture");
	snd_soc_dapm_sync(snd_soc_component_get_dapm(cpu));

#if defined(CONFIG_SND_SOC_DBMDX)
	dbmdx_remote_add_codec_controls(codec);
#endif

	register_rt5665_jack_cb(codec);

	return 0;
}

static struct snd_soc_dai_link exynos9810_dai[] = {
	{
		.name = "RDMA0",
		.stream_name = "RDMA0",
		.cpu_dai_name = "RDMA0",
		.platform_name = "17c51000.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA1",
		.stream_name = "RDMA1",
		.cpu_dai_name = "RDMA1",
		.platform_name = "17c51100.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA2",
		.stream_name = "RDMA2",
		.cpu_dai_name = "RDMA2",
		.platform_name = "17c51200.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA3",
		.stream_name = "RDMA3",
		.cpu_dai_name = "RDMA3",
		.platform_name = "17c51300.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA4",
		.stream_name = "RDMA4",
		.cpu_dai_name = "RDMA4",
		.platform_name = "17c51400.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA5",
		.stream_name = "RDMA5",
		.cpu_dai_name = "RDMA5",
		.platform_name = "17c51500.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA6",
		.stream_name = "RDMA6",
		.cpu_dai_name = "RDMA6",
		.platform_name = "17c51600.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA7",
		.stream_name = "RDMA7",
		.cpu_dai_name = "RDMA7",
		.platform_name = "17c51700.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "WDMA0",
		.stream_name = "WDMA0",
		.cpu_dai_name = "WDMA0",
		.platform_name = "17c52000.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA1",
		.stream_name = "WDMA1",
		.cpu_dai_name = "WDMA1",
		.platform_name = "17c52100.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA2",
		.stream_name = "WDMA2",
		.cpu_dai_name = "WDMA2",
		.platform_name = "17c52200.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA3",
		.stream_name = "WDMA3",
		.cpu_dai_name = "WDMA3",
		.platform_name = "17c52300.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA4",
		.stream_name = "WDMA4",
		.cpu_dai_name = "WDMA4",
		.platform_name = "17c52400.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
#if defined(CONFIG_SND_SOC_SAMSUNG_VTS)
	{
		.name = "VTS-Trigger",
		.stream_name = "VTS-Trigger",
		.cpu_dai_name = "vts-tri",
		.platform_name = "13810000.vts:vts_dma@0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.capture_only = true,
	},
	{
		.name = "VTS-Record",
		.stream_name = "VTS-Record",
		.cpu_dai_name = "vts-rec",
		.platform_name = "13810000.vts:vts_dma@1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.capture_only = true,
	},
#endif
	{
		.name = "DP Audio",
		.stream_name = "DP Audio",
		.cpu_dai_name = "audio_cpu_dummy",
		.platform_name = "dp_dma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "UAIF0",
		.stream_name = "UAIF0",
		.cpu_dai_name = "UAIF0",
		.platform_name = "snd-soc-dummy",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif0_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = exynos9810_rt5665_codec_init,
	},
	{
		.name = "UAIF1",
		.stream_name = "UAIF1",
		.cpu_dai_name = "UAIF1",
		.platform_name = "snd-soc-dummy",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = exynos9810_rt5510_amp_init,
	},
	{
		.name = "UAIF2",
		.stream_name = "UAIF2",
		.cpu_dai_name = "UAIF2",
		.platform_name = "snd-soc-dummy",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF3",
		.stream_name = "UAIF3",
		.cpu_dai_name = "UAIF3",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "DSIF",
		.stream_name = "DSIF",
		.cpu_dai_name = "DSIF",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dai_fmt = SND_SOC_DAIFMT_PDM | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &dsif_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "SIFS0",
		.stream_name = "SIFS0",
		.cpu_dai_name = "SIFS0",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS1",
		.stream_name = "SIFS1",
		.cpu_dai_name = "SIFS1",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS2",
		.stream_name = "SIFS2",
		.cpu_dai_name = "SIFS2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
};

static int gtactive3_headsetmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct rt5665_drvdata *drvdata = &exynos9810_drvdata;
	struct snd_soc_codec *codec = drvdata->codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);

	dev_info(card->dev, "%s ev: %d, jack_type %d\n", __func__, event, rt5665->jack_type);

	return 0;
}

static int gtactive3_mainmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int gtactive3_submic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int gtactive3_fm(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int gtactive3_receiver(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int gtactive3_headphone(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct rt5665_drvdata *drvdata = &exynos9810_drvdata;
	struct snd_soc_codec *codec = drvdata->codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);

	dev_info(card->dev, "%s ev: %d, jack_type %d\n", __func__, event, rt5665->jack_type);

	return 0;
}

static int gtactive3_speaker(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}
#if defined(CONFIG_SND_SOC_SAMSUNG_VTS)
static const char * const vts_output_texts[] = {
	"None",
	"DMIC1",
};

static const struct soc_enum vts_output_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(vts_output_texts),
			vts_output_texts);

static const struct snd_kcontrol_new vts_output_mux[] = {
	SOC_DAPM_ENUM("VTS Virtual Output Mux", vts_output_enum),
};
#endif
static const struct snd_kcontrol_new exynos9810_controls[] = {
	SOC_DAPM_PIN_SWITCH("HEADPHONE"),
	SOC_DAPM_PIN_SWITCH("SPEAKER"),
	SOC_DAPM_PIN_SWITCH("RECEIVER"),
	SOC_DAPM_PIN_SWITCH("MAINMIC"),
	SOC_DAPM_PIN_SWITCH("SUBMIC"),
	SOC_DAPM_PIN_SWITCH("HEADSETMIC"),
	SOC_DAPM_PIN_SWITCH("FM"),
};

static struct snd_soc_dapm_widget exynos9810_widgets[] = {

	SND_SOC_DAPM_OUTPUT("VOUTPUT"),
	SND_SOC_DAPM_INPUT("VINPUT1"),
	SND_SOC_DAPM_INPUT("VINPUT2"),
	SND_SOC_DAPM_OUTPUT("VOUTPUTCALL"),
	SND_SOC_DAPM_INPUT("VINPUTCALL"),
	SND_SOC_DAPM_MIC("HEADSETMIC", gtactive3_headsetmic),
	SND_SOC_DAPM_MIC("MAINMIC", gtactive3_mainmic),
	SND_SOC_DAPM_MIC("SUBMIC", gtactive3_submic),
	SND_SOC_DAPM_MIC("FM", gtactive3_fm),
	SND_SOC_DAPM_SPK("RECEIVER", gtactive3_receiver),
	SND_SOC_DAPM_HP("HEADPHONE", gtactive3_headphone),
	SND_SOC_DAPM_SPK("SPEAKER", gtactive3_speaker),
	SND_SOC_DAPM_MIC("BLUETOOTH MIC", NULL),
	SND_SOC_DAPM_SPK("BLUETOOTH SPK", NULL),
#if defined(CONFIG_SND_SOC_SAMSUNG_VTS)
	SND_SOC_DAPM_OUTPUT("VTS Virtual Output"),
	SND_SOC_DAPM_MUX("VTS Virtual Output Mux", SND_SOC_NOPM, 0, 0,
			&vts_output_mux[0]),
#endif
};

static struct snd_soc_codec_conf codec_conf[] = {
	{.name_prefix = "ABOX", },
	{.name_prefix = "ABOX", },
	{.name_prefix = "ABOX", },
	{.name_prefix = "ABOX", },
	{.name_prefix = "ABOX", },
	{.name_prefix = "ABOX", },
#if defined(CONFIG_SND_SOC_SAMSUNG_VTS)
	{.name_prefix = "VTS", },
#endif
};

static struct snd_soc_aux_dev aux_dev[] = {
	{
		.name = "EFFECT",
	},
};

static struct snd_soc_card exynos9810_audio = {
	.name = "Exynos9810-Audio",
	.owner = THIS_MODULE,
	.dai_link = exynos9810_dai,
	.num_links = ARRAY_SIZE(exynos9810_dai),

	.late_probe = exynos9810_late_probe,

	.controls = exynos9810_controls,
	.num_controls = ARRAY_SIZE(exynos9810_controls),
	.dapm_widgets = exynos9810_widgets,
	.num_dapm_widgets = ARRAY_SIZE(exynos9810_widgets),

	.drvdata = (void *)&exynos9810_drvdata,

	.codec_conf = codec_conf,
	.num_configs = ARRAY_SIZE(codec_conf),

	.aux_dev = aux_dev,
	.num_aux_devs = ARRAY_SIZE(aux_dev),
};

static int read_dai(struct device_node *np, const char * const prop,
			      struct device_node **dai, const char **name)
{
	int ret = 0;

	np = of_get_child_by_name(np, prop);
	if (!np)
		return -ENOENT;

	*dai = of_parse_phandle(np, "sound-dai", 0);
	if (!*dai) {
		ret = -ENODEV;
		goto out;
	}

	if (*name == NULL) {
		/* Ignoring the return as we don't register DAIs to the platform */
		ret = snd_soc_of_get_dai_name(np, name);
		if (ret && !*name)
			return ret;
	}

out:
	of_node_put(np);

	return ret;
}

static struct clk *xclkout;

static void control_xclkout(bool on)
{
	if (on) {
		clk_prepare_enable(xclkout);
	} else {
		clk_disable_unprepare(xclkout);
	}
}

static void exynos9810_mic_always_on(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct snd_soc_codec *codec = drvdata->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	dev_info(&pdev->dev, "%s\n", __func__);

	if (of_find_property(pdev->dev.of_node, "mic-always-on", NULL) != NULL) {
		snd_soc_dapm_force_enable_pin(dapm, "MICBIAS2");
		snd_soc_dapm_sync(dapm);
	} else {
		dev_info(&pdev->dev, "Invalid mic_always_on\n");
		return;
	}

	dev_info(&pdev->dev, "Success to enable mic-always-on\n");
}

static int exynos9810_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &exynos9810_audio;
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dai;
	int nlink = 0, n;
	int ret;

	card->dev = &pdev->dev;
	drvdata->dev = card->dev;
	snd_soc_card_set_drvdata(card, drvdata);

	xclkout = devm_clk_get(&pdev->dev, "xclkout");
	if (IS_ERR(xclkout)) {
		dev_err(&pdev->dev, "xclkout get failed\n");
		xclkout = NULL;
	}
	control_xclkout(true);
	dev_info(&pdev->dev, "xclkout is enabled\n");

	for_each_child_of_node(np, dai) {
		if (!exynos9810_dai[nlink].name)
			exynos9810_dai[nlink].name = dai->name;
		if (!exynos9810_dai[nlink].stream_name)
			exynos9810_dai[nlink].stream_name = dai->name;

		ret = read_dai(dai, "cpu",
			&exynos9810_dai[nlink].cpu_of_node,
			&exynos9810_dai[nlink].cpu_dai_name);
		if (ret) {
			dev_err(card->dev,
				"Failed to parse cpu DAI for %s: %d\n",
				dai->name, ret);
			return ret;
		}

		if (!exynos9810_dai[nlink].platform_name) {
			ret = read_dai(dai, "platform",
				&exynos9810_dai[nlink].platform_of_node,
				&exynos9810_dai[nlink].platform_name);
			if (ret) {
				exynos9810_dai[nlink].platform_of_node =
					exynos9810_dai[nlink].cpu_of_node;
				dev_info(card->dev,
					"Cpu node is used as platform for %s: %d\n",
					dai->name, ret);
			}
		}

		if (!exynos9810_dai[nlink].codec_name) {
			ret = read_dai(dai, "codec",
				&exynos9810_dai[nlink].codec_of_node,
				&exynos9810_dai[nlink].codec_dai_name);
			if (ret) {
				dev_err(card->dev,
					"Failed to parse codec DAI for %s: %d\n",
					dai->name, ret);
				return ret;
			}
		}

		if (++nlink == card->num_links)
			break;
	}

	if (!nlink) {
		dev_warn(card->dev, "No DAIs specified\n");
	}

	if (of_property_read_bool(np, "samsung,routing")) {
		ret = snd_soc_of_parse_audio_routing(card, "samsung,routing");
		if (ret)
			return ret;
	}

	for (n = 0; n < ARRAY_SIZE(codec_conf); n++) {
		codec_conf[n].of_node = of_parse_phandle(np, "samsung,codec", n);

		if (!codec_conf[n].of_node) {
			dev_err(&pdev->dev,
				"Property 'samsung,codec' missing\n");
			return -EINVAL;
		}
	}

	for (n = 0; n < ARRAY_SIZE(aux_dev); n++) {
		aux_dev[n].codec_of_node = of_parse_phandle(np, "samsung,aux", n);

		if (!aux_dev[n].codec_of_node) {
			dev_err(&pdev->dev,
				"Property 'samsung,aux' missing\n");
			return -EINVAL;
		}
	}

	ret = devm_snd_soc_register_card(card->dev, card);
	if (ret)
		dev_err(card->dev, "snd_soc_register_card() failed:%d\n", ret);
	else {
		exynos9810_mic_always_on(pdev);
		dev_info(card->dev, "%s done\n", __func__);
	}

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos9810_of_match[] = {
	{ .compatible = "samsung,exynos9810-audio", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos9810_of_match);
#endif /* CONFIG_OF */

static struct platform_driver exynos9810_audio_driver = {
	.driver		= {
		.name	= "exynos9810-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(exynos9810_of_match),
	},

	.probe		= exynos9810_audio_probe,
};

module_platform_driver(exynos9810_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Exynos9810 Audio Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:exynos9810-audio");
