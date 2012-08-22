/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include "msm.h"
#include "msm_sensor.h"

#define MSM_MAX_CAMERA_SENSORS 5

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static unsigned msm_camera_v4l2_nr = -1;
static struct msm_cam_server_dev g_server_dev;
static struct class *msm_class;
static dev_t msm_devno;
static int vnode_count;

module_param(msm_camera_v4l2_nr, uint, 0644);
MODULE_PARM_DESC(msm_camera_v4l2_nr, "videoX start number, -1 is autodetect");

static void msm_queue_init(struct msm_device_queue *queue, const char *name)
{
	D("%s\n", __func__);
	spin_lock_init(&queue->lock);
	queue->len = 0;
	queue->max = 0;
	queue->name = name;
	INIT_LIST_HEAD(&queue->list);
	init_waitqueue_head(&queue->wait);
}

static void msm_enqueue(struct msm_device_queue *queue,
			struct list_head *entry)
{
	unsigned long flags;
	spin_lock_irqsave(&queue->lock, flags);
	queue->len++;
	if (queue->len > queue->max) {
		queue->max = queue->len;
		pr_info("%s: queue %s new max is %d\n", __func__,
			queue->name, queue->max);
	}
	list_add_tail(entry, &queue->list);
	wake_up(&queue->wait);
	D("%s: woke up %s\n", __func__, queue->name);
	spin_unlock_irqrestore(&queue->lock, flags);
}

/* callback function from all subdevices of a msm_cam_v4l2_device */
static void msm_cam_v4l2_subdev_notify(struct v4l2_subdev *sd,
				unsigned int notification, void *arg)
{
	struct msm_cam_v4l2_device *pcam;

	if (sd == NULL)
		return;

	pcam = to_pcam(sd->v4l2_dev);

	if (pcam == NULL)
		return;

	/* forward to media controller for any changes*/
	if (pcam->mctl.mctl_notify) {
		pcam->mctl.mctl_notify(&pcam->mctl, notification, arg);
	}
}

static int msm_ctrl_cmd_done(void __user *arg)
{
	void __user *uptr;
	struct msm_queue_cmd *qcmd;
	struct msm_ctrl_cmd *command = &g_server_dev.ctrl;

	D("%s\n", __func__);

	if (copy_from_user(command, arg,
			sizeof(struct msm_ctrl_cmd)))
		return -EINVAL;

	qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_KERNEL);
	atomic_set(&qcmd->on_heap, 1);
	uptr = command->value;
	qcmd->command = command;

	if (command->length > 0) {
		command->value = g_server_dev.ctrl_data;
		if (command->length > sizeof(g_server_dev.ctrl_data)) {
			pr_err("%s: user data %d is too big (max %d)\n",
				__func__, command->length,
				sizeof(g_server_dev.ctrl_data));
			free_qcmd(qcmd);
			return -EINVAL;
		}
		if (copy_from_user(command->value, uptr, command->length)) {
			free_qcmd(qcmd);
			return -EINVAL;
		}
	}

	msm_enqueue(&g_server_dev.ctrl_q, &qcmd->list_control);
	return 0;
}

/* send control command to config and wait for results*/
static int msm_server_control(struct msm_cam_server_dev *server_dev,
				struct msm_ctrl_cmd *out)
{
	int rc = 0;
	void *value;
	struct msm_queue_cmd *rcmd;
	struct msm_ctrl_cmd *ctrlcmd;
	struct msm_device_queue *queue =  &server_dev->ctrl_q;
	void *ctrlcmd_data;

	struct v4l2_event v4l2_evt;
	struct msm_isp_event_ctrl *isp_event;
	isp_event = kzalloc(sizeof(struct msm_isp_event_ctrl), GFP_KERNEL);
	if (!isp_event) {
		pr_err("%s Insufficient memory. return", __func__);
		return -ENOMEM;
	}

	D("%s\n", __func__);

	g_server_dev.server_evt_id++;
	if (g_server_dev.server_evt_id == 0)
		g_server_dev.server_evt_id++;
	isp_event->evt_id = g_server_dev.server_evt_id;

	v4l2_evt.type = V4L2_EVENT_PRIVATE_START + MSM_CAM_RESP_V4L2;
	/* setup event object to transfer the command; */
	*((uint32_t *)v4l2_evt.u.data) = (uint32_t)isp_event;
	isp_event->resptype = MSM_CAM_RESP_V4L2;
	isp_event->isp_data.ctrl = *out;
	if (out->length > 0 && out->value != NULL) {
		ctrlcmd_data = kzalloc(out->length, GFP_KERNEL);
		if (!ctrlcmd_data) {
			kfree(isp_event);
			pr_err("%s Insufficient memory. return", __func__);
			return -ENOMEM;
		}
	memcpy(ctrlcmd_data, out->value, out->length);
	isp_event->isp_data.ctrl.value = ctrlcmd_data;
	}
	/* now send command to config thread in userspace,
	 * and wait for results */
	v4l2_event_queue(server_dev->server_command_queue.pvdev,
					  &v4l2_evt);

	D("%s v4l2_event_queue: type = 0x%x\n", __func__, v4l2_evt.type);

	/* wait for config return status */
	D("Waiting for config status\n");
	rc = wait_event_interruptible_timeout(queue->wait,
		!list_empty_careful(&queue->list),
		out->timeout_ms);
	D("Waiting is over for config status\n");
	if (list_empty_careful(&queue->list)) {
		if (!rc)
			rc = -ETIMEDOUT;
		if (rc < 0) {
			if (g_server_dev.server_evt_id == 0)
				g_server_dev.server_evt_id++;
			pr_err("%s: wait_event error %d\n", __func__, rc);
			return rc;
		}
	}

	rcmd = msm_dequeue(queue, list_control);
	BUG_ON(!rcmd);
	D("%s Finished servicing ioctl\n", __func__);

	ctrlcmd = (struct msm_ctrl_cmd *)(rcmd->command);
	value = out->value;
	if (ctrlcmd->length > 0)
		memcpy(value, ctrlcmd->value, ctrlcmd->length);

	memcpy(out, ctrlcmd, sizeof(struct msm_ctrl_cmd));
	out->value = value;

	free_qcmd(rcmd);
	D("%s: rc %d\n", __func__, rc);
	/* rc is the time elapsed. */
	if (rc >= 0) {
		/* TODO: Refactor msm_ctrl_cmd::status field */
		if (out->status == 0)
			rc = -1;
		else if (out->status == 1 || out->status == 4)
			rc = 0;
		else
			rc = -EINVAL;
	}
	return rc;
}

/*send open command to server*/
static int msm_send_open_server(int vnode_id)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	D("%s\n", __func__);
	ctrlcmd.type	   = MSM_V4L2_OPEN;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.length	 = strnlen(g_server_dev.config_info.config_dev_name[0],
				MAX_DEV_NAME_LEN)+1;
	ctrlcmd.value    = (char *)g_server_dev.config_info.config_dev_name[0];
	ctrlcmd.vnode_id = vnode_id;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

static int msm_send_close_server(int vnode_id)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	D("%s\n", __func__);
	ctrlcmd.type	   = MSM_V4L2_CLOSE;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.length	 = strnlen(g_server_dev.config_info.config_dev_name[0],
				MAX_DEV_NAME_LEN)+1;
	ctrlcmd.value    = (char *)g_server_dev.config_info.config_dev_name[0];
	ctrlcmd.vnode_id = vnode_id;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

static int msm_server_set_fmt(struct msm_cam_v4l2_device *pcam, int idx,
				 struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_pix_format *pix = &pfmt->fmt.pix;
	struct msm_ctrl_cmd ctrlcmd;
	struct img_plane_info plane_info;

	plane_info.width = pix->width;
	plane_info.height = pix->height;
	plane_info.pixelformat = pix->pixelformat;
	plane_info.buffer_type = pfmt->type;
	plane_info.ext_mode = pcam->dev_inst[idx]->image_mode;
	plane_info.num_planes = 1;
	D("%s: %d, %d, 0x%x\n", __func__,
		pfmt->fmt.pix.width, pfmt->fmt.pix.height,
		pfmt->fmt.pix.pixelformat);

	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		D("%s, Attention! Wrong buf-type %d\n", __func__, pfmt->type);

	for (i = 0; i < pcam->num_fmts; i++)
		if (pcam->usr_fmts[i].fourcc == pix->pixelformat)
			break;
	if (i == pcam->num_fmts) {
		pr_err("%s: User requested pixelformat %x not supported\n",
						__func__, pix->pixelformat);
		return -EINVAL;
	}

	ctrlcmd.type       = MSM_V4L2_VID_CAP_TYPE;
	ctrlcmd.length     = sizeof(struct img_plane_info);
	ctrlcmd.value      = (void *)&plane_info;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.vnode_id   = pcam->vnode_id;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	if (rc >= 0) {
		pcam->dev_inst[idx]->vid_fmt = *pfmt;
		pcam->dev_inst[idx]->sensor_pxlcode
					= pcam->usr_fmts[i].pxlcode;
		D("%s:inst=0x%x,idx=%d,width=%d,heigth=%d\n",
			 __func__, (u32)pcam->dev_inst[idx], idx,
			 pcam->dev_inst[idx]->vid_fmt.fmt.pix.width,
			 pcam->dev_inst[idx]->vid_fmt.fmt.pix.height);
		pcam->dev_inst[idx]->plane_info = plane_info;
	}

	return rc;
}

static int msm_server_set_fmt_mplane(struct msm_cam_v4l2_device *pcam, int idx,
				 struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_pix_format_mplane *pix_mp = &pfmt->fmt.pix_mp;
	struct msm_ctrl_cmd ctrlcmd;
	struct img_plane_info plane_info;

	plane_info.width = pix_mp->width;
	plane_info.height = pix_mp->height;
	plane_info.pixelformat = pix_mp->pixelformat;
	plane_info.buffer_type = pfmt->type;
	plane_info.ext_mode = pcam->dev_inst[idx]->image_mode;
	plane_info.num_planes = pix_mp->num_planes;
	if (plane_info.num_planes <= 0 ||
		plane_info.num_planes > VIDEO_MAX_PLANES) {
		pr_err("%s Invalid number of planes set %d", __func__,
				plane_info.num_planes);
		return -EINVAL;
	}
	D("%s: %d, %d, 0x%x\n", __func__,
		pfmt->fmt.pix_mp.width, pfmt->fmt.pix_mp.height,
		pfmt->fmt.pix_mp.pixelformat);

	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pr_err("%s, Attention! Wrong buf-type %d\n",
			__func__, pfmt->type);
		return -EINVAL;
	}

	for (i = 0; i < pcam->num_fmts; i++)
		if (pcam->usr_fmts[i].fourcc == pix_mp->pixelformat)
			break;
	if (i == pcam->num_fmts) {
		pr_err("%s: User requested pixelformat %x not supported\n",
						__func__, pix_mp->pixelformat);
		return -EINVAL;
	}

	ctrlcmd.type       = MSM_V4L2_VID_CAP_TYPE;
	ctrlcmd.length     = sizeof(struct img_plane_info);
	ctrlcmd.value      = (void *)&plane_info;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.vnode_id   = pcam->vnode_id;

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);
	if (rc >= 0) {
		pcam->dev_inst[idx]->vid_fmt = *pfmt;
		pcam->dev_inst[idx]->sensor_pxlcode
					= pcam->usr_fmts[i].pxlcode;
		D("%s:inst=0x%x,idx=%d,width=%d,heigth=%d\n",
			 __func__, (u32)pcam->dev_inst[idx], idx,
			 pcam->dev_inst[idx]->vid_fmt.fmt.pix_mp.width,
			 pcam->dev_inst[idx]->vid_fmt.fmt.pix_mp.height);
		pcam->dev_inst[idx]->plane_info = plane_info;
	}

	return rc;
}

