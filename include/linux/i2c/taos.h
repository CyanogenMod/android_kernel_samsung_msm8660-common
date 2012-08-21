#ifndef __TAOS_H__
#define __TAOS_H__


/* i2c */
#define I2C_M_WR 0 /* for i2c */
#define I2c_M_RD 1 /* for i2c */

/* sensor gpio */
#define GPIO_SENSE_OUT	27


#define REGS_PROX 	0x0 // Read  Only
#define REGS_GAIN    	0x1 // Write Only
#define REGS_HYS	0x2 // Write Only
#define REGS_CYCLE	0x3 // Write Only
#define REGS_OPMOD	0x4 // Write Only
#define REGS_CON	0x6 // Write Only

/* sensor type */
#define TAOS_LIGHT           0
#define TAOS_PROXIMITY	1
#define TAOS_ALL		2

/* power control */
#define ON              1
#define OFF		0

/* IOCTL for proximity sensor */
#define SHARP_TAOSP_IOC_MAGIC   'C'                                 
#define SHARP_TAOSP_OPEN    _IO(SHARP_TAOSP_IOC_MAGIC,1)            
#define SHARP_TAOSP_CLOSE   _IO(SHARP_TAOSP_IOC_MAGIC,2)      

/* IOCTL for light sensor */
#define SHARP_TAOSL_IOC_MAGIC   'L'                                 
#define SHARP_TAOSL_OPEN    _IO(SHARP_TAOSL_IOC_MAGIC,1)            
#define SHARP_TAOSL_CLOSE   _IO(SHARP_TAOSL_IOC_MAGIC,2) 

#define	MAX_LUX				32768
/* for proximity adc avg */
#define PROX_READ_NUM 40
#define TAOS_PROX_MAX 1023
#define TAOS_PROX_MIN 0

/* input device for proximity sensor */
#define USE_INPUT_DEVICE 	1//1  /* 0 : No Use  ,  1: Use  */

#define USE_INTERRUPT		1
#define INT_CLEAR    1 /* 0 = by polling operation, 1 = by interrupt operation */

/* Register value  for TMD2771x */    // hm83.cho 100817
#define ATIME 0xff   // 2.7ms - minimum ALS intergration time
#define WTIME 0xff  // 2.7ms - minimum Wait time
#define PTIME  0xff  // 2.7ms - minimum Prox integration time
#define PPCOUNT  1
#define PIEN 0x20    // Enable Prox interrupt
#define WEN  0x8     // Enable Wait
#define PEN  0x4     // Enable Prox
#define AEN  0x2     // Enable ALS
#define PON 0x1     //Enable Power on
#define PDRIVE 0
#define PDIODE 0x20
#define PGAIN 0
#define AGAIN 0



/* initial value for sensor register */
/*static u8 taos_original_image[8] =
{
	0x00,  
	0x08,  
	0xC2,  
	0x04,
	0x00,
};
*/
/*
static u8 taos_original_image[8] =
{
	0x00,  	// REGS_PROX
	0x08,  	// REGS_GAIN
	0x40,  	// REGS_HYS
	0x04,  	// REGS_CYCLE
	0x03,   // REGS_OPMOD
};
*/

/* TDM2771x*/

/* 16 level for premium model*/
typedef enum t_light_state
{
	LIGHT_DIM   = 0,
	LIGHT_LEVEL1   = 1,
	LIGHT_LEVEL2   = 2,
	LIGHT_LEVEL3   = 3,
	LIGHT_LEVEL4   = 4,
	LIGHT_LEVEL5   = 5,
	LIGHT_LEVEL6   = 6,
	LIGHT_LEVEL7   = 7,
	LIGHT_LEVEL8   = 8,
	LIGHT_LEVEL9   = 9,
	LIGHT_LEVEL10   = 10,
	LIGHT_LEVEL11   = 11,
	LIGHT_LEVEL12   = 12,
	LIGHT_LEVEL13   = 13,	
	LIGHT_LEVEL14   = 14,		
	LIGHT_LEVEL15   = 15,	
	LIGHT_LEVEL16   = 16,	
	LIGHT_INIT  = 17,	
}state_type;

typedef enum
{
	TAOS_ALS_CLOSED = 0,
	TAOS_ALS_OPENED = 1,
} TAOS_ALS_FOPS_STATUS;

typedef enum
{
	TAOS_PRX_CLOSED = 0,
	TAOS_PRX_OPENED = 1,
} TAOS_PRX_FOPS_STATUS;

typedef enum
{
	TAOS_PRX_DIST_UNKNOWN = 0,
	TAOS_PRX_DIST_IN = 1,
	TAOS_PRX_DIST_OUT = 2
} TAOS_PRX_DISTANCE_STATUS;

typedef enum
{
	TAOS_CHIP_UNKNOWN = 0,
	TAOS_CHIP_WORKING = 1,
	TAOS_CHIP_SLEEP = 2
} TAOS_CHIP_WORKING_STATUS;

/* driver data */
struct taos_data {
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct work_prox;  /* for proximity sensor */
    struct work_struct work_light; /* for light_sensor     */
    struct work_struct work_ptime; /* for proximity reset    */
	struct class *lightsensor_class;
	struct class *proximity_class;
	struct device *proximity_dev;
	struct device *switch_cmd_dev;
	int             irq;
	struct hrtimer timer;
	struct hrtimer ptimer;
	void	(*led_on) (void);
	void	(*led_off) (void);
	void	(*power_on) (void);
	void	(*power_off) (void);
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)
	void	(*light_on) (void);
	void	(*light_off) (void);
#endif	
	struct mutex power_lock;
	int light_count;
	int light_buffer;
	int delay;
	int avg[3];
//	struct timer_list light_init_timer;
//	struct timer_list prox_init_timer;  
};

/* platform data */
struct taos_platform_data {
	void	(*led_on) (void);
	void	(*led_off) (void);
	void	(*power_on) (void);
	void	(*power_off) (void);
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)
	void	(*light_on) (void);
	void	(*light_off) (void);
#endif			
};

struct taos_data *taos_global;
struct workqueue_struct *taos_wq;
struct workqueue_struct *taos_test_wq;
/* prototype */
extern short taos_get_proximity_value();
short IsTaosSensorOn();
static int proximity_open(struct inode *ip, struct file *fp);
static int proximity_release(struct inode *ip, struct file *fp);
static long proximity_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int taos_read_als_data(void);
int taos_get_lux(void);
static double StateToLux(state_type state);
void taos_on(struct taos_data *taos, int type);
void taos_off(struct taos_data *taos, int type);

#endif
