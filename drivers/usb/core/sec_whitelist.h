/*
 * drivers/usb/core/sec_whitelist.h
 *
 * Copyright (C) 2011 samsung corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

static struct usb_device_id sec_whitelist_table1[] = {
	/* hubs are optional in OTG, but very handy ... */
	{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 0), },
	{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 1), },
#ifdef	CONFIG_USB_PRINTER
	{ USB_DEVICE_INFO(USB_CLASS_PRINTER, 1, 1) },
	{ USB_DEVICE_INFO(USB_CLASS_PRINTER, 1, 2) },
	{ USB_DEVICE_INFO(USB_CLASS_PRINTER, 1, 3) },
	{ USB_INTERFACE_INFO(USB_CLASS_PRINTER, 1, 1) },
	{ USB_INTERFACE_INFO(USB_CLASS_PRINTER, 1, 2) },
	{ USB_INTERFACE_INFO(USB_CLASS_PRINTER, 1, 3) },
#endif
	/* HID keyboard, mouse*/
	{ USB_DEVICE_INFO(USB_CLASS_HID, 1, 1) },
	{ USB_DEVICE_INFO(USB_CLASS_HID, 1, 2) },
	{ USB_INTERFACE_INFO(USB_CLASS_HID, 1, 1) },
	{ USB_INTERFACE_INFO(USB_CLASS_HID, 1, 2) },
	{ USB_INTERFACE_INFO(USB_CLASS_HID, 0, 0) },
	/* USB storage*/
	{ USB_DEVICE_INFO(USB_CLASS_MASS_STORAGE, 6, 50) },
	{ USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, 6, 50) },
	/* PTP camera*/
	{ USB_DEVICE_INFO(USB_CLASS_STILL_IMAGE, 1, 1) },
	{ USB_INTERFACE_INFO(USB_CLASS_STILL_IMAGE, 1, 1) },
	{ }	/* Terminating entry */
};
static struct usb_device_id sec_whitelist_table2[] = {
	/* hubs are optional in OTG, but very handy ... */
	{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 0), },
	{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 1), },
	/* USB storage*/
	{ USB_DEVICE_INFO(USB_CLASS_MASS_STORAGE, 6, 50) },
	{ USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, 6, 50) },
	{ }	/* Terminating entry */
};

static struct usb_device_id *sec_table_list[] = {
	sec_whitelist_table1,
	sec_whitelist_table2,
};

static int is_seclist(struct usb_device *dev, int list_num)
{
	struct usb_device_id	*id;
	struct usb_interface_cache *intfc;
	__u8  target_InterfaceClass;
	int nintf = dev->config->desc.bNumInterfaces;
	int i = 0, j = 0;

	if (list_num < 1) {
		dev_err(&dev->dev, "is_seclist error, list_num %d\n", list_num);
		return 1;
	} else if (list_num > ARRAY_SIZE(sec_table_list)) {
		dev_err(&dev->dev, "is_seclist error, list_num %d\n", list_num);
		list_num = ARRAY_SIZE(sec_table_list);
	}
	for (i = 0; i < nintf; ++i) {
		if (dev->config->intf_cache[i]) {
			intfc = dev->config->intf_cache[i];
			for (j = 0; j < intfc->num_altsetting; ++j) {
				target_InterfaceClass =
					intfc->altsetting[j]
						.desc.bInterfaceClass;

				for (id = sec_table_list[list_num-1];
						id->match_flags; id++) {
					if ((id->match_flags &
						USB_DEVICE_ID_MATCH_DEV_CLASS)
					&& (id->bDeviceClass
						!= dev->descriptor
							.bDeviceClass))
						continue;
					if ((id->match_flags &
						USB_DEVICE_ID_MATCH_INT_CLASS)
					&& (id->bInterfaceClass
						!= target_InterfaceClass))
						continue;

					return 1;
				}
			}
		} else
			dev_err(&dev->dev,
				"dev->config->intf_cache is null\n");
	}

	/* HOST MESSAGE: report errors here, customize to match your product */
	dev_err(&dev->dev, "device v%04x p%04x is not supported\n",
		le16_to_cpu(dev->descriptor.idVendor),
		le16_to_cpu(dev->descriptor.idProduct));
	return 0;
}

