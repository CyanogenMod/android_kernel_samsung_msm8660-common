/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
 * Qualcomm MSM Runqueue Stats and cpu utilization Interface for Userspace
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
#include <linux/cpu.h>
#define DUALBOOST_DEFERED_QUEUE
#endif
#include <linux/cpufreq.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include "acpuclock.h"

#define MAX_LONG_SIZE 24
#define DEFAULT_RQ_POLL_JIFFIES 1
#define DEFAULT_DEF_TIMER_JIFFIES 5

struct notifier_block freq_transition;
struct notifier_block cpu_hotplug;
struct notifier_block freq_policy;

struct cpu_load_data {
       cputime64_t prev_cpu_idle;
       cputime64_t prev_cpu_wall;
       cputime64_t prev_cpu_iowait;
       unsigned int avg_load_maxfreq;
       unsigned int samples;
       unsigned int window_size;
       unsigned int cur_freq;
       unsigned int policy_max;
       cpumask_var_t related_cpus;
       struct mutex cpu_load_mutex;
};

static DEFINE_PER_CPU(struct cpu_load_data, cpuload);

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kstat_cpu(cpu).cpustat.user;
	busy_time += kstat_cpu(cpu).cpustat.system;
	busy_time += kstat_cpu(cpu).cpustat.irq;
	busy_time += kstat_cpu(cpu).cpustat.softirq;
	busy_time += kstat_cpu(cpu).cpustat.steal;
	busy_time += kstat_cpu(cpu).cpustat.nice;

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
 }
 
static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu,
                                                        cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static int update_average_load(unsigned int freq, unsigned int cpu)
{
	struct cpu_load_data *pcpu = &per_cpu(cpuload, cpu);
	cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
	unsigned int idle_time, wall_time, iowait_time;
	unsigned int cur_load, load_at_max_freq;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time);
	cur_iowait_time = get_cpu_iowait_time(cpu, &cur_wall_time);

	wall_time = (unsigned int) (cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int) (cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;

	iowait_time = (unsigned int) (cur_iowait_time - pcpu->prev_cpu_iowait);
	pcpu->prev_cpu_iowait = cur_iowait_time;

	if (unlikely(!wall_time || wall_time < idle_time))
		return 0;

	cur_load = 100 * (wall_time - idle_time) / wall_time;

	/* Calculate the scaled load across CPU */
	load_at_max_freq = (cur_load * freq) / pcpu->policy_max;

	if (!pcpu->avg_load_maxfreq) {
		/* This is the first sample in this window*/
		pcpu->avg_load_maxfreq = load_at_max_freq;
		pcpu->window_size = wall_time;
	} else {
		/*
		* The is already a sample available in this window.
		* Compute weighted average with prev entry, so that we get
		* the precise weighted load.
		*/
		pcpu->avg_load_maxfreq =
			((pcpu->avg_load_maxfreq * pcpu->window_size) +
			(load_at_max_freq * wall_time)) /
			(wall_time + pcpu->window_size);

		pcpu->window_size += wall_time;
	}

	return 0;
}

static unsigned int report_load_at_max_freq(void)
{
	int cpu;
	struct cpu_load_data *pcpu;
	unsigned int total_load = 0;

	for_each_online_cpu(cpu) {
		pcpu = &per_cpu(cpuload, cpu);
		mutex_lock(&pcpu->cpu_load_mutex);
		update_average_load(pcpu->cur_freq, cpu);
		total_load += pcpu->avg_load_maxfreq;
		pcpu->avg_load_maxfreq = 0;
		mutex_unlock(&pcpu->cpu_load_mutex);
	}
	return total_load;
}

static int cpufreq_transition_handler(struct notifier_block *nb,
                        unsigned long val, void *data)
{
	struct cpufreq_freqs *freqs = data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, freqs->cpu);
	int j;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		for_each_cpu(j, this_cpu->related_cpus) {
			struct cpu_load_data *pcpu = &per_cpu(cpuload, j);
			mutex_lock(&pcpu->cpu_load_mutex);
			update_average_load(freqs->old, freqs->cpu);
			pcpu->cur_freq = freqs->new;
			mutex_unlock(&pcpu->cpu_load_mutex);
		}
		break;
	}
	return 0;
}

static int cpu_hotplug_handler(struct notifier_block *nb,
                        unsigned long val, void *data)
{
	unsigned int cpu = (unsigned long)data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, cpu);

	switch (val) {
	case CPU_ONLINE:
		if (!this_cpu->cur_freq)
			this_cpu->cur_freq = acpuclk_get_rate(cpu);
	case CPU_ONLINE_FROZEN:
		this_cpu->avg_load_maxfreq = 0;
	}

	return NOTIFY_OK;
}

static void def_work_fn(struct work_struct *work)
{
	int64_t diff;

	diff = ktime_to_ns(ktime_get()) - rq_info.def_start_time;
	do_div(diff, 1000 * 1000);
	rq_info.def_interval = (unsigned int) diff;

	/* Notify polling threads on change of value */
	sysfs_notify(rq_info.kobj, NULL, "def_timer_ms");
}

