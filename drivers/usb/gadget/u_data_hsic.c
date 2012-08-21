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

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/termios.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <mach/usb_bridge.h>
#include <mach/usb_gadget_xport.h>

static unsigned int no_data_ports;

static const char *data_bridge_names[] = {
	"dun_data_hsic0",
	"rmnet_data_hsic0"
};

#define DATA_BRIDGE_NAME_MAX_LEN		20

#define GHSIC_DATA_RMNET_RX_Q_SIZE		50
#define GHSIC_DATA_RMNET_TX_Q_SIZE		300
#define GHSIC_DATA_SERIAL_RX_Q_SIZE		2
#define GHSIC_DATA_SERIAL_TX_Q_SIZE		2
#define GHSIC_DATA_RX_REQ_SIZE			2048

static unsigned int ghsic_data_rmnet_tx_q_size = GHSIC_DATA_RMNET_TX_Q_SIZE;
module_param(ghsic_data_rmnet_tx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsic_data_rmnet_rx_q_size = GHSIC_DATA_RMNET_RX_Q_SIZE;
module_param(ghsic_data_rmnet_rx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsic_data_serial_tx_q_size = GHSIC_DATA_SERIAL_TX_Q_SIZE;
module_param(ghsic_data_serial_tx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsic_data_serial_rx_q_size = GHSIC_DATA_SERIAL_RX_Q_SIZE;
module_param(ghsic_data_serial_rx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsic_data_rx_req_size = GHSIC_DATA_RX_REQ_SIZE;
module_param(ghsic_data_rx_req_size, uint, S_IRUGO | S_IWUSR);

/*flow ctrl*/
#define GHSIC_DATA_FLOW_CTRL_EN_THRESHOLD	500
#define GHSIC_DATA_FLOW_CTRL_DISABLE		300
#define GHSIC_DATA_FLOW_CTRL_SUPPORT		1
#define GHSIC_DATA_PENDLIMIT_WITH_BRIDGE	500

static unsigned int ghsic_data_fctrl_support = GHSIC_DATA_FLOW_CTRL_SUPPORT;
module_param(ghsic_data_fctrl_support, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsic_data_fctrl_en_thld =
		GHSIC_DATA_FLOW_CTRL_EN_THRESHOLD;
module_param(ghsic_data_fctrl_en_thld, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsic_data_fctrl_dis_thld = GHSIC_DATA_FLOW_CTRL_DISABLE;
module_param(ghsic_data_fctrl_dis_thld, uint, S_IRUGO | S_IWUSR);

static unsigned int ghsic_data_pend_limit_with_bridge =
		GHSIC_DATA_PENDLIMIT_WITH_BRIDGE;
module_param(ghsic_data_pend_limit_with_bridge, uint, S_IRUGO | S_IWUSR);

#define CH_OPENED 0
#define CH_READY 1

struct gdata_port {
	/* port */
	unsigned		port_num;

	/* gadget */
	spinlock_t		port_lock;
	void			*port_usb;
	struct usb_ep		*in;
	struct usb_ep		*out;

	enum gadget_type	gtype;

	/* data transfer queues */
	unsigned int		tx_q_size;
	struct list_head	tx_idle;
	struct sk_buff_head	tx_skb_q;

	unsigned int		rx_q_size;
	struct list_head	rx_idle;
	struct sk_buff_head	rx_skb_q;

	/* work */
	struct workqueue_struct	*wq;
	struct work_struct	connect_w;
	struct work_struct	disconnect_w;
	struct work_struct	write_tomdm_w;
	struct work_struct	write_tohost_w;

	struct bridge		brdg;

	/*bridge status*/
	unsigned long		bridge_sts;

	/*counters*/
	unsigned long		to_modem;
	unsigned long		to_host;
	unsigned int		rx_throttled_cnt;
	unsigned int		rx_unthrottled_cnt;
	unsigned int		tx_throttled_cnt;
	unsigned int		tx_unthrottled_cnt;
	unsigned int		tomodem_drp_cnt;
	unsigned int		unthrottled_pnd_skbs;
};

static struct {
	struct gdata_port	*port;
	struct platform_driver	pdrv;
} gdata_ports[NUM_PORTS];

static void ghsic_data_start_rx(struct gdata_port *port);

static void ghsic_data_free_requests(struct usb_ep *ep, struct list_head *head)
{
	struct usb_request	*req;

	while (!list_empty(head)) {
		req = list_entry(head->next, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(ep, req);
	}
}

static int ghsic_data_alloc_requests(struct usb_ep *ep, struct list_head *head,
		int num,
		void (*cb)(struct usb_ep *ep, struct usb_request *),
		gfp_t flags)
{
	int			i;
	struct usb_request	*req;

	pr_debug("%s: ep:%s head:%p num:%d cb:%p", __func__,
			ep->name, head, num, cb);

	for (i = 0; i < num; i++) {
		req = usb_ep_alloc_request(ep, flags);
		if (!req) {
			pr_debug("%s: req allocated:%d\n", __func__, i);
			return list_empty(head) ? -ENOMEM : 0;
		}
		req->complete = cb;
		list_add(&req->list, head);
	}

	return 0;
}

static void ghsic_data_unthrottle_tx(void *ctx)
{
	struct gdata_port	*port = ctx;
	unsigned long		flags;

	if (!port)
		return;

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb) {
		port->tx_unthrottled_cnt++;
		queue_work(port->wq, &port->write_tomdm_w);
		pr_debug("%s: port num =%d unthrottled\n", __func__,
			port->port_num);
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void ghsic_data_write_tohost(struct work_struct *w)
{
	unsigned long		flags;
	struct sk_buff		*skb;
	int			ret;
	struct usb_request	*req;
	struct usb_ep		*ep;
	struct gdata_port	*port;

	port = container_of(w, struct gdata_port, write_tohost_w);

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	ep = port->in;

	while (!list_empty(&port->tx_idle)) {
		skb = __skb_dequeue(&port->tx_skb_q);
		if (!skb)
			break;

		req = list_first_entry(&port->tx_idle, struct usb_request,
				list);
		req->context = skb;
		req->buf = skb->data;
		req->length = skb->len;

		list_del(&req->list);

		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = usb_ep_queue(ep, req, GFP_KERNEL);
		spin_lock_irqsave(&port->port_lock, flags);
		if (ret) {
			pr_err("%s: usb epIn failed\n", __func__);
			list_add(&req->list, &port->tx_idle);
			dev_kfree_skb_any(skb);
			break;
		}
		port->to_host++;
		if (ghsic_data_fctrl_support &&
			port->tx_skb_q.qlen <= ghsic_data_fctrl_dis_thld &&
			test_and_clear_bit(RX_THROTTLED, &port->brdg.flags)) {
			port->rx_unthrottled_cnt++;
			port->unthrottled_pnd_skbs = port->tx_skb_q.qlen;
			pr_debug_ratelimited("%s: disable flow ctrl:"
					" tx skbq len: %u\n",
					__func__, port->tx_skb_q.qlen);
			data_bridge_unthrottle_rx(port->brdg.ch_id);
		}
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static int ghsic_data_receive(void *p, void *data, size_t len)
{
	struct gdata_port	*port = p;
	unsigned long		flags;
	struct sk_buff		*skb = data;

	if (!port) {
		dev_kfree_skb_any(skb);
		return -EINVAL;
	}

	pr_debug("%s: p:%p#%d skb_len:%d\n", __func__,
			port, port->port_num, skb->len);

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		dev_kfree_skb_any(skb);
		return -ENOTCONN;
	}

	__skb_queue_tail(&port->tx_skb_q, skb);

	if (ghsic_data_fctrl_support &&
			port->tx_skb_q.qlen >= ghsic_data_fctrl_en_thld) {
		set_bit(RX_THROTTLED, &port->brdg.flags);
		port->rx_throttled_cnt++;
		pr_debug_ratelimited("%s: flow ctrl enabled: tx skbq len: %u\n",
					__func__, port->tx_skb_q.qlen);
		spin_unlock_irqrestore(&port->port_lock, flags);
		queue_work(port->wq, &port->write_tohost_w);
		return -EBUSY;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);

	queue_work(port->wq, &port->write_tohost_w);

	return 0;
}

static void ghsic_data_write_tomdm(struct work_struct *w)
{
	struct gdata_port	*port;
	struct sk_buff		*skb;
	unsigned long		flags;
	int			ret;

	port = container_of(w, struct gdata_port, write_tomdm_w);

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	if (test_bit(TX_THROTTLED, &port->brdg.flags)) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		goto start_rx;
	}

	while ((skb = __skb_dequeue(&port->rx_skb_q))) {
		pr_debug("%s: port:%p tom:%lu pno:%d\n", __func__,
				port, port->to_modem, port->port_num);

		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = data_bridge_write(port->brdg.ch_id, skb);
		spin_lock_irqsave(&port->port_lock, flags);
		if (ret < 0) {
			if (ret == -EBUSY) {
				/*flow control*/
				port->tx_throttled_cnt++;
				break;
			}
			pr_err_ratelimited("%s: write error:%d\n",
					__func__, ret);
			port->tomodem_drp_cnt++;
			dev_kfree_skb_any(skb);
			break;
		}
		port->to_modem++;
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
start_rx:
	ghsic_data_start_rx(port);
}

static void ghsic_data_epin_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gdata_port	*port = ep->driver_data;
	struct sk_buff		*skb = req->context;
	int			status = req->status;

	switch (status) {
	case 0:
		/* successful completion */
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		dev_kfree_skb_any(skb);
		req->buf = 0;
		usb_ep_free_request(ep, req);
		return;
	default:
		pr_err("%s: data tx ep error %d\n", __func__, status);
		break;
	}

	dev_kfree_skb_any(skb);

	spin_lock(&port->port_lock);
	list_add_tail(&req->list, &port->tx_idle);
	spin_unlock(&port->port_lock);

	queue_work(port->wq, &port->write_tohost_w);
}

static void
ghsic_data_epout_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gdata_port	*port = ep->driver_data;
	struct sk_buff		*skb = req->context;
	int			status = req->status;
	int			queue = 0;

	switch (status) {
	case 0:
		skb_put(skb, req->actual);
		queue = 1;
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* cable disconnection */
		dev_kfree_skb_any(skb);
		req->buf = 0;
		usb_ep_free_request(ep, req);
		return;
	default:
		pr_err_ratelimited("%s: %s response error %d, %d/%d\n",
					__func__, ep->name, status,
				req->actual, req->length);
		dev_kfree_skb_any(skb);
		break;
	}

	spin_lock(&port->port_lock);
	if (queue) {
		__skb_queue_tail(&port->rx_skb_q, skb);
		list_add_tail(&req->list, &port->rx_idle);
		queue_work(port->wq, &port->write_tomdm_w);
	}
	spin_unlock(&port->port_lock);
}

static void ghsic_data_start_rx(struct gdata_port *port)
{
	struct usb_request	*req;
	struct usb_ep		*ep;
	unsigned long		flags;
	int			ret;
	struct sk_buff		*skb;

	pr_debug("%s: port:%p\n", __func__, port);
	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	ep = port->out;

	while (port->port_usb && !list_empty(&port->rx_idle)) {
		if (port->rx_skb_q.qlen > ghsic_data_pend_limit_with_bridge)
			break;

		req = list_first_entry(&port->rx_idle,
					struct usb_request, list);

		skb = alloc_skb(ghsic_data_rx_req_size, GFP_ATOMIC);
		if (!skb)
			break;

		list_del(&req->list);
		req->buf = skb->data;
		req->length = ghsic_data_rx_req_size;
		req->context = skb;

		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = usb_ep_queue(ep, req, GFP_KERNEL);
		spin_lock_irqsave(&port->port_lock, flags);
		if (ret) {
			dev_kfree_skb_any(skb);

			pr_err_ratelimited("%s: rx queue failed\n", __func__);

			if (port->port_usb)
				list_add(&req->list, &port->rx_idle);
			else
				usb_ep_free_request(ep, req);
			break;
		}
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void ghsic_data_start_io(struct gdata_port *port)
{
	unsigned long	flags;
	struct usb_ep	*ep;
	int		ret;

	pr_debug("%s: port:%p\n", __func__, port);

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	ep = port->out;
	ret = ghsic_data_alloc_requests(ep, &port->rx_idle,
		port->rx_q_size, ghsic_data_epout_complete, GFP_ATOMIC);
	if (ret) {
		pr_err("%s: rx req allocation failed\n", __func__);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	ep = port->in;
	ret = ghsic_data_alloc_requests(ep, &port->tx_idle,
		port->tx_q_size, ghsic_data_epin_complete, GFP_ATOMIC);
	if (ret) {
		pr_err("%s: tx req allocation failed\n", __func__);
		ghsic_data_free_requests(ep, &port->rx_idle);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);

	/* queue out requests */
	ghsic_data_start_rx(port);
}

static void ghsic_data_connect_w(struct work_struct *w)
{
	struct gdata_port	*port =
		container_of(w, struct gdata_port, connect_w);
	int			ret;

	if (!port || !test_bit(CH_READY, &port->bridge_sts))
		return;

	pr_debug("%s: port:%p\n", __func__, port);

	ret = data_bridge_open(&port->brdg);
	if (ret) {
		pr_err("%s: unable open bridge ch:%d err:%d\n",
				__func__, port->brdg.ch_id, ret);
		return;
	}

	set_bit(CH_OPENED, &port->bridge_sts);

	ghsic_data_start_io(port);
}

static void ghsic_data_disconnect_w(struct work_struct *w)
{
	struct gdata_port	*port =
		container_of(w, struct gdata_port, disconnect_w);

	if (!test_bit(CH_OPENED, &port->bridge_sts))
		return;

	data_bridge_close(port->brdg.ch_id);
	clear_bit(CH_OPENED, &port->bridge_sts);
}

static void ghsic_data_free_buffers(struct gdata_port *port)
{
	struct sk_buff	*skb;
	unsigned long	flags;

	spin_lock_irqsave(&port->port_lock, flags);

	if (!port || !port->port_usb)
		goto free_buf_out;

	ghsic_data_free_requests(port->in, &port->tx_idle);
	ghsic_data_free_requests(port->out, &port->rx_idle);

	while ((skb = __skb_dequeue(&port->tx_skb_q)))
		dev_kfree_skb_any(skb);

	while ((skb = __skb_dequeue(&port->rx_skb_q)))
		dev_kfree_skb_any(skb);

free_buf_out:
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static int ghsic_data_probe(struct platform_device *pdev)
{
	struct gdata_port *port;
	unsigned long flags;

	pr_debug("%s: name:%s no_data_ports= %d\n",
		__func__, pdev->name, no_data_ports);

	if (pdev->id >= no_data_ports) {
		pr_err("%s: invalid port: %d\n", __func__, pdev->id);
		return -EINVAL;
	}

	port = gdata_ports[pdev->id].port;
	set_bit(CH_READY, &port->bridge_sts);

	spin_lock_irqsave(&port->port_lock, flags);
	/* if usb is online, try opening bridge */
	if (port->port_usb)
		queue_work(port->wq, &port->connect_w);
	spin_unlock_irqrestore(&port->port_lock, flags);

	return 0;
}

/* mdm disconnect */
static int ghsic_data_remove(struct platform_device *pdev)
{
	struct gdata_port *port;
	struct usb_ep *ep_in = NULL;
	struct usb_ep *ep_out = NULL;
	unsigned long flags;

	pr_debug("%s: name:%s\n", __func__, pdev->name);

	if (pdev->id >= no_data_ports) {
		pr_err("%s: invalid port: %d\n", __func__, pdev->id);
		return -EINVAL;
	}

	port = gdata_ports[pdev->id].port;

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb) {
		ep_in = port->in;
		ep_out = port->out;
	}
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (ep_in)
		usb_ep_fifo_flush(ep_in);
	if (ep_out)
		usb_ep_fifo_flush(ep_out);

	ghsic_data_free_buffers(port);

	data_bridge_close(port->brdg.ch_id);

	clear_bit(CH_READY, &port->bridge_sts);
	clear_bit(CH_OPENED, &port->bridge_sts);

	return 0;
}

static void ghsic_data_port_free(int portno)
{
	struct gdata_port	*port = gdata_ports[portno].port;
	struct platform_driver	*pdrv = &gdata_ports[portno].pdrv;

	destroy_workqueue(port->wq);
	kfree(port);

	if (pdrv)
		platform_driver_unregister(pdrv);
}

static int ghsic_data_port_alloc(unsigned port_num, enum gadget_type gtype)
{
	struct gdata_port	*port;
	struct platform_driver	*pdrv;

	port = kzalloc(sizeof(struct gdata_port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	port->wq = create_singlethread_workqueue(data_bridge_names[port_num]);
	if (!port->wq) {
		pr_err("%s: Unable to create workqueue:%s\n",
			__func__, data_bridge_names[port_num]);
		return -ENOMEM;
	}
	port->port_num = port_num;

	/* port initialization */
	spin_lock_init(&port->port_lock);

	INIT_WORK(&port->connect_w, ghsic_data_connect_w);
	INIT_WORK(&port->disconnect_w, ghsic_data_disconnect_w);
	INIT_WORK(&port->write_tohost_w, ghsic_data_write_tohost);
	INIT_WORK(&port->write_tomdm_w, ghsic_data_write_tomdm);

	INIT_LIST_HEAD(&port->tx_idle);
	INIT_LIST_HEAD(&port->rx_idle);

	skb_queue_head_init(&port->tx_skb_q);
	skb_queue_head_init(&port->rx_skb_q);

	port->gtype = gtype;
	port->brdg.ch_id = port_num;
	port->brdg.ctx = port;
	port->brdg.ops.send_pkt = ghsic_data_receive;
	port->brdg.ops.unthrottle_tx = ghsic_data_unthrottle_tx;
	gdata_ports[port_num].port = port;

	pdrv = &gdata_ports[port_num].pdrv;
	pdrv->probe = ghsic_data_probe;
	pdrv->remove = ghsic_data_remove;
	pdrv->driver.name = data_bridge_names[port_num];
	pdrv->driver.owner = THIS_MODULE;

	platform_driver_register(pdrv);

	pr_debug("%s: port:%p portno:%d\n", __func__, port, port_num);

	return 0;
}

void ghsic_data_disconnect(void *gptr, int port_num)
{
	struct gdata_port	*port;
	unsigned long		flags;

	pr_debug("%s: port#%d\n", __func__, port_num);

	port = gdata_ports[port_num].port;

	if (port_num > no_data_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, port_num);
		return;
	}

	if (!gptr || !port) {
		pr_err("%s: port is null\n", __func__);
		return;
	}

	ghsic_data_free_buffers(port);

	/* disable endpoints */
	if (port->in)
		usb_ep_disable(port->out);

	if (port->out)
		usb_ep_disable(port->in);

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = 0;
	port->in = NULL;
	port->out = NULL;
	clear_bit(TX_THROTTLED, &port->brdg.flags);
	clear_bit(RX_THROTTLED, &port->brdg.flags);
	spin_unlock_irqrestore(&port->port_lock, flags);

	queue_work(port->wq, &port->disconnect_w);
}

int ghsic_data_connect(void *gptr, int port_num)
{
	struct gdata_port		*port;
	struct gserial			*gser;
	struct grmnet			*gr;
	struct usb_endpoint_descriptor	*in_desc;
	struct usb_endpoint_descriptor	*out_desc;
	unsigned long			flags;
	int				ret = 0;

	pr_debug("%s: port#%d\n", __func__, port_num);

	port = gdata_ports[port_num].port;

	if (port_num > no_data_ports) {
		pr_err("%s: invalid portno#%d\n", __func__, port_num);
		return -ENODEV;
	}

	if (!gptr || !port) {
		pr_err("%s: port is null\n", __func__);
		return -ENODEV;
	}

	if (port->gtype == USB_GADGET_SERIAL) {
		gser = gptr;
		port->in = gser->in;
		port->out = gser->out;
		port->tx_q_size = ghsic_data_serial_tx_q_size;
		port->rx_q_size = ghsic_data_serial_rx_q_size;
		gser->in->driver_data = port;
		gser->out->driver_data = port;
		in_desc = gser->in_desc;
		out_desc = gser->out_desc;
	} else {
		gr = gptr;
		port->in = gr->in;
		port->out = gr->out;
		port->tx_q_size = ghsic_data_rmnet_tx_q_size;
		port->rx_q_size = ghsic_data_rmnet_rx_q_size;
		gr->in->driver_data = port;
		gr->out->driver_data = port;
		in_desc = gr->in_desc;
		out_desc = gr->out_desc;
	}

	ret = usb_ep_enable(port->in, in_desc);
	if (ret) {
		pr_err("%s: usb_ep_enable failed eptype:IN ep:%p",
				__func__, port->in);
		goto fail;
	}

	ret = usb_ep_enable(port->out, out_desc);
	if (ret) {
		pr_err("%s: usb_ep_enable failed eptype:OUT ep:%p",
				__func__, port->out);
		usb_ep_disable(port->in);
		goto fail;
	}
	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = gptr;
	port->to_host = 0;
	port->to_modem = 0;
	port->tomodem_drp_cnt = 0;
	port->rx_throttled_cnt = 0;
	port->rx_unthrottled_cnt = 0;
	port->tx_throttled_cnt = 0;
	port->tx_unthrottled_cnt = 0;
	port->unthrottled_pnd_skbs = 0;
	spin_unlock_irqrestore(&port->port_lock, flags);

	queue_work(port->wq, &port->connect_w);
fail:
	return ret;
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE 1024
static ssize_t ghsic_data_read_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	struct gdata_port	*port;
	struct platform_driver	*pdrv;
	char			*buf;
	unsigned long		flags;
	int			ret;
	int			i;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < no_data_ports; i++) {
		port = gdata_ports[i].port;
		if (!port)
			continue;
		pdrv = &gdata_ports[i].pdrv;
		spin_lock_irqsave(&port->port_lock, flags);

		temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
				"\nName:           %s\n"
				"#PORT:%d port#:   %p\n"
				"dpkts_to_usbhost: %lu\n"
				"dpkts_to_modem:   %lu\n"
				"tomodem_drp_cnt:  %u\n"
				"tx_buf_len:       %u\n"
				"rx_buf_len:       %u\n"
				"rx thld cnt       %u\n"
				"rx unthld cnt     %u\n"
				"tx thld cnt       %u\n"
				"tx unthld cnt     %u\n"
				"uthld pnd skbs    %u\n"
				"RX_THROTTLED      %d\n"
				"TX_THROTTLED      %d\n"
				"data_ch_open:     %d\n"
				"data_ch_ready:    %d\n",
				pdrv->driver.name,
				i, port,
				port->to_host, port->to_modem,
				port->tomodem_drp_cnt,
				port->tx_skb_q.qlen,
				port->rx_skb_q.qlen,
				port->rx_throttled_cnt,
				port->rx_unthrottled_cnt,
				port->tx_throttled_cnt,
				port->tx_unthrottled_cnt,
				port->unthrottled_pnd_skbs,
				test_bit(RX_THROTTLED, &port->brdg.flags),
				test_bit(TX_THROTTLED, &port->brdg.flags),
				test_bit(CH_OPENED, &port->bridge_sts),
				test_bit(CH_READY, &port->bridge_sts));

		spin_unlock_irqrestore(&port->port_lock, flags);
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;
}

static ssize_t ghsic_data_reset_stats(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct gdata_port	*port;
	int			i;
	unsigned long		flags;

	for (i = 0; i < no_data_ports; i++) {
		port = gdata_ports[i].port;
		if (!port)
			continue;

		spin_lock_irqsave(&port->port_lock, flags);
		port->to_host = 0;
		port->to_modem = 0;
		port->tomodem_drp_cnt = 0;
		port->rx_throttled_cnt = 0;
		port->rx_unthrottled_cnt = 0;
		port->tx_throttled_cnt = 0;
		port->tx_unthrottled_cnt = 0;
		port->unthrottled_pnd_skbs = 0;
		spin_unlock_irqrestore(&port->port_lock, flags);
	}
	return count;
}

const struct file_operations ghsic_stats_ops = {
	.read = ghsic_data_read_stats,
	.write = ghsic_data_reset_stats,
};

static struct dentry	*gdata_dent;
static struct dentry	*gdata_dfile;

static void ghsic_data_debugfs_init(void)
{
	gdata_dent = debugfs_create_dir("ghsic_data_xport", 0);
	if (IS_ERR(gdata_dent))
		return;

	gdata_dfile = debugfs_create_file("status", 0444, gdata_dent, 0,
			&ghsic_stats_ops);
	if (!gdata_dfile || IS_ERR(gdata_dfile))
		debugfs_remove(gdata_dent);
}

static void ghsic_data_debugfs_exit(void)
{
	debugfs_remove(gdata_dfile);
	debugfs_remove(gdata_dent);
}

#else
static void ghsic_data_debugfs_init(void) { }
static void ghsic_data_debugfs_exit(void) { }

#endif

int ghsic_data_setup(unsigned num_ports, enum gadget_type gtype)
{
	int		first_port_id = no_data_ports;
	int		total_num_ports = num_ports + no_data_ports;
	int		ret = 0;
	int		i;

	if (!num_ports || total_num_ports > NUM_PORTS) {
		pr_err("%s: Invalid num of ports count:%d\n",
				__func__, num_ports);
		return -EINVAL;
	}
	pr_debug("%s: count: %d\n", __func__, num_ports);

	for (i = first_port_id; i < (num_ports + first_port_id); i++) {

		/*probe can be called while port_alloc,so update no_data_ports*/
		no_data_ports++;
		ret = ghsic_data_port_alloc(i, gtype);
		if (ret) {
			no_data_ports--;
			pr_err("%s: Unable to alloc port:%d\n", __func__, i);
			goto free_ports;
		}
	}

	/*return the starting index*/
	return first_port_id;

free_ports:
	for (i = first_port_id; i < no_data_ports; i++)
		ghsic_data_port_free(i);
		no_data_ports = first_port_id;

	return ret;
}

static int __init ghsic_data_init(void)
{
	ghsic_data_debugfs_init();

	return 0;
}
module_init(ghsic_data_init);

static void __exit ghsic_data_exit(void)
{
	ghsic_data_debugfs_exit();
}
module_exit(ghsic_data_exit);
MODULE_DESCRIPTION("hsic data xport driver");
MODULE_LICENSE("GPL v2");
