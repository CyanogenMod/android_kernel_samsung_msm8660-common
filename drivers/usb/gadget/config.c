/*
 * usb/gadget/config.c -- simplify building config descriptors
 *
 * Copyright (C) 2003 David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/device.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>


/**
 * usb_descriptor_fillbuf - fill buffer with descriptors
 * @buf: Buffer to be filled
 * @buflen: Size of buf
 * @src: Array of descriptor pointers, terminated by null pointer.
 *
 * Copies descriptors into the buffer, returning the length or a
 * negative error code if they can't all be copied.  Useful when
 * assembling descriptors for an associated set of interfaces used
 * as part of configuring a composite device; or in other cases where
 * sets of descriptors need to be marshaled.
 */
int
usb_descriptor_fillbuf(void *buf, unsigned buflen,
		const struct usb_descriptor_header **src)
{
	u8	*dest = buf;

	if (!src)
		return -EINVAL;

	/* fill buffer from src[] until null descriptor ptr */
	for (; NULL != *src; src++) {
		unsigned		len = (*src)->bLength;

		if (len > buflen)
			return -EINVAL;
		memcpy(dest, *src, len);
		buflen -= len;
		dest += len;
	}
	return dest - (u8 *)buf;
}


/**
 * usb_gadget_config_buf - builts a complete configuration descriptor
 * @config: Header for the descriptor, including characteristics such
 *	as power requirements and number of interfaces.
 * @desc: Null-terminated vector of pointers to the descriptors (interface,
 *	endpoint, etc) defining all functions in this device configuration.
 * @buf: Buffer for the resulting configuration descriptor.
 * @length: Length of buffer.  If this is not big enough to hold the
 *	entire configuration descriptor, an error code will be returned.
 *
 * This copies descriptors into the response buffer, building a descriptor
 * for that configuration.  It returns the buffer length or a negative
 * status code.  The config.wTotalLength field is set to match the length
 * of the result, but other descriptor fields (including power usage and
 * interface count) must be set by the caller.
 *
 * Gadget drivers could use this when constructing a config descriptor
 * in response to USB_REQ_GET_DESCRIPTOR.  They will need to patch the
 * resulting bDescriptorType value if USB_DT_OTHER_SPEED_CONFIG is needed.
 */
int usb_gadget_config_buf(
	const struct usb_config_descriptor	*config,
	void					*buf,
	unsigned				length,
	const struct usb_descriptor_header	**desc
)
{
	struct usb_config_descriptor		*cp = buf;
	int					len;

	/* config descriptor first */
	if (length < USB_DT_CONFIG_SIZE || !desc)
		return -EINVAL;
	*cp = *config;

	/* then interface/endpoint/class/vendor/... */
	len = usb_descriptor_fillbuf(USB_DT_CONFIG_SIZE + (u8*)buf,
			length - USB_DT_CONFIG_SIZE, desc);
	if (len < 0)
		return len;
	len += USB_DT_CONFIG_SIZE;
	if (len > 0xffff)
		return -EINVAL;

	/* patch up the config descriptor */
	cp->bLength = USB_DT_CONFIG_SIZE;
	cp->bDescriptorType = USB_DT_CONFIG;
	cp->wTotalLength = cpu_to_le16(len);
	cp->bmAttributes |= USB_CONFIG_ATT_ONE;
	return len;
}

/**
 * usb_copy_descriptors - copy a vector of USB descriptors
 * @src: null-terminated vector to copy
 * Context: initialization code, which may sleep
 *
 * This makes a copy of a vector of USB descriptors.  Its primary use
 * is to support usb_function objects which can have multiple copies,
 * each needing different descriptors.  Functions may have static
 * tables of descriptors, which are used as templates and customized
 * with identifiers (for interfaces, strings, endpoints, and more)
 * as needed by a given function instance.
 */
