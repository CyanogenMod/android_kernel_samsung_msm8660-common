/*
 *  sec_debug.c
 *
 */

#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/sysrq.h>
#if 0 /* onlyjazz.out */
#include <mach/regs-pmu.h>
#endif
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/sec_param.h>

#include <mach/system.h>
#include <mach/sec_debug.h>
#include <mach/msm_iomap.h>
#include <linux/seq_file.h>
// #include <linux/smp_lock.h>
#include <mach/sec_switch.h>

#define RESTART_REASON_ADDR 0x65C

#define LOCKUP_FIRST_KEY KEY_VOLUMEUP
#define LOCKUP_SECOND_KEY KEY_VOLUMEDOWN
#define LOCKUP_THIRD_KEY KEY_POWER
#define LOCKUP_EXT_KEY KEY_HOME

#define RESET_REASON_NORMAL			0x1A2B3C00
#define RESET_REASON_SMPL			0x1A2B3C01
#define RESET_REASON_WSTR			0x1A2B3C02
#define RESET_REASON_WATCHDOG 			0x1A2B3C03
#define RESET_REASON_PANIC			0x1A2B3C04
#define RESET_REASON_LPM			0x1A2B3C10
#define RESET_REASON_RECOVERY			0x1A2B3C11
#define RESET_REASON_FOTA			0x1A2B3C12

enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT = 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC		= 0x11111111,
	UPLOAD_CAUSE_FORCED_UPLOAD		= 0x22222222,
	UPLOAD_CAUSE_USER_FAULT			= 0x33333333,
	UPLOAD_CAUSE_CP_ERROR_FATAL		= 0x66666666,
	UPLOAD_CAUSE_LPASS_ERROR_FATAL	= 0x77777777,
	UPLOAD_CAUSE_MDM_ERROR_FATAL	= 0xAAAAAAAA,
};

extern int sec_switch_get_attached_device(void);

struct sec_debug_mmu_reg_t {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};

/* ARM CORE regs mapping structure */
struct sec_debug_core_t {
	/* COMMON */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;

	/* SVC */
	unsigned int r13_svc;
	unsigned int r14_svc;
	unsigned int spsr_svc;

	/* PC & CPSR */
	unsigned int pc;
	unsigned int cpsr;

	/* USR/SYS */
	unsigned int r13_usr;
	unsigned int r14_usr;

	/* FIQ */
	unsigned int r8_fiq;
	unsigned int r9_fiq;
	unsigned int r10_fiq;
	unsigned int r11_fiq;
	unsigned int r12_fiq;
	unsigned int r13_fiq;
	unsigned int r14_fiq;
	unsigned int spsr_fiq;

	/* IRQ */
	unsigned int r13_irq;
	unsigned int r14_irq;
	unsigned int spsr_irq;

	/* MON */
	unsigned int r13_mon;
	unsigned int r14_mon;
	unsigned int spsr_mon;

	/* ABT */
	unsigned int r13_abt;
	unsigned int r14_abt;
	unsigned int spsr_abt;

	/* UNDEF */
	unsigned int r13_und;
	unsigned int r14_und;
	unsigned int spsr_und;

};

/* enable sec_debug feature */
static unsigned enable = 1;
static unsigned enable_user = 1;
static unsigned reset_reason = 0xFFEEFFEE;
module_param_named(enable, enable, uint, 0644);
module_param_named(enable_user, enable_user, uint, 0644);
module_param_named(reset_reason, reset_reason, uint, 0644);

extern unsigned int get_hw_rev(void);

static const char *gkernel_sec_build_info_date_time[] ={
	__DATE__,
	__TIME__
};

static char gkernel_sec_build_info[100];

unsigned int sec_dbg_buf_base	= 0;
unsigned int sec_dbg_buf_size	= 0;

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG

static struct task_info gExcpTaskLog[2][SCHED_LOG_MAX] __cacheline_aligned;
// static struct irq_log gExcpIrqLog[2][SCHED_LOG_MAX] __cacheline_aligned;
// static struct enterexit_log gExcpIrqEnterExitLog[2][SCHED_LOG_MAX] __cacheline_aligned;
// static struct timer_log gExcpTimerLog[2][SCHED_LOG_MAX] __cacheline_aligned;
#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
static struct dcvs_debug gExcpDcvsLog[DCVS_LOG_MAX] __cacheline_aligned;
#endif
#ifdef CONFIG_SEC_DEBUG_POWERCOLLAPSE_LOG
// static struct powercollapse_log gExcpPowerCollapseLog[POWERCOLLAPSE_LOG_MAX] __cacheline_aligned;
#endif
#ifdef CONFIG_SEC_DEBUG_SDIO_LOG
static struct sdio_log gExcpSDIOLog[SDIO_LOG_MAX] __cacheline_aligned;
#endif
atomic_t gExcpTaskLogIdx[2] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
atomic_t gExcpIrqLogIdx[2] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
atomic_t gExcpIrqEnterExitLogIdx[2] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
atomic_t gExcpTimerLogIdx[2] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
atomic_t gExcpDcvsLogIdx = ATOMIC_INIT(-1);
#endif
#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
atomic_t gExcpFGLogIdx = ATOMIC_INIT(-1);
#endif
#ifdef CONFIG_SEC_DEBUG_MDP_LOG
atomic_t gExcpMDPLogIdx = ATOMIC_INIT(-1);
#endif
#ifdef CONFIG_SEC_DEBUG_POWERCOLLAPSE_LOG
atomic_t gExcpPowerCollapseLogIdx = ATOMIC_INIT(-1);
#endif
#ifdef CONFIG_SEC_DEBUG_SDIO_LOG
atomic_t gExcpSDIOLogIdx = ATOMIC_INIT(-1);
#endif

struct sec_debug_nocache{

