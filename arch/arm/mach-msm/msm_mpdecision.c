/*
 * arch/arm/mach-msm/msm_mpdecision.c
 *
 * This program features:
 * -cpu auto-hotplug/unplug based on system load for MSM multicore cpus
 * -single core while screen is off
 * -extensive sysfs tuneables
 *
 * Copyright (c) 2012-2013, Dennis Rassmann <showp1984@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/earlysuspend.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <asm-generic/cputime.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/export.h>
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
#include <linux/input.h>
#include <linux/slab.h>
#endif
#include "acpuclock.h"

#define DEBUG 0

#define MPDEC_TAG                       "[MPDEC]: "
#define MSM_MPDEC_STARTDELAY            7000
#define MSM_MPDEC_DELAY                 70
#define MSM_MPDEC_PAUSE                 5000
#define MSM_MPDEC_IDLE_FREQ             486000
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
#define MSM_MPDEC_BOOSTTIME             800
#define MSM_MPDEC_BOOSTFREQ_CPU0        918000
#define MSM_MPDEC_BOOSTFREQ_CPU1        702000
#define MSM_MPDEC_BOOSTFREQ_CPU2	702000
#define MSM_MPDEC_BOOSTFREQ_CPU3	594000
#endif

enum {
    MSM_MPDEC_DISABLED = 0,
    MSM_MPDEC_IDLE,
    MSM_MPDEC_DOWN,
    MSM_MPDEC_UP,
};

struct msm_mpdec_cpudata_t {
    struct mutex hotplug_mutex;
    int online;
    int device_suspended;
    cputime64_t on_time;
    cputime64_t on_time_total;
    long long unsigned int times_cpu_hotplugged;
    long long unsigned int times_cpu_unplugged;
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    struct mutex boost_mutex;
    struct mutex unboost_mutex;
    unsigned long int norm_min_freq;
    unsigned long int boost_freq;
    cputime64_t boost_until;
    bool is_boosted;
    bool revib_wq_running;
#endif
};
static DEFINE_PER_CPU(struct msm_mpdec_cpudata_t, msm_mpdec_cpudata);

static struct delayed_work msm_mpdec_work;
static struct workqueue_struct *msm_mpdec_workq;
static DEFINE_MUTEX(mpdec_msm_cpu_lock);
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
static struct workqueue_struct *mpdec_input_wq;
static DEFINE_PER_CPU(struct work_struct, mpdec_input_work);
static struct workqueue_struct *msm_mpdec_revib_workq;
static DEFINE_PER_CPU(struct delayed_work, msm_mpdec_revib_work);
#endif

static struct msm_mpdec_tuners {
    unsigned int startdelay;
    unsigned int delay;
    unsigned int pause;
    bool scroff_single_core;
    unsigned long int idle_freq;
    unsigned int max_cpus;
    unsigned int min_cpus;
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    bool boost_enabled;
    unsigned int boost_time;
    unsigned long int boost_freq[4];
#endif
} msm_mpdec_tuners_ins = {
    .startdelay = MSM_MPDEC_STARTDELAY,
    .delay = MSM_MPDEC_DELAY,
    .pause = MSM_MPDEC_PAUSE,
    .scroff_single_core = true,
    .idle_freq = MSM_MPDEC_IDLE_FREQ,
    .max_cpus = CONFIG_NR_CPUS,
    .min_cpus = 1,
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    .boost_enabled = true,
    .boost_time = MSM_MPDEC_BOOSTTIME,
    .boost_freq = {
        MSM_MPDEC_BOOSTFREQ_CPU0,
        MSM_MPDEC_BOOSTFREQ_CPU1,
        MSM_MPDEC_BOOSTFREQ_CPU2,
        MSM_MPDEC_BOOSTFREQ_CPU3
    },
#endif
};

static unsigned int NwNs_Threshold[8] = {12, 0, 25, 20, 25, 10, 0, 18};
static unsigned int TwTs_Threshold[8] = {140, 0, 140, 190, 140, 190, 0, 190};

extern unsigned int get_rq_info(void);
extern unsigned long acpuclk_get_rate(int);

unsigned int state = MSM_MPDEC_IDLE;
bool was_paused = false;
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
bool is_screen_on = true;
static int update_cpu_min_freq(struct cpufreq_policy *cpu_policy,
                               int cpu, int new_freq);
static void unboost_cpu(int cpu);
#endif
static cputime64_t mpdec_paused_until = 0;

static unsigned long get_rate(int cpu) {
    return acpuclk_get_rate(cpu);
}

static int get_slowest_cpu(void) {
    int i, cpu = 0;
    unsigned long rate, slow_rate = 0;

    for (i = 1; i < CONFIG_NR_CPUS; i++) {
        if (!cpu_online(i))
            continue;
        rate = get_rate(i);
        if (slow_rate == 0) {
            slow_rate = rate;
        }
        if ((rate <= slow_rate) && (slow_rate != 0)) {
            cpu = i;
            slow_rate = rate;
        }
    }

    return cpu;
}

static unsigned long get_slowest_cpu_rate(void) {
    int i = 0;
    unsigned long rate, slow_rate = 0;

    for (i = 0; i < CONFIG_NR_CPUS; i++) {
        if (!cpu_online(i))
            continue;
        rate = get_rate(i);
        if ((rate < slow_rate) && (slow_rate != 0)) {
            slow_rate = rate;
        }
        if (slow_rate == 0) {
            slow_rate = rate;
        }
    }

    return slow_rate;
}

static void mpdec_cpu_up(int cpu) {
    if (!cpu_online(cpu)) {
        mutex_lock(&per_cpu(msm_mpdec_cpudata, cpu).hotplug_mutex);
        cpu_up(cpu);
        per_cpu(msm_mpdec_cpudata, cpu).on_time = ktime_to_ms(ktime_get());
        per_cpu(msm_mpdec_cpudata, cpu).online = true;
        per_cpu(msm_mpdec_cpudata, cpu).times_cpu_hotplugged += 1;
        pr_info(MPDEC_TAG"CPU[%d] off->on | Mask=[%d%d%d%d]\n",
                cpu, cpu_online(0), cpu_online(1), cpu_online(2), cpu_online(3));
        mutex_unlock(&per_cpu(msm_mpdec_cpudata, cpu).hotplug_mutex);
    }
}
EXPORT_SYMBOL_GPL(mpdec_cpu_up);

static void mpdec_cpu_down(int cpu) {
    cputime64_t on_time = 0;
    if (cpu_online(cpu)) {
        mutex_lock(&per_cpu(msm_mpdec_cpudata, cpu).hotplug_mutex);
        cpu_down(cpu);
        on_time = (ktime_to_ms(ktime_get()) - per_cpu(msm_mpdec_cpudata, cpu).on_time);
        per_cpu(msm_mpdec_cpudata, cpu).online = false;
        per_cpu(msm_mpdec_cpudata, cpu).on_time_total += on_time;
        per_cpu(msm_mpdec_cpudata, cpu).times_cpu_unplugged += 1;
        pr_info(MPDEC_TAG"CPU[%d] on->off | Mask=[%d%d%d%d] | time online: %llu\n",
                cpu, cpu_online(0), cpu_online(1), cpu_online(2), cpu_online(3), on_time);
        mutex_unlock(&per_cpu(msm_mpdec_cpudata, cpu).hotplug_mutex);
    }
}
EXPORT_SYMBOL_GPL(mpdec_cpu_down);

static int mp_decision(void) {
    static bool first_call = true;
    int new_state = MSM_MPDEC_IDLE;
    int nr_cpu_online;
    int index;
    unsigned int rq_depth;
    static cputime64_t total_time = 0;
    static cputime64_t last_time;
    cputime64_t current_time;
    cputime64_t this_time = 0;

    if (state == MSM_MPDEC_DISABLED)
        return MSM_MPDEC_DISABLED;

    current_time = ktime_to_ms(ktime_get());
    if (current_time <= msm_mpdec_tuners_ins.startdelay)
        return MSM_MPDEC_IDLE;

    if (first_call) {
        first_call = false;
    } else {
        this_time = current_time - last_time;
    }
    total_time += this_time;

    rq_depth = get_rq_info();
    nr_cpu_online = num_online_cpus();

    if (nr_cpu_online) {
        index = (nr_cpu_online - 1) * 2;
        if ((nr_cpu_online < CONFIG_NR_CPUS) && (rq_depth >= NwNs_Threshold[index])) {
            if ((total_time >= TwTs_Threshold[index]) &&
                (nr_cpu_online < msm_mpdec_tuners_ins.max_cpus)) {
                new_state = MSM_MPDEC_UP;
                if (get_slowest_cpu_rate() <=  msm_mpdec_tuners_ins.idle_freq)
                    new_state = MSM_MPDEC_IDLE;
            }
        } else if ((nr_cpu_online > 1) && (rq_depth <= NwNs_Threshold[index+1])) {
            if ((total_time >= TwTs_Threshold[index+1]) &&
                (nr_cpu_online > msm_mpdec_tuners_ins.min_cpus)) {
                new_state = MSM_MPDEC_DOWN;
                if (get_slowest_cpu_rate() > msm_mpdec_tuners_ins.idle_freq)
                    new_state = MSM_MPDEC_IDLE;
            }
        } else {
            new_state = MSM_MPDEC_IDLE;
            total_time = 0;
        }
    } else {
        total_time = 0;
    }

    if (new_state != MSM_MPDEC_IDLE) {
        total_time = 0;
    }

    last_time = ktime_to_ms(ktime_get());
#if DEBUG
    pr_info(MPDEC_TAG"[DEBUG] rq: %u, new_state: %i | Mask=[%d%d%d%d]\n",
            rq_depth, new_state, cpu_online(0), cpu_online(1), cpu_online(2), cpu_online(3));
#endif
    return new_state;
}

static void msm_mpdec_work_thread(struct work_struct *work) {
    unsigned int cpu = nr_cpu_ids;
    bool suspended = false;

    if (ktime_to_ms(ktime_get()) <= msm_mpdec_tuners_ins.startdelay)
            goto out;

    /* Check if we are paused */
    if (mpdec_paused_until >= ktime_to_ms(ktime_get()))
            goto out;

    for_each_possible_cpu(cpu) {
        if ((per_cpu(msm_mpdec_cpudata, cpu).device_suspended == true)) {
            suspended = true;
            break;
        }
    }
    if (suspended == true)
        goto out;

    if (!mutex_trylock(&mpdec_msm_cpu_lock))
        goto out;

    /* if sth messed with the cpus, update the check vars so we can proceed */
    if (was_paused) {
        for_each_possible_cpu(cpu) {
            if (cpu_online(cpu))
                per_cpu(msm_mpdec_cpudata, cpu).online = true;
            else if (!cpu_online(cpu))
                per_cpu(msm_mpdec_cpudata, cpu).online = false;
        }
        was_paused = false;
    }

    state = mp_decision();
    switch (state) {
    case MSM_MPDEC_DISABLED:
    case MSM_MPDEC_IDLE:
        break;
    case MSM_MPDEC_DOWN:
        cpu = get_slowest_cpu();
        if (cpu < nr_cpu_ids) {
            if ((per_cpu(msm_mpdec_cpudata, cpu).online == true) && (cpu_online(cpu))) {
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
                unboost_cpu(cpu);
#endif
                mpdec_cpu_down(cpu);
            } else if (per_cpu(msm_mpdec_cpudata, cpu).online != cpu_online(cpu)) {
                pr_info(MPDEC_TAG"CPU[%d] was controlled outside of mpdecision! | pausing [%d]ms\n",
                        cpu, msm_mpdec_tuners_ins.pause);
                mpdec_paused_until = ktime_to_ms(ktime_get()) + msm_mpdec_tuners_ins.pause;
                was_paused = true;
            }
        }
        break;
    case MSM_MPDEC_UP:
        cpu = cpumask_next_zero(0, cpu_online_mask);
        if (cpu < nr_cpu_ids) {
            if ((per_cpu(msm_mpdec_cpudata, cpu).online == false) && (!cpu_online(cpu))) {
                mpdec_cpu_up(cpu);
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
                unboost_cpu(cpu);
#endif
            } else if (per_cpu(msm_mpdec_cpudata, cpu).online != cpu_online(cpu)) {
                pr_info(MPDEC_TAG"CPU[%d] was controlled outside of mpdecision! | pausing [%d]ms\n",
                        cpu, msm_mpdec_tuners_ins.pause);
                mpdec_paused_until = ktime_to_ms(ktime_get()) + msm_mpdec_tuners_ins.pause;
                was_paused = true;
            }
        }
        break;
    default:
        pr_err(MPDEC_TAG"%s: invalid mpdec hotplug state %d\n",
               __func__, state);
    }
    mutex_unlock(&mpdec_msm_cpu_lock);