struct usb_descriptor_header **
usb_copy_descriptors(struct usb_descriptor_header **src)
{
	struct usb_descriptor_header **tmp;
	unsigned bytes;
	unsigned n_desc;
	void *mem;
	struct usb_descriptor_header **ret;

	/* count descriptors and their sizes; then add vector size */
	for (bytes = 0, n_desc = 0, tmp = src; *tmp; tmp++, n_desc++)
		bytes += (*tmp)->bLength;
	bytes += (n_desc + 1) * sizeof(*tmp);

	mem = kmalloc(bytes, GFP_KERNEL);
	if (!mem)
		return NULL;

	/* fill in pointers starting at "tmp",
	 * to descriptors copied starting at "mem";
	 * and return "ret"
	 */
	tmp = mem;
	ret = mem;
	mem += (n_desc + 1) * sizeof(*tmp);
	while (*src) {
		memcpy(mem, *src, (*src)->bLength);
		*tmp = mem;
		tmp++;
		mem += (*src)->bLength;
		src++;
	}
	*tmp = NULL;

	return ret;
}

/**
 * usb_find_endpoint - find a copy of an endpoint descriptor
 * @src: original vector of descriptors
 * @copy: copy of @src
 * @match: endpoint descriptor found in @src
 *
 * This returns the copy of the @match descriptor made for @copy.  Its
 * intended use is to help remembering the endpoint descriptor to use
 * when enabling a given endpoint.
 */
struct usb_endpoint_descriptor *
usb_find_endpoint(
	struct usb_descriptor_header **src,
	struct usb_descriptor_header **copy,
	struct usb_endpoint_descriptor *match
)
{
	while (*src) {
		if (*src == (void *) match)
			return (void *)*copy;
		src++;
		copy++;
	}
	return NULL;
}

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/**
 * usb_find_descriptor - find a copy of an descriptor header
 * @src: original vector of descriptors
 * @copy: copy of @src
 * @match: descriptor found in @src
 *
 * This returns the copy of the @match descriptor made for @copy.  Its
 * intended use is to help remembering the interface descriptor to use
 * when changing a interface.
 */
struct usb_descriptor_header *
usb_find_descriptor_header(
	struct usb_descriptor_header **src,
	struct usb_descriptor_header **copy,
	struct usb_descriptor_header *match
)
{
	while (*src) {
		if (*src == (void *) match)
			return (void *)*copy;
		src++;
		copy++;
	}
	return NULL;
}

int usb_change_interface_num(
	struct usb_descriptor_header **src,
	struct usb_descriptor_header **copy,
	struct usb_interface_descriptor *match,
	int num)
{
	struct usb_descriptor_header *find_desc = NULL;

	find_desc = usb_find_descriptor_header(
		src, copy, (struct usb_descriptor_header *)match);
	if (find_desc) {
		((struct usb_interface_descriptor *)find_desc)
		    ->bInterfaceNumber = num;
		return 1;
	}
	return 0;
}

int usb_change_iad_num(
	struct usb_descriptor_header **src,
	struct usb_descriptor_header **copy,
	struct usb_interface_assoc_descriptor *match,
	int num)
{
	struct usb_descriptor_header *find_desc = NULL;

	find_desc = usb_find_descriptor_header(
		src, copy, (struct usb_descriptor_header *)match);
	if (find_desc) {
		((struct usb_interface_assoc_descriptor *)find_desc)
		    ->bFirstInterface = num;
		return 1;
	}
	return 0;
}

int usb_change_cdc_union_num(
	struct usb_descriptor_header **src,
	struct usb_descriptor_header **copy,
	struct usb_cdc_union_desc *match,
	int num,
	int master)
{
	struct usb_descriptor_header *find_desc = NULL;

	find_desc = usb_find_descriptor_header(
		src, copy, (struct usb_descriptor_header *)match);
	if (find_desc) {
		if (master) {
			((struct usb_cdc_union_desc *)find_desc)
			    ->bMasterInterface0 = num;
		} else {
			((struct usb_cdc_union_desc *)find_desc)
			    ->bSlaveInterface0 = num;
		}
		return 1;
	}
	return 0;
}

int usb_change_cdc_call_mgmt_num(
	struct usb_descriptor_header **src,
	struct usb_descriptor_header **copy,
	struct usb_cdc_call_mgmt_descriptor *match,
	int num)
{
	struct usb_descriptor_header *find_desc = NULL;

	find_desc = usb_find_descriptor_header(
		src, copy, (struct usb_descriptor_header *)match);
	if (find_desc) {
		((struct usb_cdc_call_mgmt_descriptor *)find_desc)
		    ->bDataInterface = num;
		return 1;
	}
	return 0;
}
#endif

