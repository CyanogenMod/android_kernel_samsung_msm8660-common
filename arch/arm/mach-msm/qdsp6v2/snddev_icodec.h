/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MACH_QDSP6V2_SNDDEV_ICODEC_H
#define __MACH_QDSP6V2_SNDDEV_ICODEC_H
#include <linux/mfd/msm-adie-codec.h>
#include <mach/qdsp5v2/audio_def.h>
#include <mach/pmic.h>

struct snddev_icodec_data {
	u32 capability; /* RX or TX */
	const char *name;
	u32 copp_id; /* audpp routing */
	/* Adie profile */
	struct adie_codec_dev_profile *profile;
	/* Afe setting */
	u8 channel_mode;
	u32 default_sample_rate;
	int (*pamp_on) (void);
	void (*pamp_off) (void);
	int (*voltage_on) (void);
	void (*voltage_off) (void);
#ifdef CONFIG_VP_A2220
	int (*a2220_vp_on) (void);
	void (*a2220_vp_off) (void);
#endif
	u32 dev_vol_type;
};

#endif