static int msm_server_streamon(struct msm_cam_v4l2_device *pcam, int idx)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	D("%s\n", __func__);
	ctrlcmd.type	   = MSM_V4L2_STREAM_ON;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.length	 = 0;
	ctrlcmd.value    = NULL;
	ctrlcmd.stream_type = pcam->dev_inst[idx]->image_mode;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];


	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

static int msm_server_streamoff(struct msm_cam_v4l2_device *pcam, int idx)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;

	D("%s, pcam = 0x%x\n", __func__, (u32)pcam);
	ctrlcmd.type        = MSM_V4L2_STREAM_OFF;
	ctrlcmd.timeout_ms  = 10000;
	ctrlcmd.length      = 0;
	ctrlcmd.value       = NULL;
	ctrlcmd.stream_type = pcam->dev_inst[idx]->image_mode;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

static int msm_server_proc_ctrl_cmd(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_control *ctrl, int is_set_cmd)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd, *tmp_cmd;
	uint8_t *ctrl_data = NULL;
	void __user *uptr_cmd;
	void __user *uptr_value;
	uint32_t cmd_len = sizeof(struct msm_ctrl_cmd);
	uint32_t value_len;

	tmp_cmd = (struct msm_ctrl_cmd *)ctrl->value;
	uptr_cmd = (void __user *)ctrl->value;
	uptr_value = (void __user *)tmp_cmd->value;
	value_len = tmp_cmd->length;

	D("%s: cmd type = %d, up1=0x%x, ulen1=%d, up2=0x%x, ulen2=%d\n",
		__func__, tmp_cmd->type, (uint32_t)uptr_cmd, cmd_len,
		(uint32_t)uptr_value, tmp_cmd->length);

	ctrl_data = kzalloc(value_len+cmd_len, GFP_KERNEL);
	if (ctrl_data == 0) {
		pr_err("%s could not allocate memory\n", __func__);
		rc = -ENOMEM;
		goto end;
	}
	tmp_cmd = (struct msm_ctrl_cmd *)ctrl_data;
	if (copy_from_user((void *)ctrl_data, uptr_cmd,
					cmd_len)) {
		pr_err("%s: copy_from_user failed.\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	tmp_cmd->value = (void *)(ctrl_data+cmd_len);
	if (uptr_value && tmp_cmd->length > 0) {
		if (copy_from_user((void *)tmp_cmd->value, uptr_value,
						value_len)) {
			pr_err("%s: copy_from_user failed, size=%d\n",
				__func__, value_len);
			rc = -EINVAL;
			goto end;
		}
	} else
	tmp_cmd->value = NULL;

	ctrlcmd.type = MSM_V4L2_SET_CTRL_CMD;
	ctrlcmd.length = cmd_len + value_len;
	ctrlcmd.value = (void *)ctrl_data;
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];
	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);
	D("%s: msm_server_control rc=%d\n", __func__, rc);
	if (rc == 0) {
		if (uptr_value && tmp_cmd->length > 0 &&
			copy_to_user((void __user *)uptr_value,
				(void *)(ctrl_data+cmd_len), tmp_cmd->length)) {
			pr_err("%s: copy_to_user failed, size=%d\n",
				__func__, tmp_cmd->length);
			rc = -EINVAL;
			goto end;
		}
		tmp_cmd->value = uptr_value;
		if (copy_to_user((void __user *)uptr_cmd,
			(void *)tmp_cmd, cmd_len)) {
			pr_err("%s: copy_to_user failed in cpy, size=%d\n",
				__func__, cmd_len);
			rc = -EINVAL;
			goto end;
		}
	}
end:
	D("%s: END, type = %d, vaddr = 0x%x, vlen = %d, status = %d, rc = %d\n",
		__func__, tmp_cmd->type, (uint32_t)tmp_cmd->value,
		tmp_cmd->length, tmp_cmd->status, rc);
	kfree(ctrl_data);
	ctrl_data = NULL;
	return rc;
}

static int msm_server_s_ctrl(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_control *ctrl)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	uint8_t ctrl_data[max_control_command_size];

	WARN_ON(ctrl == NULL);

	if (ctrl && ctrl->id == MSM_V4L2_PID_CTRL_CMD)
		return msm_server_proc_ctrl_cmd(pcam, ctrl, 1);

	memset(ctrl_data, 0, sizeof(ctrl_data));

	ctrlcmd.type = MSM_V4L2_SET_CTRL;
	ctrlcmd.length = sizeof(struct v4l2_control);
	ctrlcmd.value = (void *)ctrl_data;
	memcpy(ctrlcmd.value, ctrl, ctrlcmd.length);
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

static int msm_server_g_ctrl(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_control *ctrl)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	uint8_t ctrl_data[max_control_command_size];

	WARN_ON(ctrl == NULL);
	if (ctrl && ctrl->id == MSM_V4L2_PID_CTRL_CMD)
		return msm_server_proc_ctrl_cmd(pcam, ctrl, 0);

	memset(ctrl_data, 0, sizeof(ctrl_data));

	ctrlcmd.type = MSM_V4L2_GET_CTRL;
	ctrlcmd.length = sizeof(struct v4l2_control);
	ctrlcmd.value = (void *)ctrl_data;
	memcpy(ctrlcmd.value, ctrl, ctrlcmd.length);
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	ctrl->value = ((struct v4l2_control *)ctrlcmd.value)->value;

	return rc;
}

static int msm_server_q_ctrl(struct msm_cam_v4l2_device *pcam,
			struct v4l2_queryctrl *queryctrl)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	uint8_t ctrl_data[max_control_command_size];

	WARN_ON(queryctrl == NULL);
	memset(ctrl_data, 0, sizeof(ctrl_data));

	ctrlcmd.type = MSM_V4L2_QUERY_CTRL;
	ctrlcmd.length = sizeof(struct v4l2_queryctrl);
	ctrlcmd.value = (void *)ctrl_data;
	memcpy(ctrlcmd.value, queryctrl, ctrlcmd.length);
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in userspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);
	D("%s: rc = %d\n", __func__, rc);

	if (rc >= 0)
		memcpy(queryctrl, ctrlcmd.value, sizeof(struct v4l2_queryctrl));

	return rc;
}

static int msm_server_get_fmt(struct msm_cam_v4l2_device *pcam,
		 int idx, struct v4l2_format *pfmt)
{
	struct v4l2_pix_format *pix = &pfmt->fmt.pix;

	pix->width        = pcam->dev_inst[idx]->vid_fmt.fmt.pix.width;
	pix->height       = pcam->dev_inst[idx]->vid_fmt.fmt.pix.height;
	pix->field        = pcam->dev_inst[idx]->vid_fmt.fmt.pix.field;
	pix->pixelformat  = pcam->dev_inst[idx]->vid_fmt.fmt.pix.pixelformat;
	pix->bytesperline = pcam->dev_inst[idx]->vid_fmt.fmt.pix.bytesperline;
	pix->colorspace   = pcam->dev_inst[idx]->vid_fmt.fmt.pix.colorspace;
	if (pix->bytesperline < 0)
		return pix->bytesperline;

	pix->sizeimage    = pix->height * pix->bytesperline;

	return 0;
}

static int msm_server_get_fmt_mplane(struct msm_cam_v4l2_device *pcam,
		 int idx, struct v4l2_format *pfmt)
{
	*pfmt = pcam->dev_inst[idx]->vid_fmt;
	return 0;
}

static int msm_server_try_fmt(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_pix_format *pix = &pfmt->fmt.pix;

	D("%s: 0x%x\n", __func__, pix->pixelformat);
	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		pr_err("%s: pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE!\n",
							__func__);
		return -EINVAL;
	}

	/* check if the format is supported by this host-sensor combo */
	for (i = 0; i < pcam->num_fmts; i++) {
		D("%s: usr_fmts.fourcc: 0x%x\n", __func__,
			pcam->usr_fmts[i].fourcc);
		if (pcam->usr_fmts[i].fourcc == pix->pixelformat)
			break;
	}

	if (i == pcam->num_fmts) {
		pr_err("%s: Format %x not found\n", __func__, pix->pixelformat);
		return -EINVAL;
	}
	return rc;
}

static int msm_server_try_fmt_mplane(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_pix_format_mplane *pix_mp = &pfmt->fmt.pix_mp;

	D("%s: 0x%x\n", __func__, pix_mp->pixelformat);
	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pr_err("%s: Incorrect format type %d ",
			__func__, pfmt->type);
		return -EINVAL;
	}

	/* check if the format is supported by this host-sensor combo */
	for (i = 0; i < pcam->num_fmts; i++) {
		D("%s: usr_fmts.fourcc: 0x%x\n", __func__,
			pcam->usr_fmts[i].fourcc);
		if (pcam->usr_fmts[i].fourcc == pix_mp->pixelformat)
			break;
	}

	if (i == pcam->num_fmts) {
		pr_err("%s: Format %x not found\n",
			__func__, pix_mp->pixelformat);
		return -EINVAL;
	}
	return rc;
}

static int msm_camera_get_crop(struct msm_cam_v4l2_device *pcam,
				int idx, struct v4l2_crop *crop)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;

	BUG_ON(crop == NULL);

	ctrlcmd.type = MSM_V4L2_GET_CROP;
	ctrlcmd.length = sizeof(struct v4l2_crop);
	ctrlcmd.value = (void *)crop;
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.stream_type = pcam->dev_inst[idx]->image_mode;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in userspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);
	D("%s: rc = %d\n", __func__, rc);

	return rc;
}

/*
 *
 * implementation of v4l2_ioctl_ops
 *
 */
static int msm_camera_v4l2_querycap(struct file *f, void *pctx,
				struct v4l2_capability *pcaps)
{

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	/* some other day, some other time */
	/*cap->version = LINUX_VERSION_CODE; */
	pcaps->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	return 0;
}

static int msm_camera_v4l2_queryctrl(struct file *f, void *pctx,
				struct v4l2_queryctrl *pqctrl)
{
	int rc = 0;
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam->vid_lock);
	rc = msm_server_q_ctrl(pcam, pqctrl);
	mutex_unlock(&pcam->vid_lock);
	return rc;
}

static int msm_camera_v4l2_g_ctrl(struct file *f, void *pctx,
					struct v4l2_control *c)
{
	int rc = 0;
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam->vid_lock);
	rc = msm_server_g_ctrl(pcam, c);
	mutex_unlock(&pcam->vid_lock);

	return rc;
}

static int msm_camera_v4l2_s_ctrl(struct file *f, void *pctx,
					struct v4l2_control *ctrl)
{
	int rc = 0;
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);

	WARN_ON(pctx != f->private_data);
	mutex_lock(&pcam->vid_lock);
	switch (ctrl->id) {
	case MSM_V4L2_PID_MMAP_INST:
		D("%s: mmap_inst=(0x%p, %d)\n",
			 __func__, pcam_inst, pcam_inst->my_index);
		pcam_inst->is_mem_map_inst = 1;
		break;
	case MSM_V4L2_PID_MMAP_ENTRY:
		if (copy_from_user(&pcam_inst->mem_map,
			(void *)ctrl->value,
			sizeof(struct msm_mem_map_info))) {
			rc = -EFAULT;
		} else
			D("%s:mmap entry:cookie=0x%x,mem_type=%d,len=%d\n",
				__func__, pcam_inst->mem_map.cookie,
				pcam_inst->mem_map.mem_type,
				pcam_inst->mem_map.length);
		break;
	default:
		if (ctrl->id == MSM_V4L2_PID_CAM_MODE)
			pcam->op_mode = ctrl->value;
		rc = msm_server_s_ctrl(pcam, ctrl);
		break;
	}
	mutex_unlock(&pcam->vid_lock);

	return rc;
}

