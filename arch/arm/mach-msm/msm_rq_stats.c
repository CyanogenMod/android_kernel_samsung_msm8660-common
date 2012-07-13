/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm MSM Runqueue Stats Interface for Userspace
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/rq_stats.h>

#ifdef CONFIG_SEC_DVFS_DUAL
#include <linux/cpufreq.h>
#include <linux/cpu.h>
//#define DUALBOOST_DEFERED_QUEUE
#endif

#define MAX_LONG_SIZE 24
#define DEFAULT_RQ_POLL_JIFFIES 1
#define DEFAULT_DEF_TIMER_JIFFIES 5

static void def_work_fn(struct work_struct *work)
{
	int64_t diff;

	diff = ktime_to_ns(ktime_get()) - rq_info.def_start_time;
	do_div(diff, 1000 * 1000);
	rq_info.def_interval = (unsigned int) diff;

	/* Notify polling threads on change of value */
	sysfs_notify(rq_info.kobj, NULL, "def_timer_ms");
}

#ifdef CONFIG_SEC_DVFS_DUAL
static int stall_mpdecision = 0;

#ifdef CONFIG_SEC_DVFS_DUAL_LOCK
static DEFINE_MUTEX(cpu_hotplug_driver_mutex);

void cpu_hotplug_driver_lock(void)
{
	mutex_lock(&cpu_hotplug_driver_mutex);
}

void cpu_hotplug_driver_unlock(void)
{
	mutex_unlock(&cpu_hotplug_driver_mutex);
}
#endif

static void dvfs_hotplug_callback(struct work_struct *unused)
{
	cpu_hotplug_driver_lock();
	if (cpu_is_offline(NON_BOOT_CPU))
	{
		ssize_t ret;
		struct sys_device *cpu_sys_dev;
	
		ret = cpu_up(NON_BOOT_CPU); // it takes 60ms
		if (!ret)
		{
			cpu_sys_dev = get_cpu_sysdev(NON_BOOT_CPU);
			if (cpu_sys_dev)
			{
				kobject_uevent(&cpu_sys_dev->kobj, KOBJ_ONLINE);
				stall_mpdecision = 1;
			}
		}
	}
	cpu_hotplug_driver_unlock();
}
static DECLARE_WORK(dvfs_hotplug_work, dvfs_hotplug_callback);
static int is_dual_locked = 0;

void dual_boost(unsigned int boost_on)
{
	if (boost_on)
	{	
		if (is_dual_locked != 0)
			return;

#ifndef DUALBOOST_DEFERED_QUEUE
		cpu_hotplug_driver_lock();
		if (cpu_is_offline(NON_BOOT_CPU))
		{
			ssize_t ret;
			struct sys_device *cpu_sys_dev;
		
			ret = cpu_up(NON_BOOT_CPU); // it takes 60ms
			if (!ret)
			{
				cpu_sys_dev = get_cpu_sysdev(NON_BOOT_CPU);
				if (cpu_sys_dev)
				{
					kobject_uevent(&cpu_sys_dev->kobj, KOBJ_ONLINE);
					stall_mpdecision = 1;
				}
			}
		}
		cpu_hotplug_driver_unlock();
#else	
		if (cpu_is_offline(NON_BOOT_CPU))
			schedule_work_on(BOOT_CPU, &dvfs_hotplug_work);
#endif
		is_dual_locked = 1;
	}
	else
	{
		if (stall_mpdecision == 1)
		{
			struct sys_device *cpu_sys_dev;

#ifdef DUALBOOST_DEFERED_QUEUE
			flush_work(&dvfs_hotplug_work);
#endif
			cpu_hotplug_driver_lock();	
			cpu_sys_dev = get_cpu_sysdev(NON_BOOT_CPU);
			if (cpu_sys_dev)
			{
				kobject_uevent(&cpu_sys_dev->kobj, KOBJ_ONLINE);
				stall_mpdecision = 0;
			}
			cpu_hotplug_driver_unlock();
		}
		
		is_dual_locked = 0;
	}
}
#endif

static ssize_t show_run_queue_avg(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int val = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	/* rq avg currently available only on one core */
	val = rq_info.rq_avg;
	rq_info.rq_avg = 0;
	spin_unlock_irqrestore(&rq_lock, flags);

#ifdef CONFIG_SEC_DVFS_DUAL
	if (is_dual_locked == 1)
		val = val + 1000;
#endif

	return snprintf(buf, PAGE_SIZE, "%d.%d\n", val/10, val%10);
}