	struct task_info gExcpTaskLog[2][SCHED_LOG_MAX] ;
	struct irq_log gExcpIrqLog[2][SCHED_LOG_MAX] ;
	struct enterexit_log gExcpIrqEnterExitLog[2][SCHED_LOG_MAX] ;
	struct timer_log gExcpTimerLog[2][SCHED_LOG_MAX] ;
#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
	struct dcvs_debug gExcpDcvsLog[DCVS_LOG_MAX] ;
#endif
#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
	struct fuelgauge_debug gExcpFGLog[FG_LOG_MAX] ;
#endif
#ifdef CONFIG_SEC_DEBUG_MDP_LOG
	struct mdp_log gExcpMDPLog[MDP_LOG_MAX] ;
#endif
#ifdef CONFIG_SEC_DEBUG_POWERCOLLAPSE_LOG
	struct powercollapse_log gExcpPowerCollapseLog[POWERCOLLAPSE_LOG_MAX] ;
#endif
#ifdef CONFIG_SEC_DEBUG_SDIO_LOG
	struct sdio_log gExcpSDIOLog[SDIO_LOG_MAX] ;
#endif
#ifdef CONFIG_SEC_DEBUG_REGRW_LOG
	struct regrw_log gExcpRegRWLog[2][REGRW_LOG_MAX] ;
#endif
#ifdef CONFIG_SEC_DEBUG_WORKQ_LOG
	struct workq_info gExcpWorkqLog[2][SCHED_LOG_MAX];
#endif
	atomic_t gExcpTaskLogIdx[2];
	atomic_t gExcpIrqLogIdx[2];
	atomic_t gExcpIrqEnterExitLogIdx[2];
	atomic_t gExcpTimerLogIdx[2];
#ifdef CONFIG_SEC_DEBUG_WORKQ_LOG
	atomic_t gExcpWorkqLogIdx[2];
#endif
#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
	atomic_t gExcpDcvsLogIdx;	
#endif
#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
	atomic_t gExcpFGLogIdx;
#endif
#ifdef CONFIG_SEC_DEBUG_MDP_LOG
	atomic_t gExcpMDPLogIdx;
#endif
#ifdef CONFIG_SEC_DEBUG_POWERCOLLAPSE_LOG
	atomic_t gExcpPowerCollapseLogIdx;
#endif
#ifdef CONFIG_SEC_DEBUG_SDIO_LOG
	atomic_t gExcpSDIOLogIdx;
#endif
#ifdef CONFIG_SEC_DEBUG_REGRW_LOG
	int gExcpRegRWLogIdx[2];
#endif
};
struct sec_debug_nocache* sec_debug_nocache_log = NULL;

extern void dump_all_task_info(void);
extern void dump_cpu_stat(void);

static int checksum_sched_log(void)
{
	int sum = 0, i;
	for (i = 0; i < sizeof(gExcpTaskLog); i++)
		sum += *((char *)gExcpTaskLog + i);

	return sum;
}
#else
static int checksum_sched_log(void)
{
	return 0;
}
#endif

/* klaatu - semaphore log */
#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
struct sem_debug sem_debug_free_head;
struct sem_debug sem_debug_done_head;
int sem_debug_free_head_cnt;
int sem_debug_done_head_cnt;
int sem_debug_init = 0;
spinlock_t sem_debug_lock;

/* rwsemaphore logging */
struct rwsem_debug rwsem_debug_free_head;
struct rwsem_debug rwsem_debug_done_head;
int rwsem_debug_free_head_cnt;
int rwsem_debug_done_head_cnt;
int rwsem_debug_init = 0;
spinlock_t rwsem_debug_lock;

#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

/* onlyjazz.ed26 : make the restart_reason global to enable it early in sec_debug_init and share with restart functions */
static void *restart_reason = NULL;
void *sec_dbg_buf_ptr = NULL;
unsigned int restart_reason_val = 0; // kmj_ef13

DEFINE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DEFINE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);
DEFINE_PER_CPU(enum sec_debug_upload_cause_t, sec_debug_upload_cause);

#ifdef CONFIG_SEC_MISC
/* for sec debug level */
static int debug_level;
bool kernel_sec_set_debug_level(int level)
{
	if (!(level == KERNEL_SEC_DEBUG_LEVEL_LOW 
			|| level == KERNEL_SEC_DEBUG_LEVEL_MID 
			|| level == KERNEL_SEC_DEBUG_LEVEL_HIGH)) {
		printk(KERN_NOTICE "(kernel_sec_set_debug_level) The debug value is \
				invalid(0x%x)!! Set default level(LOW)\n", level);
		debug_level = KERNEL_SEC_DEBUG_LEVEL_LOW;
		return 0;
	}
		
	debug_level = level;

	switch(level){
	case KERNEL_SEC_DEBUG_LEVEL_LOW:
		enable = 0;
		enable_user = 0;
		break;
	case KERNEL_SEC_DEBUG_LEVEL_MID:
		enable = 1;
		enable_user = 0;
		break;
	case KERNEL_SEC_DEBUG_LEVEL_HIGH:
		enable = 1;
		enable_user = 1;
		break;
	default:
		enable = 1;
		enable_user = 1;
	}
	/* write to param */
	sec_set_param(param_index_debuglevel, &debug_level);

	printk(KERN_NOTICE "(kernel_sec_set_debug_level) The debug value is 0x%x !!\n", level);
	//Reverting back below changes as MDM dumps are not available in first restart.
#if 0
 // Restart the device when debug level is changed.
	printk("Sec_debug:kernel_sec_set_debug_level()  Rebooting device..\n");
	lock_kernel();
	kernel_restart(NULL);
	unlock_kernel();
#endif

	return 1;
}
EXPORT_SYMBOL(kernel_sec_set_debug_level);

int kernel_sec_get_debug_level(void)
{
	sec_get_param(param_index_debuglevel, &debug_level);

	if(!( debug_level == KERNEL_SEC_DEBUG_LEVEL_LOW 
			|| debug_level == KERNEL_SEC_DEBUG_LEVEL_MID 
			|| debug_level == KERNEL_SEC_DEBUG_LEVEL_HIGH))
	{
		/*In case of invalid debug level, default (debug level low)*/
		printk(KERN_NOTICE "(%s) The debug value is \
				invalid(0x%x)!! Set default level(LOW)\n", __func__, debug_level);	
		debug_level = KERNEL_SEC_DEBUG_LEVEL_LOW;
		sec_set_param(param_index_debuglevel, &debug_level);
	}
	return debug_level;
}
EXPORT_SYMBOL(kernel_sec_get_debug_level);
#endif