static int msm_camera_v4l2_reqbufs(struct file *f, void *pctx,
				struct v4l2_requestbuffers *pb)
{
	int rc = 0, i, j;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);
	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam_inst->inst_lock);
	rc = vb2_reqbufs(&pcam_inst->vid_bufq, pb);
	if (rc < 0) {
		pr_err("%s reqbufs failed %d ", __func__, rc);
		mutex_unlock(&pcam_inst->inst_lock);
		return rc;
	}
	if (!pb->count) {
		/* Deallocation. free buf_offset array */
		D("%s Inst %p freeing buffer offsets array",
			__func__, pcam_inst);
		for (j = 0 ; j < pcam_inst->buf_count ; j++) {
			kfree(pcam_inst->buf_offset[j]);
			pcam_inst->buf_offset[j] = NULL;
		}
		kfree(pcam_inst->buf_offset);
		pcam_inst->buf_offset = NULL;
		/* If the userspace has deallocated all the
		 * buffers, then release the vb2 queue */
		if (pcam_inst->vbqueue_initialized) {
			vb2_queue_release(&pcam_inst->vid_bufq);
			pcam_inst->vbqueue_initialized = 0;
		}
	} else {
		D("%s Inst %p Allocating buf_offset array",
			__func__, pcam_inst);
		/* Allocation. allocate buf_offset array */
		pcam_inst->buf_offset = (struct msm_cam_buf_offset **)
			kzalloc(pb->count * sizeof(struct msm_cam_buf_offset *),
							GFP_KERNEL);
		if (!pcam_inst->buf_offset) {
			pr_err("%s out of memory ", __func__);
			mutex_unlock(&pcam_inst->inst_lock);
			return -ENOMEM;
		}
		for (i = 0; i < pb->count; i++) {
			pcam_inst->buf_offset[i] =
				kzalloc(sizeof(struct msm_cam_buf_offset) *
				pcam_inst->plane_info.num_planes, GFP_KERNEL);
			if (!pcam_inst->buf_offset[i]) {
				pr_err("%s out of memory ", __func__);
				for (j = i-1 ; j >= 0; j--) {
					kfree(pcam_inst->buf_offset[j]);
					pcam_inst->buf_offset[j] = NULL;
				}
				kfree(pcam_inst->buf_offset);
				pcam_inst->buf_offset = NULL;
				mutex_unlock(&pcam_inst->inst_lock);
				return -ENOMEM;
			}
		}
	}
	pcam_inst->buf_count = pb->count;
	mutex_unlock(&pcam_inst->inst_lock);
	return rc;
}

static int msm_camera_v4l2_querybuf(struct file *f, void *pctx,
					struct v4l2_buffer *pb)
{
	/* get the video device */
	int rc = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);
	mutex_lock(&pcam_inst->inst_lock);
	rc = vb2_querybuf(&pcam_inst->vid_bufq, pb);
	mutex_unlock(&pcam_inst->inst_lock);
	return rc;
}

static int msm_camera_v4l2_qbuf(struct file *f, void *pctx,
					struct v4l2_buffer *pb)
{
	int rc = 0, i = 0;
	/* get the camera device */
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s Inst=%p, mode=%d, idx=%d\n", __func__, pcam_inst,
		pcam_inst->image_mode, pb->index);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam_inst->inst_lock);
	if (!pcam_inst->buf_offset) {
		pr_err("%s Buffer is already released. Returning. ", __func__);
		mutex_unlock(&pcam_inst->inst_lock);
		return -EINVAL;
	}

	if (pb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* Reject the buffer if planes array was not allocated */
		if (pb->m.planes == NULL) {
			pr_err("%s Planes array is null ", __func__);
			mutex_unlock(&pcam_inst->inst_lock);
			return -EINVAL;
		}
		for (i = 0; i < pcam_inst->plane_info.num_planes; i++) {
			D("%s stored offsets for plane %d as"
				"addr offset %d, data offset %d",
				__func__, i, pb->m.planes[i].reserved[0],
				pb->m.planes[i].data_offset);
			pcam_inst->buf_offset[pb->index][i].data_offset =
				pb->m.planes[i].data_offset;
			pcam_inst->buf_offset[pb->index][i].addr_offset =
				pb->m.planes[i].reserved[0];
		}
	} else {
		D("%s stored reserved info %d", __func__, pb->reserved);
		pcam_inst->buf_offset[pb->index][0].addr_offset = pb->reserved;
	}

	rc = vb2_qbuf(&pcam_inst->vid_bufq, pb);
	D("%s, videobuf_qbuf mode %d and idx %d returns %d\n", __func__,
		pcam_inst->image_mode, pb->index, rc);

	mutex_unlock(&pcam_inst->inst_lock);
	return rc;
}

static int msm_camera_v4l2_dqbuf(struct file *f, void *pctx,
					struct v4l2_buffer *pb)
{
	int rc = 0;
	/* get the camera device */
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam_inst->inst_lock);
	if (0 == pcam_inst->streamon) {
		mutex_unlock(&pcam_inst->inst_lock);
		return -EACCES;
	}

	rc = vb2_dqbuf(&pcam_inst->vid_bufq, pb,  f->f_flags & O_NONBLOCK);
	D("%s, videobuf_dqbuf returns %d\n", __func__, rc);
	mutex_unlock(&pcam_inst->inst_lock);
	return rc;
}

static int msm_camera_v4l2_streamon(struct file *f, void *pctx,
					enum v4l2_buf_type buf_type)
{
	int rc = 0;
	/* get the camera device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s Inst %p\n", __func__, pcam_inst);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam->vid_lock);
	mutex_lock(&pcam_inst->inst_lock);
	if ((buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
		(buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
		pr_err("%s Invalid buffer type ", __func__);
		mutex_unlock(&pcam_inst->inst_lock);
		mutex_unlock(&pcam->vid_lock);
		return -EINVAL;
	}

	D("%s Calling videobuf_streamon", __func__);
	/* if HW streaming on is successful, start buffer streaming */
	rc = vb2_streamon(&pcam_inst->vid_bufq, buf_type);
	D("%s, videobuf_streamon returns %d\n", __func__, rc);

	/* turn HW (VFE/sensor) streaming */
	pcam_inst->streamon = 1;
	rc = msm_server_streamon(pcam, pcam_inst->my_index);
	mutex_unlock(&pcam_inst->inst_lock);
	mutex_unlock(&pcam->vid_lock);
	D("%s rc = %d\n", __func__, rc);
	return rc;
}

static int msm_camera_v4l2_streamoff(struct file *f, void *pctx,
					enum v4l2_buf_type buf_type)
{
	int rc = 0;
	/* get the camera device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s Inst %p\n", __func__, pcam_inst);
	WARN_ON(pctx != f->private_data);

	if ((buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
		(buf_type != V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
		pr_err("%s Invalid buffer type ", __func__);
		return -EINVAL;
	}

	/* first turn of HW (VFE/sensor) streaming so that buffers are
		not in use when we free the buffers */
	mutex_lock(&pcam->vid_lock);
	mutex_lock(&pcam_inst->inst_lock);
	pcam_inst->streamon = 0;
	if (g_server_dev.use_count > 0)
		rc = msm_server_streamoff(pcam, pcam_inst->my_index);
	if (rc < 0)
		pr_err("%s: hw failed to stop streaming\n", __func__);

	/* stop buffer streaming */
	rc = vb2_streamoff(&pcam_inst->vid_bufq, buf_type);
	D("%s, videobuf_streamoff returns %d\n", __func__, rc);
	mutex_unlock(&pcam_inst->inst_lock);
	mutex_unlock(&pcam->vid_lock);
	return rc;
}

static int msm_camera_v4l2_enum_fmt_cap(struct file *f, void *pctx,
					struct v4l2_fmtdesc *pfmtdesc)
{
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	const struct msm_isp_color_fmt *isp_fmt;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);
	if ((pfmtdesc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
		(pfmtdesc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE))
		return -EINVAL;

	if (pfmtdesc->index >= pcam->num_fmts)
		return -EINVAL;

	isp_fmt = &pcam->usr_fmts[pfmtdesc->index];

	if (isp_fmt->name)
		strlcpy(pfmtdesc->description, isp_fmt->name,
						sizeof(pfmtdesc->description));

	pfmtdesc->pixelformat = isp_fmt->fourcc;

	D("%s: [%d] 0x%x, %s\n", __func__, pfmtdesc->index,
		isp_fmt->fourcc, isp_fmt->name);
	return 0;
}

static int msm_camera_v4l2_g_fmt_cap(struct file *f,
		void *pctx, struct v4l2_format *pfmt)
{
	int rc = 0;
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	rc = msm_server_get_fmt(pcam, pcam_inst->my_index, pfmt);
	D("%s: current_fmt->fourcc: 0x%08x, rc = %d\n", __func__,
				pfmt->fmt.pix.pixelformat, rc);
	return rc;
}

static int msm_camera_v4l2_g_fmt_cap_mplane(struct file *f,
		void *pctx, struct v4l2_format *pfmt)
{
	int rc = 0;
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	rc = msm_server_get_fmt_mplane(pcam, pcam_inst->my_index, pfmt);
	D("%s: current_fmt->fourcc: 0x%08x, rc = %d\n", __func__,
					pfmt->fmt.pix_mp.pixelformat, rc);
	return rc;
}

/* This function will readjust the format parameters based in HW
  capabilities. Called by s_fmt_cap
*/
static int msm_camera_v4l2_try_fmt_cap(struct file *f, void *pctx,
					struct v4l2_format *pfmt)
{
	int rc = 0;
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam->vid_lock);
	rc = msm_server_try_fmt(pcam, pfmt);
	if (rc)
		pr_err("Format %x not found, rc = %d\n",
				pfmt->fmt.pix.pixelformat, rc);
	mutex_unlock(&pcam->vid_lock);
	return rc;
}

static int msm_camera_v4l2_try_fmt_cap_mplane(struct file *f, void *pctx,
					struct v4l2_format *pfmt)
{
	int rc = 0;
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam->vid_lock);
	rc = msm_server_try_fmt_mplane(pcam, pfmt);
	if (rc)
		pr_err("Format %x not found, rc = %d\n",
				pfmt->fmt.pix_mp.pixelformat, rc);

	mutex_unlock(&pcam->vid_lock);
	return rc;
}

/* This function will reconfig the v4l2 driver and HW device, it should be
   called after the streaming is stopped.
*/
static int msm_camera_v4l2_s_fmt_cap(struct file *f, void *pctx,
					struct v4l2_format *pfmt)
{
	int rc;
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);
	D("%s, inst=0x%x,idx=%d,priv = 0x%p\n",
		__func__, (u32)pcam_inst, pcam_inst->my_index,
		(void *)pfmt->fmt.pix.priv);
	WARN_ON(pctx != f->private_data);

	if (!pcam_inst->vbqueue_initialized) {
		pcam->mctl.mctl_vbqueue_init(pcam_inst, &pcam_inst->vid_bufq,
					V4L2_BUF_TYPE_VIDEO_CAPTURE);
		pcam_inst->vbqueue_initialized = 1;
	}

	mutex_lock(&pcam->vid_lock);

	rc = msm_server_set_fmt(pcam, pcam_inst->my_index, pfmt);
	if (rc < 0) {
		pr_err("%s: msm_server_set_fmt Error: %d\n",
				__func__, rc);
	}
	mutex_unlock(&pcam->vid_lock);

	return rc;
}