static int freq_policy_handler(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, policy->cpu);

	if (event != CPUFREQ_NOTIFY)
		goto out;

	this_cpu->policy_max = policy->max;

	pr_debug("Policy max changed from %u to %u, event %lu\n",
			this_cpu->policy_max, policy->max, event);
out:
	return NOTIFY_DONE;
}

#ifdef CONFIG_SEC_DVFS_DUAL
static int is_dual_locked = 0;
static int is_sysfs_used = 0;
static int is_uevent_sent = 0;

static DEFINE_MUTEX(cpu_hotplug_driver_mutex);

int cpu_hotplug_driver_test_lock(void)
{
	return mutex_trylock(&cpu_hotplug_driver_mutex);
}

void cpu_hotplug_driver_lock(void)
{
	mutex_lock(&cpu_hotplug_driver_mutex);
}

void cpu_hotplug_driver_unlock(void)
{
	mutex_unlock(&cpu_hotplug_driver_mutex);
}

static void dvfs_hotplug_callback(struct work_struct *unused)
{
	if (cpu_hotplug_driver_test_lock()) {
		if (cpu_is_offline(NON_BOOT_CPU)) {
			ssize_t ret;
			struct sys_device *cpu_sys_dev;
	
			ret = cpu_up(NON_BOOT_CPU); // it may take 60ms
			if (!ret) {
				cpu_sys_dev = get_cpu_sysdev(NON_BOOT_CPU);
				kobject_uevent(&cpu_sys_dev->kobj, KOBJ_ONLINE);
				is_uevent_sent = 1;
			}
		}
		cpu_hotplug_driver_unlock();
	} else if (cpu_is_offline(NON_BOOT_CPU)) {
		is_sysfs_used = 1;
		sysfs_notify(rq_info.kobj, NULL, "def_timer_ms");
	}
}

#if defined(DUALBOOST_DEFERED_QUEUE)
static DECLARE_WORK(dvfs_hotplug_work, dvfs_hotplug_callback);
#endif

void dual_boost(unsigned int boost_on)
{
	if (boost_on)
	{	
		if (is_dual_locked != 0)
			return;

		is_dual_locked = 1;

#if defined(DUALBOOST_DEFERED_QUEUE)
		if (cpu_is_offline(NON_BOOT_CPU) && !work_busy(&dvfs_hotplug_work))
			schedule_work_on(BOOT_CPU, &dvfs_hotplug_work);
#else
		if (cpu_is_offline(NON_BOOT_CPU))
			dvfs_hotplug_callback(NULL);
#endif
	}
	else {
		if (is_uevent_sent == 1 && !cpu_is_offline(NON_BOOT_CPU)) {
			struct sys_device *cpu_sys_dev = get_cpu_sysdev(NON_BOOT_CPU);
			kobject_uevent(&cpu_sys_dev->kobj, KOBJ_ONLINE);
		}

		is_uevent_sent = 0;		
		is_sysfs_used = 0;
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
#if defined (CONFIG_SEC_DVFS_DUAL)
	if (is_sysfs_used == 1)
		return snprintf(buf, MAX_LONG_SIZE, "%u\n", jiffies_to_msecs(rq_info.def_timer_jiffies));
	else
#endif
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

static ssize_t store_cpu_normalized_load(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t show_cpu_normalized_load(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
#ifdef CONFIG_SEC_DVFS_DUAL
	if (is_dual_locked == 1)
		return snprintf(buf, MAX_LONG_SIZE, "%u\n", report_load_at_max_freq() + 200);
	else
#endif
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", report_load_at_max_freq());
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
	attribs[3] = MSM_RQ_STATS_RW_ATTRIB(cpu_normalized_load);
	attribs[4] = NULL;

	for (i = 0; i < attr_count - 1 ; i++) {
		if (!attribs[i])
			goto rel2;
	}

	rq_info.attr_group = kzalloc(sizeof(struct attribute_group),
						GFP_KERNEL);
	if (!rq_info.attr_group)
		goto rel3;
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
	int i;
	struct cpufreq_policy cpu_policy;

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
	for_each_possible_cpu(i) {
		struct cpu_load_data *pcpu = &per_cpu(cpuload, i);
		mutex_init(&pcpu->cpu_load_mutex);
		cpufreq_get_policy(&cpu_policy, i);
		pcpu->policy_max = cpu_policy.cpuinfo.max_freq;
		if (cpu_online(i))
			pcpu->cur_freq = acpuclk_get_rate(i);
		cpumask_copy(pcpu->related_cpus, cpu_policy.cpus);
	}
	freq_transition.notifier_call = cpufreq_transition_handler;
	cpu_hotplug.notifier_call = cpu_hotplug_handler;
	freq_policy.notifier_call = freq_policy_handler;
	cpufreq_register_notifier(&freq_transition,
				CPUFREQ_TRANSITION_NOTIFIER);
	register_hotcpu_notifier(&cpu_hotplug);
	cpufreq_register_notifier(&freq_policy,
					CPUFREQ_POLICY_NOTIFIER);

	return ret;
}
late_initcall(msm_rq_stats_init);