/* core reg dump function*/
static void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	/* we will be in SVC mode when we enter this function. Collect
	   SVC registers along with cmn registers. */
	asm("str r0, [%0,#0]\n\t"	/* R0 is pushed first to core_reg */
	    "mov r0, %0\n\t"		/* R0 will be alias for core_reg */
	    "str r1, [r0,#4]\n\t"	/* R1 */
	    "str r2, [r0,#8]\n\t"	/* R2 */
	    "str r3, [r0,#12]\n\t"	/* R3 */
	    "str r4, [r0,#16]\n\t"	/* R4 */
	    "str r5, [r0,#20]\n\t"	/* R5 */
	    "str r6, [r0,#24]\n\t"	/* R6 */
	    "str r7, [r0,#28]\n\t"	/* R7 */
	    "str r8, [r0,#32]\n\t"	/* R8 */
	    "str r9, [r0,#36]\n\t"	/* R9 */
	    "str r10, [r0,#40]\n\t"	/* R10 */
	    "str r11, [r0,#44]\n\t"	/* R11 */
	    "str r12, [r0,#48]\n\t"	/* R12 */
	    /* SVC */
	    "str r13, [r0,#52]\n\t"	/* R13_SVC */
	    "str r14, [r0,#56]\n\t"	/* R14_SVC */
	    "mrs r1, spsr\n\t"		/* SPSR_SVC */
	    "str r1, [r0,#60]\n\t"
	    /* PC and CPSR */
	    "sub r1, r15, #0x4\n\t"	/* PC */
	    "str r1, [r0,#64]\n\t"
	    "mrs r1, cpsr\n\t"		/* CPSR */
	    "str r1, [r0,#68]\n\t"
	    /* SYS/USR */
	    "mrs r1, cpsr\n\t"		/* switch to SYS mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x1f\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#72]\n\t"	/* R13_USR */
	    "str r14, [r0,#76]\n\t"	/* R14_USR */
	    /* FIQ */
	    "mrs r1, cpsr\n\t"		/* switch to FIQ mode */
	    "and r1,r1,#0xFFFFFFE0\n\t"
	    "orr r1,r1,#0x11\n\t"
	    "msr cpsr,r1\n\t"
	    "str r8, [r0,#80]\n\t"	/* R8_FIQ */
	    "str r9, [r0,#84]\n\t"	/* R9_FIQ */
	    "str r10, [r0,#88]\n\t"	/* R10_FIQ */
	    "str r11, [r0,#92]\n\t"	/* R11_FIQ */
	    "str r12, [r0,#96]\n\t"	/* R12_FIQ */
	    "str r13, [r0,#100]\n\t"	/* R13_FIQ */
	    "str r14, [r0,#104]\n\t"	/* R14_FIQ */
	    "mrs r1, spsr\n\t"		/* SPSR_FIQ */
	    "str r1, [r0,#108]\n\t"
	    /* IRQ */
	    "mrs r1, cpsr\n\t"		/* switch to IRQ mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x12\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#112]\n\t"	/* R13_IRQ */
	    "str r14, [r0,#116]\n\t"	/* R14_IRQ */
	    "mrs r1, spsr\n\t"		/* SPSR_IRQ */
	    "str r1, [r0,#120]\n\t"
	    /* MON */
	    "mrs r1, cpsr\n\t"		/* switch to monitor mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x16\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#124]\n\t"	/* R13_MON */
	    "str r14, [r0,#128]\n\t"	/* R14_MON */
	    "mrs r1, spsr\n\t"		/* SPSR_MON */
	    "str r1, [r0,#132]\n\t"
	    /* ABT */
	    "mrs r1, cpsr\n\t"		/* switch to Abort mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x17\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#136]\n\t"	/* R13_ABT */
	    "str r14, [r0,#140]\n\t"	/* R14_ABT */
	    "mrs r1, spsr\n\t"		/* SPSR_ABT */
	    "str r1, [r0,#144]\n\t"
	    /* UND */
	    "mrs r1, cpsr\n\t"		/* switch to undef mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x1B\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#148]\n\t"	/* R13_UND */
	    "str r14, [r0,#152]\n\t"	/* R14_UND */
	    "mrs r1, spsr\n\t"		/* SPSR_UND */
	    "str r1, [r0,#156]\n\t"
	    /* restore to SVC mode */
	    "mrs r1, cpsr\n\t"		/* switch to SVC mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x13\n\t"
	    "msr cpsr,r1\n\t" :		/* output */
	    : "r"(core_reg)			/* input */
	    : "%r0", "%r1"		/* clobbered registers */
	);

	return;
}

static void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
	    "str r1, [%0]\n\t"
	    "mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
	    "str r1, [%0,#4]\n\t"
	    "mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
	    "str r1, [%0,#8]\n\t"
	    "mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
	    "str r1, [%0,#12]\n\t"
	    "mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
	    "str r1, [%0,#16]\n\t"
	    "mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
	    "str r1, [%0,#20]\n\t"
	    "mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
	    "str r1, [%0,#24]\n\t"
	    "mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
	    "str r1, [%0,#28]\n\t"
	    "mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
	    "str r1, [%0,#32]\n\t"
	    /* Don't populate DAFSR and RAFSR */
	    "mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
	    "str r1, [%0,#44]\n\t"
	    "mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
	    "str r1, [%0,#48]\n\t"
	    "mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
	    "str r1, [%0,#52]\n\t"
	    "mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
	    "str r1, [%0,#56]\n\t"
	    "mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
	    "str r1, [%0,#60]\n\t"
	    "mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
	    "str r1, [%0,#64]\n\t"
	    "mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
	    "str r1, [%0,#68]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%r1", "memory"			/* clobbered register */
	);
}

static void sec_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg, smp_processor_id()));
	sec_debug_save_core_reg(&per_cpu
				(sec_debug_core_reg, smp_processor_id()));

	pr_emerg("(%s) context saved(CPU:%d)\n", __func__, smp_processor_id());
	local_irq_restore(flags);
}

unsigned int sec_get_lpm_mode(void)
{
	unsigned int ret = 0;

	pr_info("(%s) %x\n", __func__, reset_reason);
	if(reset_reason == RESET_REASON_LPM)
		ret = 1;
	pr_emerg("(%s) %x\n", __func__, ret);
	return ret;
}

static void sec_debug_set_upload_magic(unsigned magic)
{
	pr_emerg("(%s) %x\n", __func__, magic);

	writel(magic, restart_reason);

	flush_cache_all();
	outer_flush_all();
}

static int sec_debug_normal_reboot_handler(struct notifier_block *nb,
					    unsigned long l, void *p)
{
	enable = 0; /* can't go to 'Ramdump Mode' */
	sec_debug_set_upload_magic(0x0);

	return 0;
}

static void sec_debug_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	per_cpu(sec_debug_upload_cause, smp_processor_id()) = type;

#if 0	/* onlyjazz : enable it later */
	/* to check VDD_ALIVE / XnRESET issue */
	__raw_writel(type, S5P_INFORM3);
	__raw_writel(type, S5P_INFORM4);
	__raw_writel(type, S5P_INFORM6);