static int msm_camera_v4l2_s_fmt_cap_mplane(struct file *f, void *pctx,
				struct v4l2_format *pfmt)
{
	int rc;
	struct msm_cam_v4l2_device *pcam = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
			struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s Inst %p\n", __func__, pcam_inst);
	WARN_ON(pctx != f->private_data);

	if (!pcam_inst->vbqueue_initialized) {
		pcam->mctl.mctl_vbqueue_init(pcam_inst, &pcam_inst->vid_bufq,
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		pcam_inst->vbqueue_initialized = 1;
	}

	mutex_lock(&pcam->vid_lock);
	rc = msm_server_set_fmt_mplane(pcam, pcam_inst->my_index, pfmt);
	mutex_unlock(&pcam->vid_lock);

	return rc;
}
static int msm_camera_v4l2_g_jpegcomp(struct file *f, void *pctx,
				struct v4l2_jpegcompression *pcomp)
{
	int rc = -EINVAL;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

static int msm_camera_v4l2_s_jpegcomp(struct file *f, void *pctx,
				struct v4l2_jpegcompression *pcomp)
{
	int rc = -EINVAL;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}


static int msm_camera_v4l2_g_crop(struct file *f, void *pctx,
					struct v4l2_crop *crop)
{
	int rc = -EINVAL;
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;

	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	mutex_lock(&pcam->vid_lock);
	rc = msm_camera_get_crop(pcam, pcam_inst->my_index, crop);
	mutex_unlock(&pcam->vid_lock);
	return rc;
}

static int msm_camera_v4l2_s_crop(struct file *f, void *pctx,
					struct v4l2_crop *a)
{
	int rc = -EINVAL;

	D("%s\n", __func__);
	WARN_ON(pctx != f->private_data);

	return rc;
}

/* Stream type-dependent parameter ioctls */
static int msm_camera_v4l2_g_parm(struct file *f, void *pctx,
				struct v4l2_streamparm *a)
{
	int rc = -EINVAL;
	return rc;
}
static int msm_vidbuf_get_path(u32 extendedmode)
{
	switch (extendedmode) {
	case MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL:
		return OUTPUT_TYPE_T;
	case MSM_V4L2_EXT_CAPTURE_MODE_MAIN:
		return OUTPUT_TYPE_S;
	case MSM_V4L2_EXT_CAPTURE_MODE_VIDEO:
		return OUTPUT_TYPE_V;
	case MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT:
	case MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW:
	default:
		return OUTPUT_TYPE_P;
	}
}

static int msm_camera_v4l2_s_parm(struct file *f, void *pctx,
				struct v4l2_streamparm *a)
{
	int rc = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);
	pcam_inst->image_mode = a->parm.capture.extendedmode;
	pcam_inst->pcam->dev_inst_map[pcam_inst->image_mode] = pcam_inst;
	pcam_inst->path = msm_vidbuf_get_path(pcam_inst->image_mode);
	D("%spath=%d,rc=%d\n", __func__,
		pcam_inst->path, rc);
	return rc;
}

static int msm_camera_v4l2_subscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst =
		(struct msm_cam_v4l2_dev_inst *)container_of(fh,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s:fh = 0x%x, type = 0x%x\n", __func__, (u32)fh, sub->type);
	if (pcam_inst->my_index != 0)
		return -EINVAL;
	if (sub->type == V4L2_EVENT_ALL)
		sub->type = V4L2_EVENT_PRIVATE_START+MSM_CAM_APP_NOTIFY_EVENT;
	rc = v4l2_event_subscribe(fh, sub);
	if (rc < 0)
		D("%s: failed for evtType = 0x%x, rc = %d\n",
						__func__, sub->type, rc);
	return rc;
}

static int msm_camera_v4l2_unsubscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst =
		(struct msm_cam_v4l2_dev_inst *)container_of(fh,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("%s: fh = 0x%x\n", __func__, (u32)fh);
	if (pcam_inst->my_index != 0)
		return -EINVAL;

	rc = v4l2_event_unsubscribe(fh, sub);
	D("%s: rc = %d\n", __func__, rc);
	return rc;
}

static int msm_server_v4l2_subscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc = 0;

	D("%s: fh = 0x%x, type = 0x%x", __func__, (u32)fh, sub->type);
	if (sub->type == V4L2_EVENT_ALL) {
		/*sub->type = MSM_ISP_EVENT_START;*/
		sub->type = V4L2_EVENT_PRIVATE_START + MSM_CAM_RESP_CTRL;
		D("sub->type start = 0x%x\n", sub->type);
		do {
			rc = v4l2_event_subscribe(fh, sub);
			if (rc < 0) {
				D("%s: failed for evtType = 0x%x, rc = %d\n",
						__func__, sub->type, rc);
			/* unsubscribe all events here and return */
			sub->type = V4L2_EVENT_ALL;
			v4l2_event_unsubscribe(fh, sub);
			return rc;
			} else
				D("%s: subscribed evtType = 0x%x, rc = %d\n",
						__func__, sub->type, rc);
			sub->type++;
			D("sub->type while = 0x%x\n", sub->type);
		} while (sub->type !=
			V4L2_EVENT_PRIVATE_START + MSM_CAM_RESP_MAX);
	} else {
		D("sub->type not V4L2_EVENT_ALL = 0x%x\n", sub->type);
		rc = v4l2_event_subscribe(fh, sub);
		if (rc < 0)
			D("%s: failed for evtType = 0x%x, rc = %d\n",
						__func__, sub->type, rc);
	}

	D("%s: rc = %d\n", __func__, rc);
	return rc;
}

static int msm_server_v4l2_unsubscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc = 0;

	D("%s: fh = 0x%x\n", __func__, (u32)fh);
	rc = v4l2_event_unsubscribe(fh, sub);
	D("%s: rc = %d\n", __func__, rc);
	return rc;
}

/* v4l2_ioctl_ops */
static const struct v4l2_ioctl_ops g_msm_ioctl_ops = {
	.vidioc_querycap = msm_camera_v4l2_querycap,

	.vidioc_s_crop = msm_camera_v4l2_s_crop,
	.vidioc_g_crop = msm_camera_v4l2_g_crop,

	.vidioc_queryctrl = msm_camera_v4l2_queryctrl,
	.vidioc_g_ctrl = msm_camera_v4l2_g_ctrl,
	.vidioc_s_ctrl = msm_camera_v4l2_s_ctrl,

	.vidioc_reqbufs = msm_camera_v4l2_reqbufs,
	.vidioc_querybuf = msm_camera_v4l2_querybuf,
	.vidioc_qbuf = msm_camera_v4l2_qbuf,
	.vidioc_dqbuf = msm_camera_v4l2_dqbuf,

	.vidioc_streamon = msm_camera_v4l2_streamon,
	.vidioc_streamoff = msm_camera_v4l2_streamoff,

	/* format ioctls */
	.vidioc_enum_fmt_vid_cap = msm_camera_v4l2_enum_fmt_cap,
	.vidioc_enum_fmt_vid_cap_mplane = msm_camera_v4l2_enum_fmt_cap,
	.vidioc_try_fmt_vid_cap = msm_camera_v4l2_try_fmt_cap,
	.vidioc_try_fmt_vid_cap_mplane = msm_camera_v4l2_try_fmt_cap_mplane,
	.vidioc_g_fmt_vid_cap = msm_camera_v4l2_g_fmt_cap,
	.vidioc_g_fmt_vid_cap_mplane = msm_camera_v4l2_g_fmt_cap_mplane,
	.vidioc_s_fmt_vid_cap = msm_camera_v4l2_s_fmt_cap,
	.vidioc_s_fmt_vid_cap_mplane = msm_camera_v4l2_s_fmt_cap_mplane,

	.vidioc_g_jpegcomp = msm_camera_v4l2_g_jpegcomp,
	.vidioc_s_jpegcomp = msm_camera_v4l2_s_jpegcomp,

	/* Stream type-dependent parameter ioctls */
	.vidioc_g_parm =  msm_camera_v4l2_g_parm,
	.vidioc_s_parm =  msm_camera_v4l2_s_parm,

	/* event subscribe/unsubscribe */
	.vidioc_subscribe_event = msm_camera_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = msm_camera_v4l2_unsubscribe_event,
};

/* open an active camera session to manage the streaming logic */
static int msm_cam_server_open_session(struct msm_cam_server_dev *ps,
	struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	D("%s\n", __func__);

	if (!ps || !pcam) {
		pr_err("%s NULL pointer passed in!\n", __func__);
		return rc;
	}

	/* The number of camera instance should be controlled by the
		resource manager. Currently supporting one active instance
		until multiple instances are supported */
	if (atomic_read(&ps->number_pcam_active) > 0) {
		pr_err("%s Cannot have more than one active camera %d\n",
			__func__, atomic_read(&ps->number_pcam_active));
		return -EINVAL;
	}
	/* book keeping this camera session*/
	ps->pcam_active = pcam;
	atomic_inc(&ps->number_pcam_active);

	D("config pcam = 0x%p\n", ps->pcam_active);

	/* initialization the media controller module*/
	msm_mctl_init_module(pcam);

	/*yyan: for single VFE msms (8660, 8960v1), just populate the session
	with our VFE devices that registered*/
	pcam->mctl.isp_sdev = ps->isp_subdev[0];


	/*yyan: 8960 bring up - no VPE and flash; populate later*/
	pcam->mctl.vpe_sdev = NULL;
	pcam->mctl.flash_sdev = NULL;

	return rc;

}

