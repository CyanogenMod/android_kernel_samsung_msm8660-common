/*
 * Copyright (C) 2007 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_USB_GADGET_MSM72K_UDC_H__
#define __LINUX_USB_GADGET_MSM72K_UDC_H__

#define USB_AHBBURST         (MSM_USB_BASE + 0x0090)
#define USB_AHBMODE          (MSM_USB_BASE + 0x0098)
#define USB_CAPLENGTH        (MSM_USB_BASE + 0x0100) /* 8 bit */

#define USB_USBCMD           (MSM_USB_BASE + 0x0140)
#define USB_USBSTS           (MSM_USB_BASE + 0x0144)
#define USB_PORTSC           (MSM_USB_BASE + 0x0184)
#define USB_OTGSC            (MSM_USB_BASE + 0x01A4)
#define USB_USBMODE          (MSM_USB_BASE + 0x01A8)
#define USB_PHY_CTRL         (MSM_USB_BASE + 0x0240)

#define USBCMD_RESET   2
#define USB_USBINTR          (MSM_USB_BASE + 0x0148)

#define PORTSC_PHCD            (1 << 23) /* phy suspend mode */
#define PORTSC_PTS_MASK         (3 << 30)
#define PORTSC_PTS_ULPI         (3 << 30)

#define USB_ULPI_VIEWPORT    (MSM_USB_BASE + 0x0170)
#define ULPI_RUN              (1 << 30)
#define ULPI_WRITE            (1 << 29)
#define ULPI_READ             (0 << 29)
#define ULPI_SYNC_STATE       (1 << 27)
#define ULPI_ADDR(n)          (((n) & 255) << 16)
#define ULPI_DATA(n)          ((n) & 255)
#define ULPI_DATA_READ(n)     (((n) >> 8) & 255)

/* synopsys 28nm phy registers */
#define ULPI_PWR_CLK_MNG_REG	0x88
#define OTG_COMP_DISABLE	BIT(0)

#define PHY_ALT_INT		(1 << 28) /* PHY alternate interrupt */
#define ASYNC_INTR_CTRL         (1 << 29) /* Enable async interrupt */
#define ULPI_STP_CTRL           (1 << 30) /* Block communication with PHY */
#define PHY_RETEN               (1 << 1) /* PHY retention enable/disable */
#define PHY_IDHV_INTEN          (1 << 8) /* PHY ID HV interrupt */
#define PHY_OTGSESSVLDHV_INTEN  (1 << 9) /* PHY Session Valid HV int. */

/* OTG definitions */
#define OTGSC_INTSTS_MASK	(0x7f << 16)
#define OTGSC_IDPU		(1 << 5)
#define OTGSC_ID		(1 << 8)
#define OTGSC_BSV		(1 << 11)
#define OTGSC_IDIS		(1 << 16)
#define OTGSC_BSVIS		(1 << 19)
#define OTGSC_IDIE		(1 << 24)
#define OTGSC_BSVIE		(1 << 27)

#endif /* __LINUX_USB_GADGET_MSM72K_UDC_H__ */
