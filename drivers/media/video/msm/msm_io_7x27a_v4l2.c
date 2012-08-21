/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pm_qos_params.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/camera.h>
#include <mach/clk.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>


#define MSM_AXI_QOS_PREVIEW 200000
#define MSM_AXI_QOS_SNAPSHOT 200000
#define MSM_AXI_QOS_RECORDING 200000

static struct clk *camio_cam_clk;
static struct resource *clk_ctrl_mem;
static struct msm_camera_io_clk camio_clk;
void __iomem *appbase;

void msm_io_w(u32 data, void __iomem *addr)
{
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	writel_relaxed((data), (addr));
}

u32 msm_io_r(void __iomem *addr)
{
	uint32_t data = readl_relaxed(addr);
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	return data;
}

void msm_camio_clk_rate_set_2(struct clk *clk, int rate)
{
	clk_set_rate(clk, rate);
}
int msm_camio_clk_enable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_CAM_MCLK_CLK:
		clk = clk_get(NULL, "cam_m_clk");
		camio_cam_clk = clk;
		msm_camio_clk_rate_set_2(clk, camio_clk.mclk_clk_rate);
		break;
	default:
		break;
	}

	if (!IS_ERR(clk))
		clk_enable(clk);
	else
		rc = -1;
	return rc;
}

int msm_camio_clk_disable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_CAM_MCLK_CLK:
		clk = camio_cam_clk;
		break;
	default:
		break;
	}

	if (!IS_ERR(clk)) {
		clk_disable(clk);
		clk_put(clk);
	} else
		rc = -1;
	return rc;
}

void msm_camio_clk_rate_set(int rate)
{
	struct clk *clk = camio_cam_clk;
	clk_set_rate(clk, rate);
}

int msm_sensor_probe_on(struct device *dev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = dev->platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_clk = camdev->ioclk;

	rc = camdev->camera_gpio_on();
	if (rc < 0)
		return rc;

	rc = msm_camio_clk_enable(CAMIO_CAM_MCLK_CLK);
	if (rc < 0)
		camdev->camera_gpio_off();

	return rc;
}

int msm_sensor_probe_off(struct device *dev)
{
	struct msm_camera_sensor_info *sinfo = dev->platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;

	camdev->camera_gpio_off();
	return msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
}

void msm_camio_vfe_blk_reset(void)
{
	uint32_t val;

	/* do apps reset */
	val = readl_relaxed(appbase + 0x00000210);
	val |= 0x1;
	writel_relaxed(val, appbase + 0x00000210);
	usleep_range(10000, 11000);

	val = readl_relaxed(appbase + 0x00000210);
	val &= ~0x1;
	writel_relaxed(val, appbase + 0x00000210);
	usleep_range(10000, 11000);

	/* do axi reset */
	val = readl_relaxed(appbase + 0x00000208);
	val |= 0x1;
	writel_relaxed(val, appbase + 0x00000208);
	usleep_range(10000, 11000);

	val = readl_relaxed(appbase + 0x00000208);
	val &= ~0x1;
	writel_relaxed(val, appbase + 0x00000208);
	mb();
	usleep_range(10000, 11000);
}

int msm_camio_probe_on(struct platform_device *pdev)
{
	int rc = 0;
	const struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_clk = camdev->ioclk;

	rc = camdev->camera_gpio_on();
	if (rc < 0)
		return rc;
	return msm_camio_clk_enable(CAMIO_CAM_MCLK_CLK);
}

int msm_camio_probe_off(struct platform_device *pdev)
{
	const struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;

	camdev->camera_gpio_off();
	return msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
}

void msm_camio_set_perf_lvl(enum msm_bus_perf_setting perf_setting)
{
	switch (perf_setting) {
	case S_INIT:
		add_axi_qos();
		break;
	case S_PREVIEW:
		update_axi_qos(MSM_AXI_QOS_PREVIEW);
		break;
	case S_VIDEO:
		update_axi_qos(MSM_AXI_QOS_RECORDING);
		break;
	case S_CAPTURE:
		update_axi_qos(MSM_AXI_QOS_SNAPSHOT);
		break;
	case S_DEFAULT:
		update_axi_qos(PM_QOS_DEFAULT_VALUE);
		break;
	case S_EXIT:
		release_axi_qos();
		break;
	default:
		CDBG("%s: INVALID CASE\n", __func__);
	}
}

static int __devinit clkctl_probe(struct platform_device *pdev)
{
	int rc = 0;

	clk_ctrl_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "clk_ctl");
	if (!clk_ctrl_mem) {
		pr_err("%s: no mem resource:3?\n", __func__);
		return -ENODEV;
	}

	appbase = ioremap(clk_ctrl_mem->start,
		resource_size(clk_ctrl_mem));
	if (!appbase) {
		pr_err("clkctl_probe: appbase:err\n");
		rc = -ENOMEM;
		goto ioremap_fail;
	}
	return 0;

ioremap_fail:
	msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
	return rc;
}

static int clkctl_remove(struct platform_device *pdev)
{
	if (clk_ctrl_mem)
		iounmap(clk_ctrl_mem);

	return 0;
}

static struct platform_driver clkctl_driver = {
	.probe  = clkctl_probe,
	.remove = clkctl_remove,
	.driver = {
		.name = "msm_clk_ctl",
		.owner = THIS_MODULE,
	},
};

static int __init msm_clkctl_init_module(void)
{
	return platform_driver_register(&clkctl_driver);
}

static void __exit msm_clkctl_exit_module(void)
{
	platform_driver_unregister(&clkctl_driver);
}

module_init(msm_clkctl_init_module);
module_exit(msm_clkctl_exit_module);
MODULE_DESCRIPTION("CAM IO driver");
MODULE_LICENSE("GPL v2");