/* close an active camera session to server */
static int msm_cam_server_close_session(struct msm_cam_server_dev *ps,
	struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	D("%s\n", __func__);

	if (!ps || !pcam) {
		D("%s NULL pointer passed in!\n", __func__);
		return rc;
	}


	atomic_dec(&ps->number_pcam_active);
	ps->pcam_active = NULL;

	return rc;
}
/* v4l2_file_operations */
static int msm_open(struct file *f)
{
	int i;
	int rc = -EINVAL;
	int ion_client_created = 0;
	/*struct msm_isp_ops *p_isp = 0;*/
	/* get the video device */
	struct msm_cam_v4l2_device *pcam  = video_drvdata(f);
	struct msm_cam_v4l2_dev_inst *pcam_inst;

	D("%s\n", __func__);

	if (!pcam) {
		pr_err("%s NULL pointer passed in!\n", __func__);
		return rc;
	}
	mutex_lock(&pcam->vid_lock);
	for (i = 0; i < MSM_DEV_INST_MAX; i++) {
		if (pcam->dev_inst[i] == NULL)
			break;
	}
	/* if no instance is available, return error */
	if (i == MSM_DEV_INST_MAX) {
		mutex_unlock(&pcam->vid_lock);
		return rc;
	}
	pcam_inst = kzalloc(sizeof(struct msm_cam_v4l2_dev_inst), GFP_KERNEL);
	if (!pcam_inst) {
		mutex_unlock(&pcam->vid_lock);
		return rc;
	}
	mutex_init(&pcam_inst->inst_lock);
	pcam_inst->sensor_pxlcode = pcam->usr_fmts[0].pxlcode;
	pcam_inst->my_index = i;
	pcam_inst->pcam = pcam;
	pcam->dev_inst[i] = pcam_inst;

	D("%s index %d nodeid %d count %d\n", __func__,
			pcam_inst->my_index,
			pcam->vnode_id, pcam->use_count);
	pcam->use_count++;
	if (pcam->use_count == 1) {

		rc = msm_cam_server_open_session(&g_server_dev, pcam);
		if (rc < 0) {
			pr_err("%s: cam_server_open_session failed %d\n",
			__func__, rc);
			goto msm_cam_server_open_session_failed;
		}
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		pcam->mctl.client = msm_ion_client_create(-1, "camera");
		kref_init(&pcam->mctl.refcount);
		ion_client_created = 1;
#endif
		/* Should be set to sensor ops if any but right now its OK!! */
		if (!pcam->mctl.mctl_open) {
			pr_err("%s: media contoller is not inited\n",
				 __func__);
			rc = -ENODEV;
			goto mctl_open_failed;
		}

		/* Now we really have to activate the camera */
		D("%s: call mctl_open\n", __func__);
		rc = pcam->mctl.mctl_open(&(pcam->mctl), MSM_APPS_ID_V4L2);

		if (rc < 0) {
			pr_err("%s: HW open failed rc = 0x%x\n",  __func__, rc);
			goto mctl_open_failed;
		}
		pcam->mctl.sync.pcam_sync = pcam;

		/* Register isp subdev */
		rc = v4l2_device_register_subdev(&pcam->v4l2_dev,
					pcam->mctl.isp_sdev->sd);
		if (rc < 0) {
			pr_err("%s: v4l2_device_register_subdev failed rc = %d\n",
				__func__, rc);
			goto mctl_register_isp_sd_failed;
		}
		if (pcam->mctl.isp_sdev->sd_vpe) {
			rc = v4l2_device_register_subdev(&pcam->v4l2_dev,
						pcam->mctl.isp_sdev->sd_vpe);
			if (rc < 0) {
				goto mctl_register_isp_sd_vpe_failed;
			}
		}
		rc = msm_setup_v4l2_event_queue(&pcam_inst->eventHandle,
							pcam->pvdev);
		if (rc < 0) {
			pr_err("%s: msm_setup_v4l2_event_queue failed %d",
				 __func__, rc);
			goto mctl_event_q_setup_failed;
		}
	}
	pcam_inst->vbqueue_initialized = 0;
	rc = 0;

	f->private_data = &pcam_inst->eventHandle;

	D("f->private_data = 0x%x, pcam = 0x%x\n",
		(u32)f->private_data, (u32)pcam_inst);


	if (pcam->use_count == 1) {
		msm_queue_init(&g_server_dev.ctrl_q, "control");
		rc = msm_send_open_server(pcam->vnode_id);
		if (rc < 0) {
			pr_err("%s send open server failed\n", __func__);
			goto msm_send_open_server_failed;
		}
	}
	mutex_unlock(&pcam->vid_lock);
	D("%s: end", __func__);
	/* rc = msm_cam_server_open_session(g_server_dev, pcam);*/
	return rc;

msm_send_open_server_failed:
	v4l2_fh_del(&pcam_inst->eventHandle);
	v4l2_fh_exit(&pcam_inst->eventHandle);

mctl_event_q_setup_failed:
	v4l2_device_unregister_subdev(pcam->mctl.isp_sdev->sd_vpe);
mctl_register_isp_sd_vpe_failed:
	v4l2_device_unregister_subdev(pcam->mctl.isp_sdev->sd);
mctl_register_isp_sd_failed:
	if (pcam->mctl.mctl_release)
		if (pcam->mctl.mctl_release(&(pcam->mctl)) < 0)
			pr_err("%s: mctl_release failed\n", __func__);

mctl_open_failed:
	if (pcam->use_count == 1) {
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		if (ion_client_created) {
			pr_err("%s: destroy ion client", __func__);
			kref_put(&pcam->mctl.refcount, msm_release_ion_client);
		}
#endif
		if (msm_cam_server_close_session(&g_server_dev, pcam) < 0)
			pr_err("%s: msm_cam_server_close_session failed\n",
				__func__);
	}

msm_cam_server_open_session_failed:
	if (pcam->use_count == 1) {
		pcam->dev_inst[i] = NULL;
		pcam->use_count = 0;
	}
	pcam->dev_inst[i] = NULL;
	mutex_unlock(&pcam->vid_lock);
	kfree(pcam_inst);
	return rc;
}

static int msm_addr_remap(struct msm_cam_v4l2_dev_inst *pcam_inst,
				struct vm_area_struct *vma)
{
	int phyaddr;
	int retval;
	unsigned long size;
	struct msm_sync *sync = &pcam_inst->pcam->mctl.sync;
	int rc = 0;

	rc = msm_pmem_region_get_phy_addr(&sync->pmem_stats,
			&pcam_inst->mem_map,
			&phyaddr);
	if (rc) {
		pr_err("%s: cannot map vaddr", __func__);
		return -EFAULT;
	}
	size = vma->vm_end - vma->vm_start;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	retval = remap_pfn_range(vma, vma->vm_start,
			phyaddr >> PAGE_SHIFT,
			size, vma->vm_page_prot);
	if (retval) {
		pr_err("%s:mmap: remap failed with error %d. ",
			   __func__, retval);
		memset(&pcam_inst->mem_map, 0, sizeof(pcam_inst->mem_map));
		return -ENOMEM;
	}
	D("%s:mmap: phy_addr=0x%x: %08lx-%08lx, pgoff %08lx\n",
		   __func__, (uint32_t)phyaddr,
		   vma->vm_start, vma->vm_end, vma->vm_pgoff);
	memset(&pcam_inst->mem_map, 0, sizeof(pcam_inst->mem_map));
	return 0;
}

static int msm_mmap(struct file *f, struct vm_area_struct *vma)
{
	int rc = 0;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);

	D("mmap called, vma=0x%08lx\n", (unsigned long)vma);

	if (pcam_inst->is_mem_map_inst &&
		pcam_inst->mem_map.cookie) {
		rc = msm_addr_remap(pcam_inst, vma);
		D("%s: msm_addr_remap ret=%d\n", __func__, rc);
		return rc;
	} else
		rc = vb2_mmap(&pcam_inst->vid_bufq, vma);
	D("vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end - (unsigned long)vma->vm_start,
		rc);

	return rc;
}

void msm_release_ion_client(struct kref *ref)
{
	struct msm_cam_media_controller *mctl = container_of(ref,
			struct msm_cam_media_controller, refcount);
	pr_err("%s Calling ion_client_destroy ", __func__);
	ion_client_destroy(mctl->client);
}