out:
    if (state != MSM_MPDEC_DISABLED)
        queue_delayed_work(msm_mpdec_workq, &msm_mpdec_work,
                           msecs_to_jiffies(msm_mpdec_tuners_ins.delay));
    return;
}

#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
static int update_cpu_min_freq(struct cpufreq_policy *cpu_policy,
                               int cpu, int new_freq) {
    int ret = 0;

    if (!cpu_policy)
        return -EINVAL;

    cpufreq_verify_within_limits(cpu_policy, new_freq, cpu_policy->max);
    cpu_policy->user_policy.min = new_freq;

    ret = cpufreq_update_policy(cpu);
    if (!ret) {
        pr_debug(MPDEC_TAG"Touch event! Setting CPU%d min frequency to %d\n",
            cpu, new_freq);
    }
    return ret;
}

static void unboost_cpu(int cpu) {
    struct cpufreq_policy *cpu_policy = NULL;

    if (cpu_online(cpu)) {
        if (per_cpu(msm_mpdec_cpudata, cpu).is_boosted) {
            if (mutex_trylock(&per_cpu(msm_mpdec_cpudata, cpu).unboost_mutex)) {
                cpu_policy = cpufreq_cpu_get(cpu);
                if (!cpu_policy) {
                    pr_debug(MPDEC_TAG"NULL policy on cpu %d\n", cpu);
                    return;
                }
#if DEBUG
                pr_info(MPDEC_TAG"un boosted cpu%i to %lu", cpu, per_cpu(msm_mpdec_cpudata, cpu).norm_min_freq);
#endif
                per_cpu(msm_mpdec_cpudata, cpu).is_boosted = false;
                per_cpu(msm_mpdec_cpudata, cpu).revib_wq_running = false;
                if ((cpu_policy->min != per_cpu(msm_mpdec_cpudata, cpu).boost_freq) &&
                    (cpu_policy->min != per_cpu(msm_mpdec_cpudata, cpu).norm_min_freq)) {
                    pr_info(MPDEC_TAG"cpu%u min was changed while boosted (%lu->%u), using new min",
                            cpu, per_cpu(msm_mpdec_cpudata, cpu).norm_min_freq, cpu_policy->min);
                    per_cpu(msm_mpdec_cpudata, cpu).norm_min_freq = cpu_policy->min;
                }
                update_cpu_min_freq(cpu_policy, cpu, per_cpu(msm_mpdec_cpudata, cpu).norm_min_freq);
                cpufreq_cpu_put(cpu_policy);
                mutex_unlock(&per_cpu(msm_mpdec_cpudata, cpu).unboost_mutex);
            }
        }
    }

    return;
}