#else
	*(unsigned int *)0xc0000004 = type;
#endif	

	pr_emerg("(%s) %x\n", __func__, type);
}

#ifdef CONFIG_BATTERY_SEC
extern void disable_charging_before_reset(void);
#endif
void sec_debug_hw_reset(void)
{
	pr_emerg("(%s) %s\n", __func__, gkernel_sec_build_info);
	pr_emerg("(%s) rebooting...\n", __func__);
#ifdef CONFIG_BATTERY_SEC
	disable_charging_before_reset();
#endif

	flush_cache_all();

	outer_flush_all();

	arch_reset(0, "sec_debug_hw_reset");

	while (1) ;
}

extern void charm_assert_panic(void);

static int sec_debug_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	sec_debug_set_upload_magic(0x776655ee);

	if (!enable) {
		/* reset is moved to panic() function */
//		sec_debug_hw_reset();
		if((DEV_TYPE_JIG == sec_switch_get_attached_device()) &
				(!strcmp(buf, "Commercial Dump"))) {

			sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
			pr_emerg("(%s) %s\n", __func__, gkernel_sec_build_info);
			pr_emerg("(%s) rebooting...\n", __func__);

			flush_cache_all();
			outer_flush_all();
			arch_reset(0, "sec_debug_comm_dump");

			while (1) ;
		}
		return -1;
	}
	else

	if (!strcmp(buf, "User Fault"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (!strcmp(buf, "Crash Key"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strcmp(buf, "external_modem"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_MDM_ERROR_FATAL);
	else if (!strcmp(buf, "modem"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strcmp(buf, "lpass"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_LPASS_ERROR_FATAL);
	else
		sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	pr_err("(%s) checksum_sched_log: %x\n", __func__, checksum_sched_log());

#if 0
	handle_sysrq('t', NULL);
#endif

	sec_debug_dump_stack();
	charm_assert_panic();
	sec_debug_hw_reset();

	return 0;
}

/*
 * Called from dump_stack()
 * This function call does not necessarily mean that a fatal error
 * had occurred. It may be just a warning.
 */
int sec_debug_dump_stack(void)
{
	if (!enable)
		return -1;

	sec_debug_save_context();

	/* flush L1 from each core.
	   L2 will be flushed later before reset. */
	flush_cache_all();

	return 0;
}

void sec_debug_check_crash_key(unsigned int code, int value)
{
	static enum { NONE, STEP1, STEP2, STEP3, STEP4, STEP5, STEP6, STEP7, STEP8, STEP9, STEP10} state = NONE;

	if (!enable) {
        if(DEV_TYPE_JIG == sec_switch_get_attached_device()) {
    		switch (state) {
    		case NONE:
    			state = (code == LOCKUP_FIRST_KEY && value) ? STEP1 : NONE;
    			break;
     		case STEP1:
    			state = (code == LOCKUP_SECOND_KEY && value) ? STEP2 : NONE;
    			break;
     		case STEP2:
    			state = (code == LOCKUP_FIRST_KEY && !value) ? STEP3 : NONE;
    			break;
     		case STEP3:
    			state = (code == LOCKUP_FIRST_KEY && value) ? STEP4 : NONE;
    			break;
     		case STEP4:
    			state = (code == LOCKUP_FIRST_KEY && !value) ? STEP5 : NONE;
    			break;
     		case STEP5:
    			state = (code == LOCKUP_FIRST_KEY && value) ? STEP6 : NONE;
    			break;
     		case STEP6:
    			state = (code == LOCKUP_SECOND_KEY && !value) ? STEP7 : NONE;
    			break;
     		case STEP7:
    			state = (code == LOCKUP_SECOND_KEY && value) ? STEP8 : NONE;
    			break;
     		case STEP8:
    			state = (code == LOCKUP_SECOND_KEY && !value) ? STEP9 : NONE;
    			break;
     		case STEP9:
    			state = (code == LOCKUP_SECOND_KEY && value) ? STEP10 : NONE;
                break;
    		case STEP10:
    			if (code == LOCKUP_THIRD_KEY && value) {
    				dump_all_task_info();
    				dump_cpu_stat();
    				panic("Commercial Dump");
    			}
    			else
    			    state = NONE;
    			break;
    		default:
    			break;
    		}
        }
		return;
	}

	//pr_info("%s: %d %d\n", __func__, code, value);
#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120L) \
 || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#if defined (CONFIG_KOR_MODEL_SHV_E120L)
	if(get_hw_rev() >= 0x01)//SHV_E120L
#elif defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K)
	if(get_hw_rev() >= 0x06)//SHV_E120S
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if(get_hw_rev() >= 0x02)//SHV_E160S
#else
	if(get_hw_rev() > 0x04)// SHV_E110S
#endif
	{
		switch (state) {
		case NONE:
			if (code == LOCKUP_EXT_KEY && value)
				state = STEP1;
			else
			        state = NONE;
			break;
		case STEP1:
			if (code == LOCKUP_FIRST_KEY && value) {
				dump_all_task_info();
				dump_cpu_stat();
				panic("Crash Key");
			}
			else
			        state = NONE;
			break;
		default:
			break;
		}
	}
	else
#endif
	{
		switch (state) {
		case NONE:
			if (code == LOCKUP_FIRST_KEY && value)
				state = STEP1;
			else
			        state = NONE;
			break;
		case STEP1:
			if (code == LOCKUP_SECOND_KEY&& value)
				state = STEP2;
			else
			        state = NONE;
			break;
		case STEP2:
			if (code == LOCKUP_THIRD_KEY&& value) {
				dump_all_task_info();
				dump_cpu_stat();
				panic("Crash Key");
			}
			else
			        state = NONE;
			break;
        default:
            break;
		}
	}
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = sec_debug_normal_reboot_handler
};

static struct notifier_block nb_panic_block = {
	.notifier_call = sec_debug_panic_handler,
	.priority = -20,  /* Register this at the end of the list 
                         to run other handlers in advance */
};

static void sec_debug_set_build_info(void)
{
	char *p = gkernel_sec_build_info;
	sprintf(p, "Kernel Build Info : ");
	strcat(p, " Date:");
	strncat(p, gkernel_sec_build_info_date_time[0], 12);
	strcat(p, " Time:");
	strncat(p, gkernel_sec_build_info_date_time[1], 9);
}

__init int sec_debug_init(void)
{
	pr_emerg("(%s)\n", __func__);

	restart_reason = MSM_IMEM_BASE + RESTART_REASON_ADDR;
	/* check restart_reason here */
	pr_emerg("(%s) reset_reason : 0x%x\n", __func__, reset_reason);

	register_reboot_notifier(&nb_reboot_block);
	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	if (!enable)
		return -1;

	sec_dbg_buf_ptr = ioremap_nocache(sec_dbg_buf_base, sec_dbg_buf_size);
	/* check sec_dbg_buf_ptr here */
	/* check magic code here */

	pr_emerg("(%s) sec_dbg_buf_ptr = 0x%x\n", __func__, (unsigned int)sec_dbg_buf_ptr);

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG

	sec_debug_nocache_log = (struct sec_debug_nocache*)sec_dbg_buf_ptr;

	if (sec_debug_nocache_log) {
		/* init sec_debug_nocache_log here */
		atomic_set(&(sec_debug_nocache_log->gExcpTaskLogIdx[0]),-1);
		atomic_set(&(sec_debug_nocache_log->gExcpTaskLogIdx[1]),-1);
		atomic_set(&(sec_debug_nocache_log->gExcpIrqLogIdx[0]),-1);
		atomic_set(&(sec_debug_nocache_log->gExcpIrqLogIdx[1]),-1);
		atomic_set(&(sec_debug_nocache_log->gExcpIrqEnterExitLogIdx[0]),-1);
		atomic_set(&(sec_debug_nocache_log->gExcpIrqEnterExitLogIdx[1]),-1);
		atomic_set(&(sec_debug_nocache_log->gExcpTimerLogIdx[0]),-1);
		atomic_set(&(sec_debug_nocache_log->gExcpTimerLogIdx[1]),-1);
#ifdef CONFIG_SEC_DEBUG_WORKQ_LOG
		atomic_set(&(sec_debug_nocache_log->gExcpWorkqLogIdx[0]),-1);
		atomic_set(&(sec_debug_nocache_log->gExcpWorkqLogIdx[1]),-1);
#endif
#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
		atomic_set(&(sec_debug_nocache_log->gExcpDcvsLogIdx),-1);
#endif
#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
		atomic_set(&(sec_debug_nocache_log->gExcpFGLogIdx),-1);
#endif
#ifdef CONFIG_SEC_DEBUG_MDP_LOG
		atomic_set(&(sec_debug_nocache_log->gExcpMDPLogIdx),-1);
#endif
#ifdef CONFIG_SEC_DEBUG_POWERCOLLAPSE_LOG
		atomic_set(&(sec_debug_nocache_log->gExcpPowerCollapseLogIdx),-1);
#endif
#ifdef CONFIG_SEC_DEBUG_SDIO_LOG
		atomic_set(&(sec_debug_nocache_log->gExcpSDIOLogIdx),-1);
#endif
#ifdef CONFIG_SEC_DEBUG_REGRW_LOG
		sec_debug_nocache_log->gExcpRegRWLogIdx[0] = -1;
		sec_debug_nocache_log->gExcpRegRWLogIdx[1] = -1;
#endif
#endif
	}

	debug_semaphore_init();
	sec_debug_set_build_info();
	sec_debug_set_upload_magic(0x776655ee);
	sec_debug_set_upload_cause(UPLOAD_CAUSE_INIT);

	return 0;
}

int sec_debug_is_enabled(void)
{
	return enable;
}

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG

void sec_debug_task_sched_log(int cpu, struct task_struct *task)
{
	unsigned i;

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpTaskLogIdx[cpu])) & (SCHED_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpTaskLog[cpu][i].time = cpu_clock(cpu);
		strcpy(sec_debug_nocache_log->gExcpTaskLog[cpu][i].comm, task->comm);
		sec_debug_nocache_log->gExcpTaskLog[cpu][i].pid = task->pid;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpTaskLogIdx[cpu]) & (SCHED_LOG_MAX - 1);
		gExcpTaskLog[cpu][i].time = cpu_clock(cpu);
		strcpy(gExcpTaskLog[cpu][i].comm, task->comm);
		gExcpTaskLog[cpu][i].pid = task->pid;
	}
*/
}

void sec_debug_timer_log(unsigned int type, int int_lock, void *fn)
{
	int cpu = smp_processor_id();
	unsigned i;

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpTimerLogIdx[cpu])) & (SCHED_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpTimerLog[cpu][i].time = cpu_clock(cpu);
		sec_debug_nocache_log->gExcpTimerLog[cpu][i].type = type;
		sec_debug_nocache_log->gExcpTimerLog[cpu][i].int_lock = int_lock;
		sec_debug_nocache_log->gExcpTimerLog[cpu][i].fn = (void *)fn;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpTimerLogIdx[cpu]) & (SCHED_LOG_MAX - 1);
		gExcpTimerLog[cpu][i].time = cpu_clock(cpu);
		gExcpTimerLog[cpu][i].type = type;
		gExcpTimerLog[cpu][i].int_lock = int_lock;
		gExcpTimerLog[cpu][i].fn = (void *)fn;
	}
