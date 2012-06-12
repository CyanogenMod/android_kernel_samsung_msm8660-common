#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sched.h>
#include <linux/semaphore.h>

#if defined(CONFIG_SEC_DEBUG)

//#define CONFIG_SEC_DEBUG_REGRW_LOG

extern int sec_debug_init(void);
extern int sec_debug_dump_stack(void);
extern void sec_debug_hw_reset(void);
extern void sec_debug_check_crash_key(unsigned int code, int value);
extern void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y, u32 bpp,
				     u32 frames);
extern void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
				      u32 addr1);
extern void sec_getlog_supply_loggerinfo(void *p_main, void *p_radio,
					 void *p_events, void *p_system);
extern void sec_getlog_supply_kloginfo(void *klog_buf);

extern void sec_gaf_supply_rqinfo(unsigned short curr_offset,
				  unsigned short rq_offset);
extern int sec_debug_is_enabled(void);
#else
static inline int sec_debug_init(void)
{
}
static inline int sec_debug_dump_stack(void) {}
static inline void sec_debug_hw_reset(void){}
static inline void sec_debug_check_crash_key(unsigned int code, int value) {}

static inline void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y,
					    u32 bpp, u32 frames)
{
}

static inline void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
					     u32 addr1)
{
}

static inline void sec_getlog_supply_loggerinfo(void *p_main,
						void *p_radio, void *p_events,
						void *p_system)
{
}

static inline void sec_getlog_supply_kloginfo(void *klog_buf)
{
}

static inline void sec_gaf_supply_rqinfo(unsigned short curr_offset,
					 unsigned short rq_offset)
{
}
static inline int sec_debug_is_enabled(void) {return 0;}
#endif

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
extern void sec_debug_task_sched_log(int cpu, struct task_struct *task);
extern void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en);
extern void sec_debug_irq_sched_log_end(void);
extern void sec_debug_timer_log(unsigned int type, int int_lock, void *fn);
extern void sec_debug_sched_log_init(void);
#else
static inline void sec_debug_task_sched_log(int cpu, struct task_struct *task)
{
}
static inline void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en)
{
}
static inline void sec_debug_irq_sched_log_end(void)
{
}
static inline void sec_debug_timer_log(unsigned int type, int int_lock, void *fn)
{
}
static inline void sec_debug_sched_log_init(void)
{
}
#endif
#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
extern void sec_debug_irq_enterexit_log(unsigned int irq, unsigned long long start_time);
#else
static inline void sec_debug_irq_enterexit_log(unsigned int irq, unsigned long long start_time)
{
}
#endif

#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
extern void debug_semaphore_init(void);
extern void debug_semaphore_down_log(struct semaphore *sem);
extern void debug_semaphore_up_log(struct semaphore *sem);
extern void debug_rwsemaphore_init(void);
extern void debug_rwsemaphore_down_log(struct rw_semaphore *sem, int dir);
extern void debug_rwsemaphore_up_log(struct rw_semaphore *sem);
#define debug_rwsemaphore_down_read_log(x) \
	debug_rwsemaphore_down_log(x,READ_SEM)
#define debug_rwsemaphore_down_write_log(x) \
	debug_rwsemaphore_down_log(x,WRITE_SEM)
#else
static inline void debug_semaphore_init(void)
{
}
static inline void debug_semaphore_down_log(struct semaphore *sem)
{
}
static inline void debug_semaphore_up_log(struct semaphore *sem)
{
}
static inline void debug_rwsemaphore_init(void)
{
}
static inline void debug_rwsemaphore_down_read_log(struct rw_semaphore *sem)
{
}
static inline void debug_rwsemaphore_down_write_log(struct rw_semaphore *sem)
{
}
static inline void debug_rwsemaphore_up_log(struct rw_semaphore *sem)
{
}
#endif

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX 1024

struct irq_log{
	unsigned long long time;
	int irq;
	void *fn;
	int en;
	unsigned long long elapsed_time;
};

struct softirq_log{
	unsigned long long time;
	void *fn;
	unsigned long long elapsed_time;
};