static void msm_mpdec_revib_work_thread(struct work_struct *work) {
    int cpu = smp_processor_id();

    if (per_cpu(msm_mpdec_cpudata, cpu).is_boosted) {
        per_cpu(msm_mpdec_cpudata, cpu).revib_wq_running = true;
        if (ktime_to_ms(ktime_get()) > per_cpu(msm_mpdec_cpudata, cpu).boost_until) {
            unboost_cpu(cpu);
        } else {
            queue_delayed_work_on(cpu,
                                  msm_mpdec_revib_workq,
                                  &per_cpu(msm_mpdec_revib_work, cpu),
                                  msecs_to_jiffies((per_cpu(msm_mpdec_cpudata, cpu).boost_until - ktime_to_ms(ktime_get()))));
        }
    } else {
        per_cpu(msm_mpdec_cpudata, cpu).revib_wq_running = false;
    }
    return;
}

static void mpdec_input_callback(struct work_struct *unused) {
    struct cpufreq_policy *cpu_policy = NULL;
    int cpu = smp_processor_id();
    bool boosted = false;

    if (!per_cpu(msm_mpdec_cpudata, cpu).is_boosted) {
        if (mutex_trylock(&per_cpu(msm_mpdec_cpudata, cpu).boost_mutex)) {
            cpu_policy = cpufreq_cpu_get(cpu);
            if (!cpu_policy) {
                pr_debug(MPDEC_TAG"NULL policy on cpu %d\n", cpu);
                return;
            }
            per_cpu(msm_mpdec_cpudata, cpu).norm_min_freq = cpu_policy->min;
            update_cpu_min_freq(cpu_policy, cpu, per_cpu(msm_mpdec_cpudata, cpu).boost_freq);
#if DEBUG
            pr_info(MPDEC_TAG"boosted cpu%i to %lu", cpu, per_cpu(msm_mpdec_cpudata, cpu).boost_freq);
#endif
            per_cpu(msm_mpdec_cpudata, cpu).is_boosted = true;
            per_cpu(msm_mpdec_cpudata, cpu).boost_until = ktime_to_ms(ktime_get()) + MSM_MPDEC_BOOSTTIME;
            boosted = true;
            cpufreq_cpu_put(cpu_policy);
            mutex_unlock(&per_cpu(msm_mpdec_cpudata, cpu).boost_mutex);
        }
    } else {
        boosted = true;
    }
    if (boosted && !per_cpu(msm_mpdec_cpudata, cpu).revib_wq_running) {
        per_cpu(msm_mpdec_cpudata, cpu).revib_wq_running = true;
        queue_delayed_work_on(cpu,
                              msm_mpdec_revib_workq,
                              &per_cpu(msm_mpdec_revib_work, cpu),
                              msecs_to_jiffies(MSM_MPDEC_BOOSTTIME));
    } else if (boosted && per_cpu(msm_mpdec_cpudata, cpu).revib_wq_running) {
        per_cpu(msm_mpdec_cpudata, cpu).boost_until = ktime_to_ms(ktime_get()) + MSM_MPDEC_BOOSTTIME;
    }

    return;
}