*/
}

void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en)
{
	int cpu = smp_processor_id();
	unsigned i;

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpIrqLogIdx[cpu])) & (SCHED_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpIrqLog[cpu][i].time = cpu_clock(cpu);
		sec_debug_nocache_log->gExcpIrqLog[cpu][i].irq = irq;
		sec_debug_nocache_log->gExcpIrqLog[cpu][i].fn = (void *)fn;
		sec_debug_nocache_log->gExcpIrqLog[cpu][i].en = en;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpIrqLogIdx[cpu]) & (SCHED_LOG_MAX - 1);
		gExcpIrqLog[cpu][i].time = cpu_clock(cpu);
		gExcpIrqLog[cpu][i].irq = irq;
		gExcpIrqLog[cpu][i].fn = (void *)fn;
		gExcpIrqLog[cpu][i].en = en;
	}
*/
}

void sec_debug_irq_sched_log_end(void)
{
	int cpu = smp_processor_id();
	unsigned i;

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpIrqLogIdx[cpu])) & (SCHED_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpIrqLog[cpu][i].elapsed_time = cpu_clock(cpu) - sec_debug_nocache_log->gExcpIrqLog[cpu][i].time;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_read(&gExcpIrqLogIdx[cpu]) & (SCHED_LOG_MAX - 1);
		gExcpIrqLog[cpu][i].elapsed_time = cpu_clock(cpu) -gExcpIrqLog[cpu][i].time;
	}