static ssize_t show_run_queue_poll_ms(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	ret = snprintf(buf, MAX_LONG_SIZE, "%u\n",
		       jiffies_to_msecs(rq_info.rq_poll_jiffies));
	spin_unlock_irqrestore(&rq_lock, flags);

	return ret;
}

static ssize_t store_run_queue_poll_ms(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned int val = 0;
	unsigned long flags = 0;
	static DEFINE_MUTEX(lock_poll_ms);

	mutex_lock(&lock_poll_ms);

	spin_lock_irqsave(&rq_lock, flags);
	sscanf(buf, "%u", &val);
	rq_info.rq_poll_jiffies = msecs_to_jiffies(val);
	spin_unlock_irqrestore(&rq_lock, flags);

	mutex_unlock(&lock_poll_ms);

	return count;
}

static ssize_t show_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", rq_info.def_interval);
}

static ssize_t store_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	sscanf(buf, "%u", &val);
	rq_info.def_timer_jiffies = msecs_to_jiffies(val);

	rq_info.def_start_time = ktime_to_ns(ktime_get());
	return count;
}

#define MSM_RQ_STATS_RO_ATTRIB(att) ({ \
		struct attribute *attrib = NULL; \
		struct kobj_attribute *ptr = NULL; \
		ptr = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL); \
		if (ptr) { \
			ptr->attr.name = #att; \
			ptr->attr.mode = S_IRUGO; \
			ptr->show = show_##att; \
			ptr->store = NULL; \
			attrib = &ptr->attr; \
		} \
		attrib; })

#define MSM_RQ_STATS_RW_ATTRIB(att) ({ \
		struct attribute *attrib = NULL; \
		struct kobj_attribute *ptr = NULL; \
		ptr = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL); \
		if (ptr) { \
			ptr->attr.name = #att; \
			ptr->attr.mode = S_IWUSR|S_IRUSR; \
			ptr->show = show_##att; \
			ptr->store = store_##att; \
			attrib = &ptr->attr; \
		} \
		attrib; })

static int init_rq_attribs(void)
{
	int i;
	int err = 0;
	const int attr_count = 4;

	struct attribute **attribs =
		kzalloc(sizeof(struct attribute *) * attr_count, GFP_KERNEL);

	if (!attribs)
		goto rel;

	rq_info.rq_avg = 0;

	attribs[0] = MSM_RQ_STATS_RW_ATTRIB(def_timer_ms);
	attribs[1] = MSM_RQ_STATS_RO_ATTRIB(run_queue_avg);
	attribs[2] = MSM_RQ_STATS_RW_ATTRIB(run_queue_poll_ms);
	attribs[3] = NULL;

	for (i = 0; i < attr_count - 1 ; i++) {
		if (!attribs[i])
			goto rel2;
	}

	rq_info.attr_group = kzalloc(sizeof(struct attribute_group),
						GFP_KERNEL);
	if (!rq_info.attr_group)
		goto rel2;
	rq_info.attr_group->attrs = attribs;

	/* Create /sys/devices/system/cpu/cpu0/rq-stats/... */
	rq_info.kobj = kobject_create_and_add("rq-stats",
			&get_cpu_sysdev(0)->kobj);
	if (!rq_info.kobj)
		goto rel3;

	err = sysfs_create_group(rq_info.kobj, rq_info.attr_group);
	if (err)
		kobject_put(rq_info.kobj);
	else
		kobject_uevent(rq_info.kobj, KOBJ_ADD);

	if (!err)
		return err;

rel3:
	kfree(rq_info.attr_group);
	kfree(rq_info.kobj);
rel2:
	for (i = 0; i < attr_count - 1; i++)
		kfree(attribs[i]);
rel:
	kfree(attribs);

	return -ENOMEM;
}

static int __init msm_rq_stats_init(void)
{
	int ret;

	rq_wq = create_singlethread_workqueue("rq_stats");
	BUG_ON(!rq_wq);
	INIT_WORK(&rq_info.def_timer_work, def_work_fn);
	spin_lock_init(&rq_lock);
	rq_info.rq_poll_jiffies = DEFAULT_RQ_POLL_JIFFIES;
	rq_info.def_timer_jiffies = DEFAULT_DEF_TIMER_JIFFIES;
	rq_info.rq_poll_last_jiffy = 0;
	rq_info.def_timer_last_jiffy = 0;
	ret = init_rq_attribs();

	rq_info.init = 1;
	return ret;
}
late_initcall(msm_rq_stats_init);