static void mpdec_input_event(struct input_handle *handle, unsigned int type,
        unsigned int code, int value) {
    int i = 0;

    if (!msm_mpdec_tuners_ins.boost_enabled)
        return;

    if (!is_screen_on)
        return;

    for_each_online_cpu(i) {
        queue_work_on(i, mpdec_input_wq, &per_cpu(mpdec_input_work, i));
    }
}

static int input_dev_filter(const char *input_dev_name) {
    if (strstr(input_dev_name, "touch") ||
        strstr(input_dev_name, "keypad")) {
        return 0;
    } else {
        return 1;
    }
}

static int mpdec_input_connect(struct input_handler *handler,
        struct input_dev *dev, const struct input_device_id *id) {
    struct input_handle *handle;
    int error;

    if (input_dev_filter(dev->name))
        return -ENODEV;

    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "mpdec";

    error = input_register_handle(handle);
    if (error)
        goto err2;

    error = input_open_device(handle);
    if (error)
        goto err1;

    return 0;
err1:
    input_unregister_handle(handle);
err2:
    kfree(handle);
    return error;
}

static void mpdec_input_disconnect(struct input_handle *handle) {
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

static const struct input_device_id mpdec_ids[] = {
    { .driver_info = 1 },
    { },
};

static struct input_handler mpdec_input_handler = {
    .event        = mpdec_input_event,
    .connect      = mpdec_input_connect,
    .disconnect   = mpdec_input_disconnect,
    .name         = "mpdec_inputreq",
    .id_table     = mpdec_ids,
};
#endif

static void msm_mpdec_early_suspend(struct early_suspend *h) {
    int cpu = nr_cpu_ids;
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    is_screen_on = false;
#endif

    if (!msm_mpdec_tuners_ins.scroff_single_core) {
        pr_info(MPDEC_TAG"Screen -> off\n");
        return;
    }

    /* main work thread can sleep now */
    cancel_delayed_work_sync(&msm_mpdec_work);

    for_each_possible_cpu(cpu) {
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
        unboost_cpu(cpu);
#endif
        if ((cpu >= 1) && (cpu_online(cpu))) {
            mpdec_cpu_down(cpu);
        }
        per_cpu(msm_mpdec_cpudata, cpu).device_suspended = true;
    }

    pr_info(MPDEC_TAG"Screen -> off. Deactivated mpdecision.\n");
}

static void msm_mpdec_late_resume(struct early_suspend *h) {
    int cpu = nr_cpu_ids;
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    is_screen_on = true;
#endif

    for_each_possible_cpu(cpu)
        per_cpu(msm_mpdec_cpudata, cpu).device_suspended = false;

    if (msm_mpdec_tuners_ins.scroff_single_core) {
        /* wake up main work thread */
        was_paused = true;
        queue_delayed_work(msm_mpdec_workq, &msm_mpdec_work, 0);

        /* restore min/max cpus limits */
        for (cpu=1; cpu<CONFIG_NR_CPUS; cpu++) {
            if (cpu < msm_mpdec_tuners_ins.min_cpus) {
                if (!cpu_online(cpu))
                    mpdec_cpu_up(cpu);
            } else if (cpu > msm_mpdec_tuners_ins.max_cpus) {
                if (cpu_online(cpu))
                    mpdec_cpu_down(cpu);
            }
        }

        pr_info(MPDEC_TAG"Screen -> on. Activated mpdecision. | Mask=[%d%d%d%d]\n",
                cpu_online(0), cpu_online(1), cpu_online(2), cpu_online(3));
    } else {
        pr_info(MPDEC_TAG"Screen -> on\n");
    }
}

static struct early_suspend msm_mpdec_early_suspend_handler = {
    .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
    .suspend = msm_mpdec_early_suspend,
    .resume = msm_mpdec_late_resume,
};

/**************************** SYSFS START ****************************/
struct kobject *msm_mpdec_kobject;

#define show_one(file_name, object)                    \
static ssize_t show_##file_name                        \
(struct kobject *kobj, struct attribute *attr, char *buf)               \
{                                    \
    return sprintf(buf, "%u\n", msm_mpdec_tuners_ins.object);    \
}

show_one(startdelay, startdelay);
show_one(delay, delay);
show_one(pause, pause);
show_one(scroff_single_core, scroff_single_core);
show_one(min_cpus, min_cpus);
show_one(max_cpus, max_cpus);
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
show_one(boost_enabled, boost_enabled);
show_one(boost_time, boost_time);
#endif

#define show_one_twts(file_name, arraypos)                              \
static ssize_t show_##file_name                                         \
(struct kobject *kobj, struct attribute *attr, char *buf)               \
{                                                                       \
    return sprintf(buf, "%u\n", TwTs_Threshold[arraypos]);          \
}
show_one_twts(twts_threshold_0, 0);
show_one_twts(twts_threshold_1, 1);
show_one_twts(twts_threshold_2, 2);
show_one_twts(twts_threshold_3, 3);
show_one_twts(twts_threshold_4, 4);
show_one_twts(twts_threshold_5, 5);
show_one_twts(twts_threshold_6, 6);
show_one_twts(twts_threshold_7, 7);

#define store_one_twts(file_name, arraypos)                             \
static ssize_t store_##file_name                                        \
(struct kobject *a, struct attribute *b, const char *buf, size_t count) \
{                                                                       \
    unsigned int input;                                             \
    int ret;                                                        \
    ret = sscanf(buf, "%u", &input);                                \
    if (ret != 1)                                                   \
        return -EINVAL;                                         \
    TwTs_Threshold[arraypos] = input;                               \
    return count;                                                   \
}                                                                       \
define_one_global_rw(file_name);
store_one_twts(twts_threshold_0, 0);
store_one_twts(twts_threshold_1, 1);
store_one_twts(twts_threshold_2, 2);
store_one_twts(twts_threshold_3, 3);
store_one_twts(twts_threshold_4, 4);
store_one_twts(twts_threshold_5, 5);
store_one_twts(twts_threshold_6, 6);
store_one_twts(twts_threshold_7, 7);

#define show_one_nwns(file_name, arraypos)                              \
static ssize_t show_##file_name                                         \
(struct kobject *kobj, struct attribute *attr, char *buf)               \
{                                                                       \
    return sprintf(buf, "%u\n", NwNs_Threshold[arraypos]);          \
}
show_one_nwns(nwns_threshold_0, 0);
show_one_nwns(nwns_threshold_1, 1);
show_one_nwns(nwns_threshold_2, 2);
show_one_nwns(nwns_threshold_3, 3);
show_one_nwns(nwns_threshold_4, 4);
show_one_nwns(nwns_threshold_5, 5);
show_one_nwns(nwns_threshold_6, 6);
show_one_nwns(nwns_threshold_7, 7);

#define store_one_nwns(file_name, arraypos)                             \
static ssize_t store_##file_name                                        \
(struct kobject *a, struct attribute *b, const char *buf, size_t count) \
{                                                                       \
    unsigned int input;                                             \
    int ret;                                                        \
    ret = sscanf(buf, "%u", &input);                                \
    if (ret != 1)                                                   \
        return -EINVAL;                                         \
    NwNs_Threshold[arraypos] = input;                               \
    return count;                                                   \
}                                                                       \
define_one_global_rw(file_name);
store_one_nwns(nwns_threshold_0, 0);
store_one_nwns(nwns_threshold_1, 1);
store_one_nwns(nwns_threshold_2, 2);
store_one_nwns(nwns_threshold_3, 3);
store_one_nwns(nwns_threshold_4, 4);
store_one_nwns(nwns_threshold_5, 5);
store_one_nwns(nwns_threshold_6, 6);
store_one_nwns(nwns_threshold_7, 7);

static ssize_t show_idle_freq (struct kobject *kobj, struct attribute *attr,
					char *buf)
{
    return sprintf(buf, "%lu\n", msm_mpdec_tuners_ins.idle_freq);
}

static ssize_t show_enabled(struct kobject *a, struct attribute *b,
                   char *buf)
{
    unsigned int enabled;
    switch (state) {
    case MSM_MPDEC_DISABLED:
        enabled = 0;
        break;
    case MSM_MPDEC_IDLE:
    case MSM_MPDEC_DOWN:
    case MSM_MPDEC_UP:
        enabled = 1;
        break;
    default:
        enabled = 333;
    }
    return sprintf(buf, "%u\n", enabled);
}

static ssize_t store_startdelay(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int input;
    int ret;
    ret = sscanf(buf, "%u", &input);
    if (ret != 1)
        return -EINVAL;

    msm_mpdec_tuners_ins.startdelay = input;

    return count;
}

static ssize_t store_delay(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int input;
    int ret;
    ret = sscanf(buf, "%u", &input);
    if (ret != 1)
        return -EINVAL;

    msm_mpdec_tuners_ins.delay = input;

    return count;
}

static ssize_t store_pause(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int input;
    int ret;
    ret = sscanf(buf, "%u", &input);
    if (ret != 1)
        return -EINVAL;

    msm_mpdec_tuners_ins.pause = input;

    return count;
}

static ssize_t store_idle_freq(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    long unsigned int input;
    int ret;
    ret = sscanf(buf, "%lu", &input);
    if (ret != 1)
        return -EINVAL;

    msm_mpdec_tuners_ins.idle_freq = input;

    return count;
}

static ssize_t store_scroff_single_core(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int input;
    int ret;
    ret = sscanf(buf, "%u", &input);
    if (ret != 1)
        return -EINVAL;
    switch (buf[0]) {
        case '0':
        case '1':
            msm_mpdec_tuners_ins.scroff_single_core = input;
            break;
        default:
            ret = -EINVAL;
    }
    return count;
}

static ssize_t store_max_cpus(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int input;
    int ret, cpu;
    ret = sscanf(buf, "%u", &input);
    if ((ret != 1) || input > CONFIG_NR_CPUS || input < msm_mpdec_tuners_ins.min_cpus)
                return -EINVAL;

    msm_mpdec_tuners_ins.max_cpus = input;
    if (num_online_cpus() > input) {
        for (cpu=CONFIG_NR_CPUS; cpu>0; cpu--) {
            if (num_online_cpus() <= input)
                break;
            if (!cpu_online(cpu))
                continue;
            mpdec_cpu_down(cpu);
        }
        pr_info(MPDEC_TAG"max_cpus set to %u. Affected CPUs were unplugged!\n", input);
    }

    return count;
}

static ssize_t store_min_cpus(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int input;
    int ret, cpu;
    ret = sscanf(buf, "%u", &input);
    if ((ret != 1) || input < 1 || input > msm_mpdec_tuners_ins.max_cpus)
        return -EINVAL;

    msm_mpdec_tuners_ins.min_cpus = input;
    if (num_online_cpus() < input) {
        for (cpu=1; cpu<CONFIG_NR_CPUS; cpu++) {
            if (num_online_cpus() >= input)
                break;
            if (cpu_online(cpu))
                continue;
            mpdec_cpu_up(cpu);
        }
        pr_info(MPDEC_TAG"min_cpus set to %u. Affected CPUs were hotplugged!\n", input);
    }

    return count;
}

static ssize_t store_enabled(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int cpu, input, enabled;
    int ret;
    ret = sscanf(buf, "%u", &input);
    if (ret != 1)
        return -EINVAL;

    switch (state) {
    case MSM_MPDEC_DISABLED:
        enabled = 0;
        break;
    case MSM_MPDEC_IDLE:
    case MSM_MPDEC_DOWN:
    case MSM_MPDEC_UP:
        enabled = 1;
        break;
    default:
        enabled = 333;
    }

    if (buf[0] == enabled)
        return -EINVAL;

    switch (buf[0]) {
    case '0':
        state = MSM_MPDEC_DISABLED;
        pr_info(MPDEC_TAG"nap time... Hot plugging offline CPUs...\n");
        for (cpu = 1; cpu < CONFIG_NR_CPUS; cpu++)
            if (!cpu_online(cpu))
                mpdec_cpu_up(cpu);
        break;
    case '1':
        state = MSM_MPDEC_IDLE;
        was_paused = true;
        queue_delayed_work(msm_mpdec_workq, &msm_mpdec_work,
                           msecs_to_jiffies(msm_mpdec_tuners_ins.delay));
        pr_info(MPDEC_TAG"firing up mpdecision...\n");
        break;
    default:
        ret = -EINVAL;
    }
    return count;
}

#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
static ssize_t store_boost_enabled(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int input;
    int ret;
    ret = sscanf(buf, "%u", &input);
    if (ret != 1)
        return -EINVAL;

    msm_mpdec_tuners_ins.boost_enabled = input;

    return count;
}

static ssize_t store_boost_time(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    unsigned int input;
    int ret;
    ret = sscanf(buf, "%u", &input);
    if (ret != 1)
        return -EINVAL;

    msm_mpdec_tuners_ins.boost_time = input;

    return count;
}

static ssize_t show_boost_freqs(struct kobject *a, struct attribute *b,
                   char *buf)
{
    ssize_t len = 0;
    int cpu = 0;

    for_each_present_cpu(cpu) {
        len += sprintf(buf + len, "%lu\n", per_cpu(msm_mpdec_cpudata, cpu).boost_freq);
    }
    return len;
}
static ssize_t store_boost_freqs(struct kobject *a, struct attribute *b,
                   const char *buf, size_t count)
{
    int i = 0;
    unsigned int cpu = 0;
    long unsigned int hz = 0;
    const char *chz = NULL;

    for (i=0; i<count; i++) {
        if (buf[i] == ' ') {
            sscanf(&buf[(i-1)], "%u", &cpu);
            chz = &buf[(i+1)];
        }
    }
    sscanf(chz, "%lu", &hz);

    /* if this cpu is currently boosted, unboost */
    unboost_cpu(cpu);

    /* update boost freq */
    per_cpu(msm_mpdec_cpudata, cpu).boost_freq = hz;

    return count;
}
define_one_global_rw(boost_freqs);
#endif

define_one_global_rw(startdelay);
define_one_global_rw(delay);
define_one_global_rw(pause);
define_one_global_rw(scroff_single_core);
define_one_global_rw(idle_freq);
define_one_global_rw(min_cpus);
define_one_global_rw(max_cpus);
define_one_global_rw(enabled);
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
define_one_global_rw(boost_enabled);
define_one_global_rw(boost_time);
#endif

static struct attribute *msm_mpdec_attributes[] = {
    &startdelay.attr,
    &delay.attr,
    &pause.attr,
    &scroff_single_core.attr,
    &idle_freq.attr,
    &min_cpus.attr,
    &max_cpus.attr,
    &enabled.attr,
    &twts_threshold_0.attr,
    &twts_threshold_1.attr,
    &twts_threshold_2.attr,
    &twts_threshold_3.attr,
    &twts_threshold_4.attr,
    &twts_threshold_5.attr,
    &twts_threshold_6.attr,
    &twts_threshold_7.attr,
    &nwns_threshold_0.attr,
    &nwns_threshold_1.attr,
    &nwns_threshold_2.attr,
    &nwns_threshold_3.attr,
    &nwns_threshold_4.attr,
    &nwns_threshold_5.attr,
    &nwns_threshold_6.attr,
    &nwns_threshold_7.attr,
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    &boost_freqs.attr,
    &boost_enabled.attr,
    &boost_time.attr,
#endif
    NULL
};


static struct attribute_group msm_mpdec_attr_group = {
    .attrs = msm_mpdec_attributes,
    .name = "conf",
};

/********* STATS START *********/

static ssize_t show_time_cpus_on(struct kobject *a, struct attribute *b,
                   char *buf)
{
    ssize_t len = 0;
    int cpu = 0;

    for_each_possible_cpu(cpu) {
        if (cpu_online(cpu)) {
            len += sprintf(buf + len, "%i %llu\n", cpu,
                           (per_cpu(msm_mpdec_cpudata, cpu).on_time_total +
                            (ktime_to_ms(ktime_get()) -
                             per_cpu(msm_mpdec_cpudata, cpu).on_time)));
        } else
            len += sprintf(buf + len, "%i %llu\n", cpu, per_cpu(msm_mpdec_cpudata, cpu).on_time_total);
    }

    return len;
}
define_one_global_ro(time_cpus_on);

static ssize_t show_times_cpus_hotplugged(struct kobject *a, struct attribute *b,
                   char *buf)
{
    ssize_t len = 0;
    int cpu = 0;

    for_each_possible_cpu(cpu) {
        len += sprintf(buf + len, "%i %llu\n", cpu, per_cpu(msm_mpdec_cpudata, cpu).times_cpu_hotplugged);
    }

    return len;
}
define_one_global_ro(times_cpus_hotplugged);

static ssize_t show_times_cpus_unplugged(struct kobject *a, struct attribute *b,
                   char *buf)
{
    ssize_t len = 0;
    int cpu = 0;

    for_each_possible_cpu(cpu) {
        len += sprintf(buf + len, "%i %llu\n", cpu, per_cpu(msm_mpdec_cpudata, cpu).times_cpu_unplugged);
    }

    return len;
}
define_one_global_ro(times_cpus_unplugged);

static struct attribute *msm_mpdec_stats_attributes[] = {
    &time_cpus_on.attr,
    &times_cpus_hotplugged.attr,
    &times_cpus_unplugged.attr,
    NULL
};


static struct attribute_group msm_mpdec_stats_attr_group = {
    .attrs = msm_mpdec_stats_attributes,
    .name = "stats",
};
/**************************** SYSFS END ****************************/

static int __init msm_mpdec_init(void) {
    int cpu, rc, err = 0;
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    int i;
    unsigned long int boost_freq = 0;
    struct cpufreq_policy *policy;
    unsigned int ret = -EINVAL;
#endif

    for_each_possible_cpu(cpu) {
        mutex_init(&(per_cpu(msm_mpdec_cpudata, cpu).hotplug_mutex));
        per_cpu(msm_mpdec_cpudata, cpu).device_suspended = false;
        per_cpu(msm_mpdec_cpudata, cpu).online = true;
        per_cpu(msm_mpdec_cpudata, cpu).on_time_total = 0;
        per_cpu(msm_mpdec_cpudata, cpu).times_cpu_unplugged = 0;
        per_cpu(msm_mpdec_cpudata, cpu).times_cpu_hotplugged = 0;
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
        policy=cpufreq_cpu_get(cpu);
        if(policy == NULL)
            return -EINVAL;

        per_cpu(msm_mpdec_cpudata, cpu).norm_min_freq = policy->user_policy.min;
        switch (cpu) {
            case 0:
            case 1:
            case 2:
                boost_freq = msm_mpdec_tuners_ins.boost_freq[cpu];
                break;
            default:
                boost_freq = msm_mpdec_tuners_ins.boost_freq[3];
                break;
        }
        per_cpu(msm_mpdec_cpudata, cpu).boost_freq = boost_freq;
        per_cpu(msm_mpdec_cpudata, cpu).is_boosted = false;
        per_cpu(msm_mpdec_cpudata, cpu).revib_wq_running = false;
        per_cpu(msm_mpdec_cpudata, cpu).boost_until = 0;
        mutex_init(&(per_cpu(msm_mpdec_cpudata, cpu).boost_mutex));
        mutex_init(&(per_cpu(msm_mpdec_cpudata, cpu).unboost_mutex));
#endif
    }

    was_paused = true;

    msm_mpdec_workq = alloc_workqueue("mpdec",
                                      WQ_UNBOUND | WQ_RESCUER | WQ_FREEZABLE,
                                      1);
    if (!msm_mpdec_workq)
        return -ENOMEM;
    INIT_DELAYED_WORK(&msm_mpdec_work, msm_mpdec_work_thread);

#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    mpdec_input_wq = create_workqueue("mpdeciwq");
    if (!mpdec_input_wq) {
        printk(KERN_ERR "%s: Failed to create mpdeciwq workqueue\n", __func__);
        return -EFAULT;
    }
    msm_mpdec_revib_workq = create_workqueue("mpdecribwq");
    if (!msm_mpdec_revib_workq) {
        printk(KERN_ERR "%s: Failed to create mpdecrevibwq workqueue\n", __func__);
        return -EFAULT;
    }
    for_each_possible_cpu(i) {
        INIT_WORK(&per_cpu(mpdec_input_work, i), mpdec_input_callback);
        INIT_DELAYED_WORK(&per_cpu(msm_mpdec_revib_work, i), msm_mpdec_revib_work_thread);
    }
    rc = input_register_handler(&mpdec_input_handler);
#endif

    if (state != MSM_MPDEC_DISABLED)
        queue_delayed_work(msm_mpdec_workq, &msm_mpdec_work,
                           msecs_to_jiffies(msm_mpdec_tuners_ins.delay));

    register_early_suspend(&msm_mpdec_early_suspend_handler);

    msm_mpdec_kobject = kobject_create_and_add("msm_mpdecision", kernel_kobj);
    if (msm_mpdec_kobject) {
        rc = sysfs_create_group(msm_mpdec_kobject,
                                &msm_mpdec_attr_group);
        if (rc) {
            pr_warn(MPDEC_TAG"sysfs: ERROR, could not create sysfs group");
        }
        rc = sysfs_create_group(msm_mpdec_kobject,
                                &msm_mpdec_stats_attr_group);
        if (rc) {
            pr_warn(MPDEC_TAG"sysfs: ERROR, could not create sysfs stats group");
        }
    } else
        pr_warn(MPDEC_TAG"sysfs: ERROR, could not create sysfs kobj");

    pr_info(MPDEC_TAG"%s init complete.", __func__);

    return err;
}
late_initcall(msm_mpdec_init);

void msm_mpdec_exit(void) {
#ifdef CONFIG_MSM_MPDEC_INPUTBOOST_CPUMIN
    input_unregister_handler(&mpdec_input_handler);
    destroy_workqueue(msm_mpdec_revib_workq);
    destroy_workqueue(mpdec_input_wq);
#endif
    destroy_workqueue(msm_mpdec_workq);
}