*/
}

#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
void sec_debug_irq_enterexit_log(unsigned int irq, unsigned long long start_time)
{
	int cpu = smp_processor_id();
	unsigned i;

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpIrqEnterExitLogIdx[cpu])) & (SCHED_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpIrqEnterExitLog[cpu][i].time = start_time;
		sec_debug_nocache_log->gExcpIrqEnterExitLog[cpu][i].end_time = cpu_clock(cpu);
		sec_debug_nocache_log->gExcpIrqEnterExitLog[cpu][i].irq = irq;
		sec_debug_nocache_log->gExcpIrqEnterExitLog[cpu][i].elapsed_time = sec_debug_nocache_log->gExcpIrqEnterExitLog[cpu][i].end_time -start_time;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpIrqEnterExitLogIdx[cpu]) & (SCHED_LOG_MAX - 1);
		gExcpIrqEnterExitLog[cpu][i].time = start_time;
		gExcpIrqEnterExitLog[cpu][i].end_time = cpu_clock(cpu);
		gExcpIrqEnterExitLog[cpu][i].irq = irq;
		gExcpIrqEnterExitLog[cpu][i].elapsed_time = gExcpIrqEnterExitLog[cpu][i].end_time -start_time;
	}
*/
}
#endif

#ifdef CONFIG_SEC_DEBUG_WORKQ_LOG
void debug_workq_log(int cpu, void *fn, struct task_struct *task) {
	unsigned i;

	if (sec_debug_nocache_log)
        {
                i = atomic_inc_return(&(sec_debug_nocache_log->gExcpWorkqLogIdx[cpu])) & (SCHED_LOG_MAX - 1);

		sec_debug_nocache_log->gExcpWorkqLog[cpu][i].fn = fn;
		strcpy(sec_debug_nocache_log->gExcpWorkqLog[cpu][i].comm, task->comm);
		sec_debug_nocache_log->gExcpWorkqLog[cpu][i].pid = task->pid;
	}
}
#endif
#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */

/* klaatu - semaphore log */
#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
void debug_semaphore_init(void)
{
	int i = 0;
	struct sem_debug *sem_debug =  NULL;

	spin_lock_init(&sem_debug_lock);
	sem_debug_free_head_cnt = 0;
	sem_debug_done_head_cnt = 0;

	/* initialize list head of sem_debug */
	INIT_LIST_HEAD(&sem_debug_free_head.list);
	INIT_LIST_HEAD(&sem_debug_done_head.list);

	for (i = 0; i < SEMAPHORE_LOG_MAX ; i++) {
		/* malloc semaphore */
		sem_debug  = kmalloc(sizeof(struct sem_debug), GFP_KERNEL);
		/* add list */
		list_add(&sem_debug->list, &sem_debug_free_head.list);
		sem_debug_free_head_cnt++;
	}

	sem_debug_init = 1;
}

void debug_semaphore_down_log(struct semaphore *sem)
{
	struct list_head *tmp;
	struct sem_debug *sem_dbg;
	unsigned long flags;

	if (!sem_debug_init)
		return;

	spin_lock_irqsave(&sem_debug_lock, flags);
	list_for_each(tmp, &sem_debug_free_head.list) {
		sem_dbg = list_entry(tmp, struct sem_debug, list);
		sem_dbg->task = current;
		sem_dbg->sem = sem;
		/* strcpy(sem_dbg->comm,current->group_leader->comm); */
		sem_dbg->pid = current->pid;
		sem_dbg->cpu = smp_processor_id();
		list_del(&sem_dbg->list);
		list_add(&sem_dbg->list, &sem_debug_done_head.list);
		sem_debug_free_head_cnt--;
		sem_debug_done_head_cnt++;
		break;
	}
	spin_unlock_irqrestore(&sem_debug_lock, flags);
}

void debug_semaphore_up_log(struct semaphore *sem)
{
	struct list_head *tmp;
	struct sem_debug *sem_dbg;
	unsigned long flags;

	if (!sem_debug_init)
		return;

	spin_lock_irqsave(&sem_debug_lock, flags);
	list_for_each(tmp, &sem_debug_done_head.list) {
		sem_dbg = list_entry(tmp, struct sem_debug, list);
		if (sem_dbg->sem == sem && sem_dbg->pid == current->pid) {
			list_del(&sem_dbg->list);
			list_add(&sem_dbg->list, &sem_debug_free_head.list);
			sem_debug_free_head_cnt++;
			sem_debug_done_head_cnt--;
			break;
		}
	}
	spin_unlock_irqrestore(&sem_debug_lock, flags);
}

/* rwsemaphore logging */
void debug_rwsemaphore_init(void)
{
	int i = 0;
	struct rwsem_debug *rwsem_debug =  NULL;

	spin_lock_init(&rwsem_debug_lock);
	rwsem_debug_free_head_cnt = 0;
	rwsem_debug_done_head_cnt = 0;

	/* initialize list head of sem_debug */
	INIT_LIST_HEAD(&rwsem_debug_free_head.list);
	INIT_LIST_HEAD(&rwsem_debug_done_head.list);

	for (i = 0; i < RWSEMAPHORE_LOG_MAX ; i++) {
		/* malloc semaphore */
		rwsem_debug  = kmalloc(sizeof(struct rwsem_debug), GFP_KERNEL);
		/* add list */
		list_add(&rwsem_debug->list, &rwsem_debug_free_head.list);
		rwsem_debug_free_head_cnt++;
	}

	rwsem_debug_init = 1;
}

void debug_rwsemaphore_down_log(struct rw_semaphore *sem, int dir)
{
	struct list_head *tmp;
	struct rwsem_debug *sem_dbg;
	unsigned long flags;

	if (!rwsem_debug_init)
		return;

	spin_lock_irqsave(&rwsem_debug_lock, flags);
	list_for_each(tmp, &rwsem_debug_free_head.list) {
		sem_dbg = list_entry(tmp, struct rwsem_debug, list);
		sem_dbg->task = current;
		sem_dbg->sem = sem;
		/* strcpy(sem_dbg->comm,current->group_leader->comm); */
		sem_dbg->pid = current->pid;
		sem_dbg->cpu = smp_processor_id();
		sem_dbg->direction = dir;
		list_del(&sem_dbg->list);
		list_add(&sem_dbg->list, &rwsem_debug_done_head.list);
		rwsem_debug_free_head_cnt--;
		rwsem_debug_done_head_cnt++;
		break;
	}
	spin_unlock_irqrestore(&rwsem_debug_lock, flags);
}

