/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_CSIPHY_H
#define MSM_CSIPHY_H

#include <linux/clk.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>

struct csiphy_device {
	struct platform_device *pdev;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;
	struct mutex mutex;

	struct clk *csiphy_clk[2];
};

struct csiphy_cfg_params {
	struct v4l2_subdev *subdev;
	void *parms;
};

#define VIDIOC_MSM_CSIPHY_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 7, struct csiphy_cfg_params)

#define VIDIOC_MSM_CSIPHY_INIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct v4l2_subdev*)

#define VIDIOC_MSM_CSIPHY_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, struct v4l2_subdev*)

#endif