static int msm_close(struct file *f)
{
	int rc = 0;
	struct msm_cam_v4l2_device *pcam;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);
	pcam = pcam_inst->pcam;
	if (!pcam) {
		pr_err("%s NULL pointer of camera device!\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pcam->vid_lock);
	mutex_lock(&pcam_inst->inst_lock);

	if (pcam_inst->streamon) {
		/*something went wrong since instance
		is closing without streamoff*/
		if (pcam->mctl.mctl_release) {
			rc = pcam->mctl.mctl_release(&(pcam->mctl));
			if (rc < 0)
				pr_err("mctl_release fails %d\n", rc);
		}
		pcam->mctl.mctl_release = NULL;/*so that it isn't closed again*/
	}

	pcam_inst->streamon = 0;
	pcam->use_count--;
	pcam->dev_inst_map[pcam_inst->image_mode] = NULL;
	if (pcam_inst->vbqueue_initialized)
		vb2_queue_release(&pcam_inst->vid_bufq);
	D("%s Closing down instance %p ", __func__, pcam_inst);
	D("%s index %d nodeid %d count %d\n", __func__, pcam_inst->my_index,
		pcam->vnode_id, pcam->use_count);
	pcam->dev_inst[pcam_inst->my_index] = NULL;
	if (pcam_inst->my_index == 0) {
		v4l2_fh_del(&pcam_inst->eventHandle);
		v4l2_fh_exit(&pcam_inst->eventHandle);
	}

	mutex_unlock(&pcam_inst->inst_lock);
	mutex_destroy(&pcam_inst->inst_lock);
	kfree(pcam_inst);
	f->private_data = NULL;

	if (pcam->use_count == 0) {
		v4l2_device_unregister_subdev(pcam->mctl.isp_sdev->sd);
		v4l2_device_unregister_subdev(pcam->mctl.isp_sdev->sd_vpe);
		rc = msm_cam_server_close_session(&g_server_dev, pcam);
		if (rc < 0)
			pr_err("msm_cam_server_close_session fails %d\n", rc);

		if (g_server_dev.use_count > 0) {
			rc = msm_send_close_server(pcam->vnode_id);
			if (rc < 0)
				pr_err("msm_send_close_server failed %d\n", rc);
		}
		if (pcam->mctl.mctl_release) {
			rc = pcam->mctl.mctl_release(&(pcam->mctl));
			if (rc < 0)
				pr_err("mctl_release fails %d\n", rc);
		}
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		kref_put(&pcam->mctl.refcount, msm_release_ion_client);
#endif
		if (g_server_dev.use_count == 0)
			mutex_unlock(&g_server_dev.server_lock);
	}
	mutex_unlock(&pcam->vid_lock);
	return rc;
}

static unsigned int msm_poll(struct file *f, struct poll_table_struct *wait)
{
	int rc = 0;
	struct msm_cam_v4l2_device *pcam;
	struct msm_cam_v4l2_dev_inst *pcam_inst;
	pcam_inst = container_of(f->private_data,
		struct msm_cam_v4l2_dev_inst, eventHandle);
	pcam = pcam_inst->pcam;
	D("%s\n", __func__);
	if (!pcam) {
		pr_err("%s NULL pointer of camera device!\n", __func__);
		return -EINVAL;
	}
	if (pcam_inst->my_index == 0) {
		poll_wait(f, &(pcam_inst->eventHandle.events->wait), wait);
		if (v4l2_event_pending(&pcam_inst->eventHandle))
			rc |= POLLPRI;
	} else {
		if (!pcam_inst->vid_bufq.streaming) {
			D("%s vid_bufq.streaming is off, inst=0x%x\n",
			__func__, (u32)pcam_inst);
			return -EINVAL;
		}
		rc |= vb2_poll(&pcam_inst->vid_bufq, f, wait);
	}
	D("%s returns, rc  = 0x%x\n", __func__, rc);
	return rc;
}

static unsigned int msm_poll_server(struct file *fp,
					struct poll_table_struct *wait)
{
	int rc = 0;

	D("%s\n", __func__);
	poll_wait(fp,
		 &g_server_dev.server_command_queue.eventHandle.events->wait,
		 wait);
	if (v4l2_event_pending(&g_server_dev.server_command_queue.eventHandle))
		rc |= POLLPRI;

	return rc;
}
static long msm_ioctl_server(struct file *fp, unsigned int cmd,
	unsigned long arg)
{
	int rc = -EINVAL;
	struct v4l2_event ev;
	struct msm_camera_info temp_cam_info;
	struct msm_cam_config_dev_info temp_config_info;
	struct msm_mctl_node_info temp_mctl_info;
	struct v4l2_event_subscription temp_sub;
	int i;

	D("%s: cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case MSM_CAM_IOCTL_GET_CAMERA_INFO:
		if (copy_from_user(&temp_cam_info, (void __user *)arg,
					  sizeof(struct msm_camera_info))) {
			rc = -EINVAL;
			return rc;
		}
		for (i = 0; i < g_server_dev.camera_info.num_cameras; i++) {
			if (copy_to_user((void __user *)
			temp_cam_info.video_dev_name[i],
			 g_server_dev.camera_info.video_dev_name[i],
			strlen(g_server_dev.camera_info.video_dev_name[i]))) {
				rc = -EINVAL;
				return rc;
			}
			temp_cam_info.has_3d_support[i] =
				g_server_dev.camera_info.has_3d_support[i];
			temp_cam_info.is_internal_cam[i] =
				g_server_dev.camera_info.is_internal_cam[i];
			temp_cam_info.s_mount_angle[i] =
				g_server_dev.camera_info.s_mount_angle[i];
			temp_cam_info.sensor_type[i] =
				g_server_dev.camera_info.sensor_type[i];

		}
		temp_cam_info.num_cameras =
			g_server_dev.camera_info.num_cameras;
		if (copy_to_user((void __user *)arg,
							  &temp_cam_info,
				sizeof(struct msm_camera_info))) {
			rc = -EINVAL;
			return rc;
		}
		rc = 0;
		break;

	case MSM_CAM_IOCTL_GET_CONFIG_INFO:
		if (copy_from_user(&temp_config_info, (void __user *)arg,
				  sizeof(struct msm_cam_config_dev_info))) {
			rc = -EINVAL;
			return rc;
		}
		for (i = 0;
		 i < g_server_dev.config_info.num_config_nodes; i++) {
			if (copy_to_user(
			(void __user *)temp_config_info.config_dev_name[i],
			g_server_dev.config_info.config_dev_name[i],
			strlen(g_server_dev.config_info.config_dev_name[i]))) {
				rc = -EINVAL;
				return rc;
			}
		}
		temp_config_info.num_config_nodes =
			g_server_dev.config_info.num_config_nodes;
		if (copy_to_user((void __user *)arg,
							  &temp_config_info,
				sizeof(struct msm_cam_config_dev_info))) {
			rc = -EINVAL;
			return rc;
		}
		rc = 0;
		break;
	case MSM_CAM_IOCTL_GET_MCTL_INFO:
		if (copy_from_user(&temp_mctl_info, (void __user *)arg,
				  sizeof(struct msm_mctl_node_info))) {
			rc = -EINVAL;
			return rc;
		}

		for (i = 0; i < g_server_dev.mctl_node_info.num_mctl_nodes;
				i++) {
			if (copy_to_user((void __user *)
			temp_mctl_info.mctl_node_name[i],
			g_server_dev.mctl_node_info.mctl_node_name[i], strnlen(
			g_server_dev.mctl_node_info.mctl_node_name[i],
			MAX_DEV_NAME_LEN))) {
				rc = -EINVAL;
				return rc;
			}
		}
		temp_mctl_info.num_mctl_nodes =
			g_server_dev.mctl_node_info.num_mctl_nodes;
		if (copy_to_user((void __user *)arg,
							  &temp_mctl_info,
				sizeof(struct msm_mctl_node_info))) {
			rc = -EINVAL;
			return rc;
		}
		rc = 0;
	break;

	case VIDIOC_SUBSCRIBE_EVENT:
		if (copy_from_user(&temp_sub, (void __user *)arg,
				  sizeof(struct v4l2_event_subscription))) {
			rc = -EINVAL;
			return rc;
		}
		rc = msm_server_v4l2_subscribe_event
			(&g_server_dev.server_command_queue.eventHandle,
			 &temp_sub);
		if (rc < 0)
			return rc;

		break;

	case VIDIOC_DQEVENT: {
		void __user *u_ctrl_value = NULL, *user_ptr = NULL;
		struct msm_isp_event_ctrl u_isp_event;
		struct msm_isp_event_ctrl *k_isp_event;

		/* First, copy the event structure from userspace */
		D("%s: VIDIOC_DQEVENT\n", __func__);
		if (copy_from_user(&ev, (void __user *)arg,
				sizeof(struct v4l2_event))) {
			break;
		}
		/* Next, get the pointer to event_ctrl structure
		 * embedded inside the v4l2_event.u.data array. */
		user_ptr = (void __user *)(*((uint32_t *)ev.u.data));

		/* Next, copy the userspace event ctrl structure */
		if (copy_from_user((void *)&u_isp_event, user_ptr,
				sizeof(struct msm_isp_event_ctrl))) {
			rc = -EFAULT;
			break;
		}

		/* Save the pointer of the user allocated command buffer*/
		u_ctrl_value = u_isp_event.isp_data.ctrl.value;

		/* Dequeue the event queued into the v4l2 queue*/
		rc = v4l2_event_dequeue(
			&g_server_dev.server_command_queue.eventHandle,
			&ev, fp->f_flags & O_NONBLOCK);
		if (rc < 0) {
			pr_err("no pending events?");
			rc = -EFAULT;
			break;
		}

		/* Use k_isp_event to point to the event_ctrl structure
		 * embedded inside v4l2_event.u.data */
		k_isp_event = (struct msm_isp_event_ctrl *)
				(*((uint32_t *)ev.u.data));
		if (!k_isp_event) {
			pr_err("%s Invalid event ctrl structure. ", __func__);
			rc = -EFAULT;
			break;
		}
		if (k_isp_event->evt_id != g_server_dev.server_evt_id) {
			pr_err("%s: isp_event->evt_id (0x%x) neq cur_server_evt_id (0x%x)",
				   __func__, k_isp_event->evt_id, g_server_dev.server_evt_id);
			kfree(k_isp_event->isp_data.ctrl.value);
			k_isp_event->isp_data.ctrl.value = NULL;
			kfree(k_isp_event);
			k_isp_event = NULL;
			rc = -EFAULT;
			break;
		}
		/* Copy the event structure into user struct*/
		u_isp_event = *k_isp_event;

		/* Restore the saved pointer of the user
		 * allocated command buffer. */
		u_isp_event.isp_data.ctrl.value = u_ctrl_value;
		if (ev.type == V4L2_EVENT_PRIVATE_START+MSM_CAM_RESP_V4L2) {
			/* Copy the ctrl cmd, if present*/
			if (k_isp_event->isp_data.ctrl.length > 0 &&
				k_isp_event->isp_data.ctrl.value != NULL) {
				void *k_ctrl_value =
					k_isp_event->isp_data.ctrl.value;
				if (copy_to_user(u_ctrl_value, k_ctrl_value,
					k_isp_event->isp_data.ctrl.length)) {
					kfree(k_isp_event->isp_data.ctrl.value);
					k_isp_event->isp_data.ctrl.value = NULL;
					kfree(k_isp_event);
					k_isp_event = NULL;
					rc = -EINVAL;
					break;
				}
				kfree(k_isp_event->isp_data.ctrl.value);
				k_isp_event->isp_data.ctrl.value = NULL;
			}
			/* Copy the event ctrl structure back into
			 * user's structure. */
			if (copy_to_user(user_ptr, (void *)&u_isp_event,
					 sizeof(struct msm_isp_event_ctrl))) {
				kfree(k_isp_event);
				k_isp_event = NULL;
				rc = -EINVAL;
				break;
			}
		}

		/* Copy the v4l2_event structure back to the user*/
		if (copy_to_user((void __user *)arg, &ev,
				sizeof(struct v4l2_event))) {
			kfree(k_isp_event);
			k_isp_event = NULL;
			rc = -EINVAL;
			break;
		}
		kfree(k_isp_event);
		k_isp_event = NULL;
		break;
	}
	case MSM_CAM_IOCTL_CTRL_CMD_DONE:
		D("%s: MSM_CAM_IOCTL_CTRL_CMD_DONE\n", __func__);
		rc = msm_ctrl_cmd_done((void __user *)arg);
		break;

	default:
		break;
	}
	return rc;
}

static int msm_open_server(struct inode *inode, struct file *fp)
{
	int rc;
	D("%s: open %s\n", __func__, fp->f_path.dentry->d_name.name);
	mutex_lock(&g_server_dev.server_lock);
	rc = nonseekable_open(inode, fp);
	if (rc < 0) {
		pr_err("%s: nonseekable_open error %d\n", __func__, rc);
		mutex_unlock(&g_server_dev.server_lock);
		return rc;
	}
	g_server_dev.use_count++;
	if (g_server_dev.use_count == 1)
		msm_queue_init(&g_server_dev.ctrl_q, "control");
	mutex_unlock(&g_server_dev.server_lock);
	return rc;
}

static unsigned int msm_poll_config(struct file *fp,
					struct poll_table_struct *wait)
{
	int rc = 0;
	struct msm_cam_config_dev *config = fp->private_data;
	if (config == NULL)
		return -EINVAL;

	D("%s\n", __func__);

	poll_wait(fp,
	&config->config_stat_event_queue.eventHandle.events->wait, wait);
	if (v4l2_event_pending(&config->config_stat_event_queue.eventHandle))
		rc |= POLLPRI;
	return rc;
}

static int msm_close_server(struct inode *inode, struct file *fp)
{
	struct v4l2_event_subscription sub;
	D("%s\n", __func__);
	mutex_lock(&g_server_dev.server_lock);
	if (g_server_dev.use_count > 0)
		g_server_dev.use_count--;
	mutex_unlock(&g_server_dev.server_lock);
	if (g_server_dev.use_count == 0) {
		if (g_server_dev.pcam_active) {
			struct v4l2_event v4l2_ev;
			mutex_lock(&g_server_dev.server_lock);

			v4l2_ev.type = V4L2_EVENT_PRIVATE_START
				+ MSM_CAM_APP_NOTIFY_ERROR_EVENT;
			ktime_get_ts(&v4l2_ev.timestamp);
			v4l2_event_queue(
				g_server_dev.pcam_active->pvdev, &v4l2_ev);
		}
	sub.type = V4L2_EVENT_ALL;
	msm_server_v4l2_unsubscribe_event(
		&g_server_dev.server_command_queue.eventHandle, &sub);
	}
	return 0;
}


static long msm_v4l2_evt_notify(struct msm_cam_media_controller *mctl,
		unsigned int cmd, unsigned long evt)
{
	struct v4l2_event v4l2_ev;
	struct msm_cam_v4l2_device *pcam = NULL;

	if (!mctl) {
		pr_err("%s: mctl is NULL\n", __func__);
		return -EINVAL;
	}

	if (copy_from_user(&v4l2_ev, (void __user *)evt,
		sizeof(struct v4l2_event))) {
		ERR_COPY_FROM_USER();
		return -EFAULT;
	}

	pcam = mctl->sync.pcam_sync;
	ktime_get_ts(&v4l2_ev.timestamp);
	v4l2_event_queue(pcam->pvdev, &v4l2_ev);
	return 0;
}

static long msm_ioctl_config(struct file *fp, unsigned int cmd,
	unsigned long arg)
{

	int rc = 0;
	struct v4l2_event ev;
	struct msm_cam_config_dev *config_cam = fp->private_data;
	struct v4l2_event_subscription temp_sub;

	D("%s: cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
		/* memory management shall be handeld here*/
	case MSM_CAM_IOCTL_REGISTER_PMEM:
		return msm_register_pmem(
			&config_cam->p_mctl->sync.pmem_stats,
			(void __user *)arg, config_cam->p_mctl->client);
		break;

	case MSM_CAM_IOCTL_UNREGISTER_PMEM:
		return msm_pmem_table_del(
			&config_cam->p_mctl->sync.pmem_stats,
			(void __user *)arg, config_cam->p_mctl->client);
		break;
	case VIDIOC_SUBSCRIBE_EVENT:
		if (copy_from_user(&temp_sub,
			(void __user *)arg,
			sizeof(struct v4l2_event_subscription))) {
				rc = -EINVAL;
				return rc;
		}
		rc = msm_server_v4l2_subscribe_event
			(&config_cam->config_stat_event_queue.eventHandle,
				 &temp_sub);
		if (rc < 0)
			return rc;
		break;

	case VIDIOC_UNSUBSCRIBE_EVENT:
		if (copy_from_user(&temp_sub, (void __user *)arg,
			  sizeof(struct v4l2_event_subscription))) {
			rc = -EINVAL;
			return rc;
		}
		rc = msm_server_v4l2_unsubscribe_event
			(&config_cam->config_stat_event_queue.eventHandle,
			 &temp_sub);
		if (rc < 0)
			return rc;
		break;

	case VIDIOC_DQEVENT: {
		void __user *u_msg_value = NULL, *user_ptr = NULL;
		struct msm_isp_event_ctrl u_isp_event;
		struct msm_isp_event_ctrl *k_isp_event;

		/* First, copy the v4l2 event structure from userspace */
		D("%s: VIDIOC_DQEVENT\n", __func__);
		if (copy_from_user(&ev, (void __user *)arg,
				sizeof(struct v4l2_event)))
			break;
		/* Next, get the pointer to event_ctrl structure
		 * embedded inside the v4l2_event.u.data array. */
		user_ptr = (void __user *)(*((uint32_t *)ev.u.data));

		/* Next, copy the userspace event ctrl structure */
		if (copy_from_user((void *)&u_isp_event, user_ptr,
				   sizeof(struct msm_isp_event_ctrl))) {
			break;
		}
		/* Save the pointer of the user allocated command buffer*/
		u_msg_value = u_isp_event.isp_data.isp_msg.data;

		/* Dequeue the event queued into the v4l2 queue*/
		rc = v4l2_event_dequeue(
			&config_cam->config_stat_event_queue.eventHandle,
			&ev, fp->f_flags & O_NONBLOCK);
		if (rc < 0) {
			pr_err("no pending events?");
			break;
		}
		/* Use k_isp_event to point to the event_ctrl structure
		 * embedded inside v4l2_event.u.data */
		k_isp_event = (struct msm_isp_event_ctrl *)
				(*((uint32_t *)ev.u.data));
		/* Copy the event structure into user struct. */
		u_isp_event = *k_isp_event;
		if (ev.type != (V4L2_EVENT_PRIVATE_START +
				MSM_CAM_RESP_DIV_FRAME_EVT_MSG) &&
				ev.type != (V4L2_EVENT_PRIVATE_START +
				MSM_CAM_RESP_MCTL_PP_EVENT)) {

			/* Restore the saved pointer of the
			 * user allocated command buffer. */
			u_isp_event.isp_data.isp_msg.data = u_msg_value;

			if (ev.type == (V4L2_EVENT_PRIVATE_START +
					MSM_CAM_RESP_STAT_EVT_MSG)) {
				if (k_isp_event->isp_data.isp_msg.len > 0) {
					void *k_msg_value =
					k_isp_event->isp_data.isp_msg.data;
					if (copy_to_user(u_msg_value,
							k_msg_value,
					 k_isp_event->isp_data.isp_msg.len)) {
						rc = -EINVAL;
						break;
					}
					kfree(k_msg_value);
					k_msg_value = NULL;
				}
			}
		}
		/* Copy the event ctrl structure back
		 * into user's structure. */
		if (copy_to_user(user_ptr,
				(void *)&u_isp_event, sizeof(
				struct msm_isp_event_ctrl))) {
			rc = -EINVAL;
			break;
		}
		kfree(k_isp_event);
		k_isp_event = NULL;
		/* Copy the v4l2_event structure back to the user*/
		if (copy_to_user((void __user *)arg, &ev,
				sizeof(struct v4l2_event))) {
			rc = -EINVAL;
			break;
		}
		}

		break;

	case MSM_CAM_IOCTL_V4L2_EVT_NOTIFY:
		rc = msm_v4l2_evt_notify(config_cam->p_mctl, cmd, arg);
		break;

	case MSM_CAM_IOCTL_SET_MEM_MAP_INFO:
		if (copy_from_user(&config_cam->mem_map, (void __user *)arg,
				sizeof(struct msm_mem_map_info)))
			rc = -EINVAL;
		break;

	default:{
		/* For the rest of config command, forward to media controller*/
		struct msm_cam_media_controller *p_mctl = config_cam->p_mctl;
		if (p_mctl && p_mctl->mctl_cmd) {
			rc = config_cam->p_mctl->mctl_cmd(p_mctl, cmd, arg);
		} else {
			rc = -EINVAL;
			pr_err("%s: media controller is null\n", __func__);
		}

		break;
	} /* end of default*/
	} /* end of switch*/
	return rc;
}

static int msm_mmap_config(struct file *fp, struct vm_area_struct *vma)
{
	struct msm_cam_config_dev *config_cam = fp->private_data;
	int rc = 0;
	int phyaddr;
	int retval;
	unsigned long size;

	D("%s: phy_addr=0x%x", __func__, config_cam->mem_map.cookie);
	phyaddr = (int)config_cam->mem_map.cookie;
	if (!phyaddr) {
		pr_err("%s: no physical memory to map", __func__);
		return -EFAULT;
	}
	memset(&config_cam->mem_map, 0,
		sizeof(struct msm_mem_map_info));
	size = vma->vm_end - vma->vm_start;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	retval = remap_pfn_range(vma, vma->vm_start,
					phyaddr >> PAGE_SHIFT,
					size, vma->vm_page_prot);
	if (retval) {
		pr_err("%s: remap failed, rc = %d",
					__func__, retval);
		rc = -ENOMEM;
		goto end;
	}
	D("%s: phy_addr=0x%x: %08lx-%08lx, pgoff %08lx\n",
			__func__, (uint32_t)phyaddr,
			vma->vm_start, vma->vm_end, vma->vm_pgoff);
end:
	return rc;
}

static int msm_open_config(struct inode *inode, struct file *fp)
{
	int rc;

	struct msm_cam_config_dev *config_cam =
	container_of(inode->i_cdev, struct msm_cam_config_dev, config_cdev);

	D("%s: open %s\n", __func__, fp->f_path.dentry->d_name.name);

	rc = nonseekable_open(inode, fp);
	if (rc < 0) {
		pr_err("%s: nonseekable_open error %d\n", __func__, rc);
		return rc;
	}
	config_cam->use_count++;

	/*config_cam->isp_subdev = g_server_dev.pcam_active->mctl.isp_sdev;*/
	/* assume there is only one active camera possible*/
	config_cam->p_mctl = &g_server_dev.pcam_active->mctl;

	INIT_HLIST_HEAD(&config_cam->p_mctl->sync.pmem_stats);
	spin_lock_init(&config_cam->p_mctl->sync.pmem_stats_spinlock);

	config_cam->p_mctl->config_device = config_cam;
	kref_get(&config_cam->p_mctl->refcount);
	fp->private_data = config_cam;
	return rc;
}

static int msm_close_config(struct inode *node, struct file *f)
{
	struct msm_cam_config_dev *config_cam = f->private_data;
	D("%s Decrementing ref count of config node ", __func__);
	kref_put(&config_cam->p_mctl->refcount, msm_release_ion_client);
	return 0;
}

static struct v4l2_file_operations g_msm_fops = {
	.owner   = THIS_MODULE,
	.open	= msm_open,
	.poll	= msm_poll,
	.mmap	= msm_mmap,
	.release = msm_close,
	.ioctl   = video_ioctl2,
};

/* Init a config node for ISP control,
 * which will create a config device (/dev/config0/ and plug in
 * ISP's operation "v4l2_ioctl_ops*"
 */
static const struct file_operations msm_fops_server = {
	.owner = THIS_MODULE,
	.open  = msm_open_server,
	.poll  = msm_poll_server,
	.unlocked_ioctl = msm_ioctl_server,
	.release = msm_close_server,
};

static const struct file_operations msm_fops_config = {
	.owner = THIS_MODULE,
	.open  = msm_open_config,
	.poll  = msm_poll_config,
	.unlocked_ioctl = msm_ioctl_config,
	.mmap	= msm_mmap_config,
	.release = msm_close_config,
};

int msm_setup_v4l2_event_queue(struct v4l2_fh *eventHandle,
					struct video_device *pvdev)
{
	int rc = 0;
	/* v4l2_fh support */
	spin_lock_init(&pvdev->fh_lock);
	INIT_LIST_HEAD(&pvdev->fh_list);

	rc = v4l2_fh_init(eventHandle, pvdev);
	if (rc < 0)
		return rc;
	if (eventHandle->events == NULL) {
		rc = v4l2_event_init(eventHandle);
		if (rc < 0)
			return rc;
	}

	/* queue of max size 30 */
	rc = v4l2_event_alloc(eventHandle, 30);
	if (rc < 0)
		return rc;

	v4l2_fh_add(eventHandle);
	return rc;

}

static int msm_setup_config_dev(int node, char *device_name)
{
	int rc = -ENODEV;
	struct device *device_config;
	int dev_num = node;
	dev_t devno;
	struct msm_cam_config_dev *config_cam;

	config_cam = kzalloc(sizeof(*config_cam), GFP_KERNEL);
	if (!config_cam) {
		pr_err("%s: could not allocate memory for msm_cam_config_device\n",
			__func__);
		return -ENOMEM;
	}

	D("%s\n", __func__);

	devno = MKDEV(MAJOR(msm_devno), dev_num+1);
	device_config = device_create(msm_class, NULL, devno, NULL,
					"%s%d", device_name, dev_num);

	if (IS_ERR(device_config)) {
		rc = PTR_ERR(device_config);
		pr_err("%s: error creating device: %d\n", __func__, rc);
		return rc;
	}

	cdev_init(&config_cam->config_cdev,
			&msm_fops_config);
	config_cam->config_cdev.owner = THIS_MODULE;

	rc = cdev_add(&config_cam->config_cdev, devno, 1);
	if (rc < 0) {
		pr_err("%s: error adding cdev: %d\n", __func__, rc);
		device_destroy(msm_class, devno);
		return rc;
	}
	g_server_dev.config_info.config_dev_name[dev_num]
		= dev_name(device_config);
	D("%s Connected config device %s\n", __func__,
		g_server_dev.config_info.config_dev_name[dev_num]);
	g_server_dev.config_info.config_dev_id[dev_num]
		= dev_num;

	config_cam->config_stat_event_queue.pvdev = video_device_alloc();
	if (config_cam->config_stat_event_queue.pvdev == NULL) {
		pr_err("%s: video_device_alloc failed\n", __func__);
		return -ENOMEM;
	}

	rc = msm_setup_v4l2_event_queue(
		&config_cam->config_stat_event_queue.eventHandle,
		config_cam->config_stat_event_queue.pvdev);
	if (rc < 0)
		pr_err("%s failed to initialize event queue\n", __func__);

	return rc;
}

static int msm_setup_server_dev(int node, char *device_name)
{
	int rc = -ENODEV;
	struct device *device_server;
	int dev_num = node;
	dev_t devno;

	D("%s\n", __func__);

	devno = MKDEV(MAJOR(msm_devno), dev_num);
	device_server = device_create(msm_class, NULL,
			devno, NULL, "%s", device_name);

	if (IS_ERR(device_server)) {
		rc = PTR_ERR(device_server);
		pr_err("%s: error creating device: %d\n", __func__, rc);
		return rc;
	}

	cdev_init(&g_server_dev.server_cdev, &msm_fops_server);
	g_server_dev.server_cdev.owner = THIS_MODULE;

	rc = cdev_add(&g_server_dev.server_cdev, devno, 1);
	if (rc < 0) {
		pr_err("%s: error adding cdev: %d\n", __func__, rc);
		device_destroy(msm_class, devno);
		return rc;
	}

	mutex_init(&g_server_dev.server_lock);
	g_server_dev.pcam_active = NULL;
	g_server_dev.camera_info.num_cameras = 0;
	atomic_set(&g_server_dev.number_pcam_active, 0);

	/*initialize fake video device and event queue*/

	g_server_dev.server_command_queue.pvdev = video_device_alloc();
	if (g_server_dev.server_command_queue.pvdev == NULL) {
		pr_err("%s: video_device_alloc failed\n", __func__);
		return -ENOMEM;
	}
	rc = msm_setup_v4l2_event_queue(
		&g_server_dev.server_command_queue.eventHandle,
		g_server_dev.server_command_queue.pvdev);
	if (rc < 0)
		pr_err("%s failed to initialize event queue\n", __func__);

	return rc;
}

static int msm_cam_dev_init(struct msm_cam_v4l2_device *pcam)
{
	int rc = -ENOMEM;
	struct video_device *pvdev = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(pcam->mctl.sensor_sdev);
	D("%s\n", __func__);

	/* first register the v4l2 device */
	pcam->v4l2_dev.dev = &client->dev;
	rc = v4l2_device_register(pcam->v4l2_dev.dev, &pcam->v4l2_dev);
	if (rc < 0)
		return -EINVAL;
	else
		pcam->v4l2_dev.notify = msm_cam_v4l2_subdev_notify;


	/* now setup video device */
	pvdev = video_device_alloc();
	if (pvdev == NULL) {
		pr_err("%s: video_device_alloc failed\n", __func__);
		return rc;
	}

	/* init video device's driver interface */
	D("sensor name = %s, sizeof(pvdev->name)=%d\n",
		pcam->mctl.sensor_sdev->name, sizeof(pvdev->name));

	/* device info - strlcpy is safer than strncpy but
	   only if architecture supports*/
	strlcpy(pvdev->name, pcam->mctl.sensor_sdev->name, sizeof(pvdev->name));

	pvdev->release   = video_device_release;
	pvdev->fops	  = &g_msm_fops;
	pvdev->ioctl_ops = &g_msm_ioctl_ops;
	pvdev->minor	 = -1;
	pvdev->vfl_type  = 1;

	/* register v4l2 video device to kernel as /dev/videoXX */
	D("video_register_device\n");
	rc = video_register_device(pvdev,
					VFL_TYPE_GRABBER,
					msm_camera_v4l2_nr);
	if (rc) {
		pr_err("%s: video_register_device failed\n", __func__);
		goto reg_fail;
	}
	D("%s: video device registered as /dev/video%d\n",
		__func__, pvdev->num);

	/* connect pcam and video dev to each other */
	pcam->pvdev	= pvdev;
	video_set_drvdata(pcam->pvdev, pcam);

	/* If isp HW registeration is successful,
	 * then create event queue to
	 * receievent event froms HW
	*/
	/* yyan: no global - each sensor will
	 * create a new vidoe node! */
	/* g_pmsm_camera_v4l2_dev = pmsm_camera_v4l2_dev; */
	/* g_pmsm_camera_v4l2_dev->pvdev = pvdev; */

	return rc ;

reg_fail:
	video_device_release(pvdev);
	v4l2_device_unregister(&pcam->v4l2_dev);
	pcam->v4l2_dev.dev = NULL;
	return rc;
}

static int msm_sync_destroy(struct msm_sync *sync)
{
	if (sync) {
		mutex_destroy(&sync->lock);
		wake_lock_destroy(&sync->wake_lock);
	}
	return 0;
}

static int msm_actuator_probe(struct msm_actuator_info *actuator_info,
			      struct v4l2_subdev *act_sdev,
			      struct msm_actuator_ctrl *actctrl)
{
	int rc = 0;
	struct i2c_adapter *adapter = NULL;
	void *act_client = NULL;
	struct msm_actuator_ctrl *a_ext_ctrl = NULL;

	D("%s called\n", __func__);

	if (!actuator_info)
		goto probe_fail;

	adapter = i2c_get_adapter(actuator_info->bus_id);
	if (!adapter)
		goto probe_fail;

	act_client = i2c_new_device(adapter, actuator_info->board_info);
	if (!act_client)
		goto device_fail;

	a_ext_ctrl = (struct msm_actuator_ctrl *)i2c_get_clientdata(act_client);
	if (!a_ext_ctrl)
		goto client_fail;

	*actctrl = *a_ext_ctrl;
	a_ext_ctrl->a_create_subdevice((void *)actuator_info,
				       (void *)act_sdev);
	return rc;

client_fail:
	i2c_unregister_device(act_client);
device_fail:
	i2c_put_adapter(adapter);
	adapter = NULL;
probe_fail:
	actctrl->a_init_table = NULL;
	actctrl->a_power_up = NULL;
	actctrl->a_power_down = NULL;
	actctrl->a_config = NULL;
	actctrl->a_create_subdevice = NULL;
	return rc;
}

static int msm_sync_init(struct msm_sync *sync,
	struct platform_device *pdev)
{
	int rc = 0;
	wake_lock_init(&sync->wake_lock, WAKE_LOCK_IDLE, "msm_camera");
	sync->opencnt = 0;
	mutex_init(&sync->lock);
	return rc;
}

/* register a msm sensor into the msm device, which will probe the
 * sensor HW. if the HW exist then create a video device (/dev/videoX/)
 * to represent this sensor */
int msm_sensor_register(struct v4l2_subdev *sensor_sd)
{
	int rc = -EINVAL;
	struct msm_camera_sensor_info *sdata;
	struct msm_cam_v4l2_device *pcam;
	struct msm_sensor_ctrl_t *s_ctrl;
	struct v4l2_subdev *act_sdev = NULL;
	struct msm_actuator_ctrl *actctrl = NULL;

	D("%s for %s\n", __func__, sensor_sd->name);

	/* allocate the memory for the camera device first */
	pcam = kzalloc(sizeof(*pcam), GFP_KERNEL);
	if (!pcam) {
		pr_err("%s: could not allocate mem for msm_cam_v4l2_device\n",
			__func__);
		return -ENOMEM;
	}

	pcam->mctl.sensor_sdev = sensor_sd;
	s_ctrl = get_sctrl(sensor_sd);
	sdata = (struct msm_camera_sensor_info *) s_ctrl->sensordata;

	pcam->mctl.act_sdev = kzalloc(sizeof(struct v4l2_subdev),
								  GFP_KERNEL);
	if (!pcam->mctl.act_sdev) {
		pr_err("%s: could not allocate mem for actuator v4l2_subdev\n",
			   __func__);
		kfree(pcam);
		return -ENOMEM;
	}

	act_sdev = pcam->mctl.act_sdev;
	actctrl = &pcam->mctl.sync.actctrl;

	msm_actuator_probe(sdata->actuator_info,
					   act_sdev, actctrl);

	/* setup a manager object*/
	rc = msm_sync_init(&pcam->mctl.sync, NULL);
	if (rc < 0)
		goto failure;
	D("%s: pcam =0x%p\n", __func__, pcam);
	D("%s: &pcam->mctl.sync =0x%p\n", __func__, &pcam->mctl.sync);

	pcam->mctl.sync.sdata = sdata;
	pcam->mctl.sync.pcam_sync = pcam;

	/* init the user count and lock*/
	pcam->use_count = 0;
	mutex_init(&pcam->vid_lock);
	mutex_init(&pcam->mctl_node.dev_lock);

	/* Initialize the formats supported */
	rc  = msm_mctl_init_user_formats(pcam);
	if (rc < 0)
		goto failure;

	rc  = msm_cam_dev_init(pcam);
	if (rc < 0)
		goto failure;

	rc = msm_setup_mctl_node(pcam);
	if (rc < 0) {
		pr_err("%s:failed to create mctl device: %d\n",
			 __func__, rc);
		goto failure;
	}

	g_server_dev.camera_info.video_dev_name
	[g_server_dev.camera_info.num_cameras]
	= video_device_node_name(pcam->pvdev);
	D("%s Connected video device %s\n", __func__,
		g_server_dev.camera_info.video_dev_name
		[g_server_dev.camera_info.num_cameras]);

	g_server_dev.camera_info.s_mount_angle
	[g_server_dev.camera_info.num_cameras]
	= sdata->sensor_platform_info->mount_angle;

	g_server_dev.camera_info.is_internal_cam
	[g_server_dev.camera_info.num_cameras]
	= sdata->camera_type;

	g_server_dev.mctl_node_info.mctl_node_name
	[g_server_dev.mctl_node_info.num_mctl_nodes]
	= video_device_node_name(pcam->mctl_node.pvdev);

	pr_info("%s mctl_node_name[%d] = %s\n", __func__,
		g_server_dev.mctl_node_info.num_mctl_nodes,
		g_server_dev.mctl_node_info.mctl_node_name
		[g_server_dev.mctl_node_info.num_mctl_nodes]);

	g_server_dev.camera_info.num_cameras++;
	g_server_dev.mctl_node_info.num_mctl_nodes++;

	D("%s done, rc = %d\n", __func__, rc);
	D("%s number of sensors connected is %d\n", __func__,
		g_server_dev.camera_info.num_cameras);

	/* register the subdevice, must be done for callbacks */
	rc = v4l2_device_register_subdev(&pcam->v4l2_dev, sensor_sd);
	if (rc < 0) {
		D("%s sensor sub device register failed\n",
			__func__);
		goto failure;
	}

	if (sdata->actuator_info) {
		rc = v4l2_device_register_subdev(&pcam->v4l2_dev, act_sdev);
		if (rc < 0) {
			D("%s actuator sub device register failed\n",
			  __func__);
			goto failure;
		}
	}

	pcam->vnode_id = vnode_count++;
	return rc;

failure:
	/* mutex_destroy not needed at this moment as the associated
	implemenation of mutex_init is not consuming resources */
	msm_sync_destroy(&pcam->mctl.sync);
	kfree(act_sdev);
	kzfree(pcam);
	return rc;
}
EXPORT_SYMBOL(msm_sensor_register);

static int __init msm_camera_init(void)
{
	int rc = 0, i;
	/*for now just create a config 0 node
	  put logic here later to know how many configs to create*/
	g_server_dev.config_info.num_config_nodes = 1;

	rc = msm_isp_init_module(g_server_dev.config_info.num_config_nodes);
	if (rc < 0) {
		pr_err("Failed to initialize isp\n");
		return rc;
	}

	if (!msm_class) {
		rc = alloc_chrdev_region(&msm_devno, 0,
		g_server_dev.config_info.num_config_nodes+1, "msm_camera");
		if (rc < 0) {
			pr_err("%s: failed to allocate chrdev: %d\n", __func__,
			rc);
			return rc;
		}

		msm_class = class_create(THIS_MODULE, "msm_camera");
		if (IS_ERR(msm_class)) {
			rc = PTR_ERR(msm_class);
			pr_err("%s: create device class failed: %d\n",
			__func__, rc);
			return rc;
		}
	}

	D("creating server and config nodes\n");
	rc = msm_setup_server_dev(0, "video_msm");
	if (rc < 0) {
		pr_err("%s: failed to create server dev: %d\n", __func__,
		rc);
		return rc;
	}

	for (i = 0; i < g_server_dev.config_info.num_config_nodes; i++) {
		rc = msm_setup_config_dev(i, "config");
		if (rc < 0) {
			pr_err("%s:failed to create config dev: %d\n",
			 __func__, rc);
			return rc;
		}
	}

	msm_isp_register(&g_server_dev);
	return rc;
}

static void __exit msm_camera_exit(void)
{
	msm_isp_unregister(&g_server_dev);
}

module_init(msm_camera_init);
module_exit(msm_camera_exit);