void debug_rwsemaphore_up_log(struct rw_semaphore *sem)
{
	struct list_head *tmp;
	struct rwsem_debug *sem_dbg;
	unsigned long flags;

	if (!rwsem_debug_init)
		return;

	spin_lock_irqsave(&rwsem_debug_lock, flags);
	list_for_each(tmp, &rwsem_debug_done_head.list) {
		sem_dbg = list_entry(tmp, struct rwsem_debug, list);
		if (sem_dbg->sem == sem && sem_dbg->pid == current->pid) {
			list_del(&sem_dbg->list);
			list_add(&sem_dbg->list, &rwsem_debug_free_head.list);
			rwsem_debug_free_head_cnt++;
			rwsem_debug_done_head_cnt--;
			break;
		}
	}
	spin_unlock_irqrestore(&rwsem_debug_lock, flags);
}
#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

static int __init sec_dbg_setup(char *str)
{
	unsigned size = memparse(str, &str);
	// unsigned long flags;

	pr_emerg("(%s)\n", __func__);

	if (size && (size == roundup_pow_of_two(size)) && (*str == '@')) {
		unsigned long long base = 0;
		// unsigned *sec_log_mag;
		// unsigned start;

		base = simple_strtoul(++str, &str, 0);

		sec_dbg_buf_size = size;
		sec_dbg_buf_base = base;
	}

	pr_emerg("(%s) sec_dbg_buf_base = 0x%x\n", __func__, sec_dbg_buf_base);
	pr_emerg("(%s) sec_dbg_buf_size = 0x%x\n", __func__, sec_dbg_buf_size);
	return 1;
}

__setup("sec_dbg=", sec_dbg_setup);



#ifdef CONFIG_SEC_DEBUG_USER
void sec_user_fault_dump(void)
{
	if (enable == 1 && enable_user == 1)
		panic("User Fault");
}

static int sec_user_fault_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *offs)
{
	char buf[100];

	if (count > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	if (strncmp(buf, "dump_user_fault", 15) == 0)
		sec_user_fault_dump();

	return count;
}

static const struct file_operations sec_user_fault_proc_fops = {
	.write		= sec_user_fault_write,
};

static int __init sec_debug_user_fault_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("user_fault", (S_IWUSR | S_IWGRP), NULL,
			    &sec_user_fault_proc_fops);
	if (!entry)
		return -ENOMEM;
	return 0;
}

device_initcall(sec_debug_user_fault_init);
#endif

#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
void sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq, unsigned int new_freq)
{
    unsigned int i;

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpDcvsLogIdx)) & (DCVS_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpDcvsLog[i].cpu_no = cpu_no;
		sec_debug_nocache_log->gExcpDcvsLog[i].prev_freq = prev_freq;
		sec_debug_nocache_log->gExcpDcvsLog[i].new_freq = new_freq;
		sec_debug_nocache_log->gExcpDcvsLog[i].time = cpu_clock(cpu_no);
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpDcvsLogIdx) & (DCVS_LOG_MAX - 1);
		gExcpDcvsLog[i].cpu_no = cpu_no;
		gExcpDcvsLog[i].prev_freq = prev_freq;
		gExcpDcvsLog[i].new_freq = new_freq;
		gExcpDcvsLog[i].time = cpu_clock(cpu_no);
	}
*/
}
#endif

#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
void sec_debug_fuelgauge_log(unsigned int voltage, unsigned short soc, unsigned short charging_status)
{
    unsigned int i;
    int cpu = smp_processor_id();

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpFGLogIdx)) & (FG_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpFGLog[i].time = cpu_clock(cpu);
		sec_debug_nocache_log->gExcpFGLog[i].voltage = voltage;
		sec_debug_nocache_log->gExcpFGLog[i].soc = soc;
		sec_debug_nocache_log->gExcpFGLog[i].charging_status = charging_status;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpFGLogIdx) & (FG_LOG_MAX - 1);
		gExcpFGLog[i].time = cpu_clock(cpu);
		gExcpFGLog[i].voltage = voltage;
		gExcpFGLog[i].soc = soc;
		gExcpFGLog[i].charging_status = charging_status;
	}

*/
}
#endif

#ifdef CONFIG_SEC_DEBUG_MDP_LOG

static int sec_debug_mdp_log_enabled = 1;

void sec_debug_mdp_log(unsigned int value1, unsigned int value2)
{
    unsigned int i;
    int cpu = smp_processor_id();

	if (!sec_debug_mdp_log_enabled)
		return;

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpMDPLogIdx)) & (MDP_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpMDPLog[i].time = cpu_clock(cpu);
		sec_debug_nocache_log->gExcpMDPLog[i].value1 = value1;
		sec_debug_nocache_log->gExcpMDPLog[i].value2 = value2;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpMDPLogIdx) & (MDP_LOG_MAX - 1);
		gExcpMDPLog[i].time = cpu_clock(cpu);
		gExcpMDPLog[i].value1 = value1;
		gExcpMDPLog[i].value2 = value2;
	}
*/
}

void sec_debug_mdp_enable(int enable)
{
	sec_debug_mdp_log_enabled = enable;
}

#endif

#ifdef CONFIG_SEC_DEBUG_POWERCOLLAPSE_LOG

void sec_debug_powercollapse_log(unsigned int value1, unsigned int value2)
{
    unsigned int i;
    int cpu = smp_processor_id();

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpPowerCollapseLogIdx)) & (POWERCOLLAPSE_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpPowerCollapseLog[i].time = cpu_clock(cpu);
		sec_debug_nocache_log->gExcpPowerCollapseLog[i].value1 = value1;
		sec_debug_nocache_log->gExcpPowerCollapseLog[i].value2 = value2;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpPowerCollapseLogIdx) & (POWERCOLLAPSE_LOG_MAX - 1);
		gExcpPowerCollapseLog[i].time = cpu_clock(cpu);
		gExcpPowerCollapseLog[i].value1 = value1;
		gExcpPowerCollapseLog[i].value2 = value2;
	}
*/
}

#endif

#ifdef CONFIG_SEC_DEBUG_SDIO_LOG

static int sec_debug_sdio_log_enabled = 1;