struct enterexit_log{
    unsigned int irq;
	unsigned long long time;
	unsigned long long end_time;
	unsigned long long elapsed_time;
};

struct task_info{
	unsigned long long time;
	char comm[TASK_COMM_LEN];
	pid_t pid;
};

union task_log{
	struct task_info task;
	struct irq_log irq;
};

struct sched_log{
	unsigned long long time;
	union task_log log;
};

struct timer_log{
	unsigned long long time;
	unsigned int type;
	int int_lock;
	void* fn;
};
#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */

#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
#define SEMAPHORE_LOG_MAX 100
struct sem_debug{
	struct list_head list;
	struct semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	/* char comm[TASK_COMM_LEN]; */
};

enum {
	READ_SEM,
	WRITE_SEM
};

#define RWSEMAPHORE_LOG_MAX 100
struct rwsem_debug{
	struct list_head list;
	struct rw_semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	int direction;
	/* char comm[TASK_COMM_LEN]; */
};

#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
#define DCVS_LOG_MAX 256

struct dcvs_debug {
	unsigned long long time;
	int cpu_no;
	unsigned int prev_freq;
	unsigned int new_freq;
};

extern void sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq, unsigned int new_freq);
#endif

#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
#define FG_LOG_MAX 128

struct fuelgauge_debug {
	unsigned long long time;
	unsigned int voltage;
	unsigned short soc;
	unsigned short charging_status;
};

extern void sec_debug_fuelgauge_log(unsigned int voltage, unsigned short soc, unsigned short charging_status);
#endif

#ifdef CONFIG_SEC_DEBUG_MDP_LOG
#define MDP_LOG_MAX 1024

struct mdp_log {
	unsigned long long time;
	unsigned int value1;
	unsigned int value2;
};

extern void sec_debug_mdp_log(unsigned int value1, unsigned int value2);

extern void sec_debug_mdp_enable(int enable);


#else
static inline void sec_debug_mdp_log(unsigned int value1, unsigned int value2)
{
}

static inline void sec_debug_mdp_enable(int enable)
{
}

#endif

#ifdef CONFIG_SEC_DEBUG_POWERCOLLAPSE_LOG
#define POWERCOLLAPSE_LOG_MAX 1024

struct powercollapse_log {
	unsigned long long time;
	unsigned int value1;
	unsigned int value2;
};

extern void sec_debug_powercollapse_log(unsigned int value1, unsigned int value2);

#else
static inline void sec_debug_powercollapse_log(unsigned int value1, unsigned int value2)
{
}

#endif

#ifdef CONFIG_SEC_DEBUG_SDIO_LOG
#define SDIO_LOG_MAX 1024

struct sdio_log {
	unsigned long long time;
	unsigned int value1;
	unsigned int value2;
};

extern void sec_debug_sdio_log(unsigned int value1, unsigned int value2);

extern void sec_debug_sdio_enable(int enable);


#else
static inline void sec_debug_sdio_log(unsigned int value1, unsigned int value2)
{
}

static inline void sec_debug_sdio_enable(int enable)
{
}

#endif

#ifdef CONFIG_SEC_DEBUG_REGRW_LOG
#define REGRW_LOG_MAX 1024

struct regrw_log {
	unsigned long addr;
	unsigned long data;
	unsigned long size_rw; /* size and rw : size at high 2 bytes, rw at low 2 */
	unsigned long done;
};

extern unsigned int sec_debug_reg_read(unsigned long addr, int size);
extern void sec_debug_reg_write(unsigned long addr, unsigned long data, int size);
#endif

/* for sec debug level */
#define KERNEL_SEC_DEBUG_LEVEL_LOW	(0x574F4C44)
#define KERNEL_SEC_DEBUG_LEVEL_MID	(0x44494D44)
#define KERNEL_SEC_DEBUG_LEVEL_HIGH	(0x47494844)
extern bool kernel_sec_set_debug_level(int level);
extern int kernel_sec_get_debug_level(void);

#define LOCAL_CONFIG_PRINT_EXTRA_INFO

#endif /* SEC_DEBUG_H */
