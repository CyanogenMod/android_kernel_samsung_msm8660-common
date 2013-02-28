/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <kgsl_device.h>

#include "kgsl_trace.h"

static void _add_event_to_list(struct list_head *head, struct kgsl_event *event)
{
	struct list_head *n;

	for (n = head->next; n != head; n = n->next) {
		struct kgsl_event *e =
			list_entry(n, struct kgsl_event, list);

		if (timestamp_cmp(e->timestamp, event->timestamp) > 0) {
			list_add(&event->list, n->prev);
			break;
		}
	}

	if (n == head)
		list_add_tail(&event->list, head);
}

/**
 * kgsl_add_event - Add a new timstamp event for the KGSL device
 * @device - KGSL device for the new event
 * @ts - the timestamp to trigger the event on
 * @cb - callback function to call when the timestamp expires
 * @priv - private data for the specific event type
 * @owner - driver instance that owns this event
 *
 * @returns - 0 on success or error code on failure
 */

int kgsl_add_event(struct kgsl_device *device, u32 ts,
	void (*cb)(struct kgsl_device *, void *, u32), void *priv,
	void *owner)
{
	struct kgsl_event *event;
	unsigned int cur_ts;

	if (cb == NULL)
		return -EINVAL;

	cur_ts = device->ftbl->readtimestamp(device,
						KGSL_TIMESTAMP_RETIRED);


	/*
	 * Check to see if the requested timestamp has already fired.  If it
	 * did do the callback right away.  Make sure to send the timestamp that
	 * the event expected instead of the current timestamp because sometimes
	 * the event handlers can get confused.
	 */

	if (timestamp_cmp(cur_ts, ts) >= 0) {
		trace_kgsl_fire_event(ts, 0);
		cb(device, priv, ts);
		return 0;
	}

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (event == NULL)
		return -ENOMEM;

	event->owner = owner;
	event->timestamp = ts;
	event->priv = priv;
	event->func = cb;
	event->owner = owner;
	event->created = jiffies;

	trace_kgsl_register_event(ts);

	_add_event_to_list(&device->events, event);

	/*
	 * Increase the active count on the device to avoid going into power
	 * saving modes while events are pending
	 */

	device->active_cnt++;

	queue_work(device->work_queue, &device->ts_expired_ws);
	return 0;
}
EXPORT_SYMBOL(kgsl_add_event);

/**
 * kgsl_cancel_events - Cancel all generic events for a process
 * @device - KGSL device for the events to cancel
 * @owner - driver instance that owns the events to cancel
 *
 */
void kgsl_cancel_events(struct kgsl_device *device,
	void *owner)
{
	struct kgsl_event *event, *event_tmp;
	unsigned int cur = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);

	list_for_each_entry_safe(event, event_tmp, &device->events, list) {
		if (event->owner != owner)
			continue;

		/*
		 * "cancel" the events by calling their callback.
		 * Currently, events are used for lock and memory
		 * management, so if the process is dying the right
		 * thing to do is release or free. Send the current timestamp so
		 * the callback knows how far the GPU made it before things went
		 * explosion
		 */

		trace_kgsl_fire_event(cur, jiffies - event->created);

		if (event->func)
			event->func(device, event->priv, cur);

		list_del(&event->list);
		kfree(event);

		kgsl_active_count_put(device);
	}
}
EXPORT_SYMBOL(kgsl_cancel_events);

static inline void _process_event_list(struct kgsl_device *device,
		struct list_head *head, unsigned int timestamp)
{
	struct kgsl_event *event, *tmp;

	list_for_each_entry_safe(event, tmp, head, list) {
		if (timestamp_cmp(timestamp, event->timestamp) < 0)
			break;

		/*
		 * Send the timestamp of the expired event, not the current
		 * timestamp.  This prevents the event handlers from getting
		 * confused if they don't bother comparing the current timetamp
		 * to the timestamp they wanted
		 */

		trace_kgsl_fire_event(event->timestamp,
			jiffies - event->created);

		if (event->func)
			event->func(device, event->priv, event->timestamp);

		list_del(&event->list);
		kfree(event);

		kgsl_active_count_put(device);
	}
}

static inline int _mark_next_event(struct kgsl_device *device,
		struct list_head *head)
{
	struct kgsl_event *event;

	if (!list_empty(head)) {
		event = list_first_entry(head, struct kgsl_event, list);
		if (device->ftbl->next_event)
			return device->ftbl->next_event(device, event);
	}
	return 0;
}
void kgsl_process_events(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work, struct kgsl_device,
		ts_expired_ws);
	uint32_t timestamp;

	mutex_lock(&device->mutex);

	while (1) {

		/* Process expired global events */
		timestamp = device->ftbl->readtimestamp(device,
				KGSL_TIMESTAMP_RETIRED);

		_process_event_list(device, &device->events, timestamp);

		/*
		 * Keep looping until we hit an event which has not
		 * passed and then we write a dummy interrupt.
		 * mark_next_event will return 1 for every event
		 * that has passed and return 0 for the event which has not
		 * passed yet.
		 */
		if (_mark_next_event(device, &device->events) == 0)
			break;

	}

	mutex_unlock(&device->mutex);
}
EXPORT_SYMBOL(kgsl_process_events);