void sec_debug_sdio_log(unsigned int value1, unsigned int value2)
{
    unsigned int i;
    int cpu = smp_processor_id();

	if (!sec_debug_sdio_log_enabled)
		return;

	if (sec_debug_nocache_log)
	{
		i = atomic_inc_return(&(sec_debug_nocache_log->gExcpSDIOLogIdx)) & (SDIO_LOG_MAX - 1);
		sec_debug_nocache_log->gExcpSDIOLog[i].time = cpu_clock(cpu);
		sec_debug_nocache_log->gExcpSDIOLog[i].value1 = value1;
		sec_debug_nocache_log->gExcpSDIOLog[i].value2 = value2;
	}
/*  not to log when DEBUG_LEVEL_LOW */
/*
	else
	{
		i = atomic_inc_return(&gExcpSDIOLogIdx) & (SDIO_LOG_MAX - 1);
		gExcpSDIOLog[i].time = cpu_clock(cpu);
		gExcpSDIOLog[i].value1 = value1;
		gExcpSDIOLog[i].value2 = value2;
	}
*/
}

void sec_debug_sdio_enable(int enable)
{
	sec_debug_sdio_log_enabled = enable;
}

#endif

#ifdef CONFIG_SEC_DEBUG_REGRW_LOG
volatile unsigned int sec_debug_reg_read(unsigned long addr, int size)
{
	unsigned long flags;
	volatile unsigned int data;
    int cpu = smp_processor_id();
	
	if(!sec_debug_nocache_log) {
		data = *(volatile unsigned int __force *)(addr);
		return data;
	}

	raw_local_irq_save(flags);
	sec_debug_nocache_log->gExcpRegRWLogIdx[cpu] =
		(sec_debug_nocache_log->gExcpRegRWLogIdx[cpu] + 1) & (REGRW_LOG_MAX -1);
	sec_debug_nocache_log->gExcpRegRWLog[cpu][sec_debug_nocache_log->gExcpRegRWLogIdx[cpu]].addr = addr;
	sec_debug_nocache_log->gExcpRegRWLog[cpu][sec_debug_nocache_log->gExcpRegRWLogIdx[cpu]].size_rw = (size<<16)|1;

	data = *(volatile unsigned int __force *)(addr);

	sec_debug_nocache_log->gExcpRegRWLog[cpu][sec_debug_nocache_log->gExcpRegRWLogIdx[cpu]].data = data;
	sec_debug_nocache_log->gExcpRegRWLog[cpu][sec_debug_nocache_log->gExcpRegRWLogIdx[cpu]].done = 1;
	raw_local_irq_restore(flags);

	return data;
}
EXPORT_SYMBOL(sec_debug_reg_read);

void sec_debug_reg_write(unsigned long addr, unsigned long data, int size)
{
	unsigned long flags;
    int cpu = smp_processor_id();

	if(!sec_debug_nocache_log) {
		if(size == sizeof(unsigned char))
			*(volatile unsigned char __force   *)(addr) = (data);
		else if(size == sizeof(unsigned short))
			*(volatile unsigned short __force   *)(addr) = (data);
		else
			*(volatile unsigned int __force   *)(addr) = (data);
		return;
	}

	raw_local_irq_save(flags);
	sec_debug_nocache_log->gExcpRegRWLogIdx[cpu] =
		(sec_debug_nocache_log->gExcpRegRWLogIdx[cpu] + 1) & (REGRW_LOG_MAX -1);
	sec_debug_nocache_log->gExcpRegRWLog[cpu][sec_debug_nocache_log->gExcpRegRWLogIdx[cpu]].addr = addr;
	sec_debug_nocache_log->gExcpRegRWLog[cpu][sec_debug_nocache_log->gExcpRegRWLogIdx[cpu]].data = data;
	sec_debug_nocache_log->gExcpRegRWLog[cpu][sec_debug_nocache_log->gExcpRegRWLogIdx[cpu]].size_rw = (size<<16)|2;

	if(size == sizeof(unsigned char))
		*(volatile unsigned char __force   *)(addr) = (data);
	else if(size == sizeof(unsigned short))
		*(volatile unsigned short __force   *)(addr) = (data);
	else
		*(volatile unsigned int __force   *)(addr) = (data);


	sec_debug_nocache_log->gExcpRegRWLog[cpu][sec_debug_nocache_log->gExcpRegRWLogIdx[cpu]].done = 1;
	raw_local_irq_restore(flags);

}
EXPORT_SYMBOL(sec_debug_reg_write);
#endif

unsigned int sec_debug_is_recovery_mode(void)
{
	if( (reset_reason == RESET_REASON_RECOVERY) || (reset_reason == RESET_REASON_FOTA) ) {
		pr_info("(%s) reset_reason = %x, ret = 1 \n", __func__, reset_reason);
		return (unsigned int) 1;
	} else {
		pr_info("(%s) reset_reason = %x, ret = 0 \n", __func__, reset_reason);
		return (unsigned int) 0;
	}
#if 0
        if( restart_reason_val == 0x77665502 )
                return (unsigned int) 1;
        else
                return (unsigned int) 0;
#endif
}

/* Reset Reason given from Bootloader */
static int set_reset_reason_proc_show(struct seq_file *m, void *v)
{
	/*
		RESET_REASON_NORMAL
		RESET_REASON_SMPL
		RESET_REASON_WSTR
		RESET_REASON_WATCHDOG
		RESET_REASON_PANIC
		RESET_REASON_LPM
		RESET_REASON_RECOVERY
		RESET_REASON_FOTA
	*/
	switch(reset_reason){
	case RESET_REASON_NORMAL:
		seq_printf(m, "PON_NORMAL\n");
		break;
	case RESET_REASON_SMPL:
		seq_printf(m, "PON_SMPL\n");
		break;	
	case RESET_REASON_WSTR:
	case RESET_REASON_WATCHDOG:
	case RESET_REASON_PANIC:
		seq_printf(m, "PON_PANIC\n");
		break;	
	case RESET_REASON_LPM:
	case RESET_REASON_RECOVERY:
	case RESET_REASON_FOTA:
		seq_printf(m, "PON_NORMAL\n");
		break;	
	default:
		seq_printf(m, "PON_NORMAL\n");
	}

	return 0;
}

static int sec_reset_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_reset_reason_proc_show, NULL);
}

static const struct file_operations sec_reset_reason_proc_fops = {
	.open		= sec_reset_reason_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", S_IRUGO, NULL,
			    &sec_reset_reason_proc_fops);
	if (!entry)
		return -ENOMEM;

	return 0;
}

device_initcall(sec_debug_reset_reason_init);
