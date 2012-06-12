
#ifndef _SEC_KEYBOARD_H_
#define _SEC_KEYBOARD_H_

#define KEYBOARD_SIZE   128
#define US_KEYBOARD     0xeb
#define UK_KEYBOARD     0xec

#define KEYBOARD_MIN   0x4
#define KEYBOARD_MAX   0x7f

#define MAX_BUF     64

//#define ACC_EN_KBD

enum KEY_LAYOUT
{
	UNKOWN_KEYLAYOUT = 0,
	US_KEYLAYOUT,
	UK_KEYLAYOUT,
};

/*
struct kbd_callbacks {
	void (*get_data)(struct kbd_callbacks *cb, unsigned int val);
	int (*check_keyboard_dock)(struct kbd_callbacks *cb, int val);
};

struct dock_keyboard_platform_data {
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	void	(*register_cb)(struct kbd_callbacks *cb);
	void	(*init_keyboard_gpio) (void);
	int gpio_accessory_enable;
};
*/

  /* Each client has this additional data */
struct dock_keyboard_drvdata
{
	struct kbd_callbacks callbacks;
	struct input_dev *input_dev;
	struct mutex mutex;
	struct early_suspend	early_suspend;
	struct timer_list timer;
	struct timer_list key_timer;
	struct device *keyboard_dev;
	struct work_struct work_msg;
	struct work_struct work_timer;
	void	(*acc_power)(u8 token, bool active);
	bool led_on;
	bool dockconnected;
	bool pre_connected;
	bool pressed[KEYBOARD_SIZE];
	bool pre_uart_path;
	int buf_front;
	int buf_rear;
	int acc_int_gpio;
	unsigned int remap_key;
	unsigned int kl;
	unsigned int pre_kl;
	unsigned char key_buf[MAX_BUF+1];
	unsigned short keycode[KEYBOARD_SIZE];
	unsigned long connected_time;
	unsigned long disconnected_time;
};

static const unsigned short dock_keycodes[KEYBOARD_SIZE] =
{
	 // keycode              ,         decimal     hex
	KEY_RESERVED,       		//	0		0
	KEY_RESERVED,       		//	1		1
	KEY_RESERVED,       		//	2		2
	KEY_RESERVED,       		//	3		3
	KEY_A,              			//	4		4
	KEY_B,              			//	5		5
	KEY_C,              			//	6		6
	KEY_D,              			//	7		7
	KEY_E,              			//	8		8
	KEY_F,              			//	9		9
	KEY_G,              			//	10		A
	KEY_H,              			//	11		B
	KEY_I,              			//	12		C
	KEY_J,              			//	13		D
	KEY_K,              			//	14		E
	KEY_L,              			//	15		F
	KEY_M,              			//	16		10
	KEY_N,              			//	17		11
	KEY_O,              			//	18		12
	KEY_P,              			//	19		13
	KEY_Q,              			//	20		14
	KEY_R,              			//	21		15
	KEY_S,              			//	22		16
	KEY_T,              			//	23		17
	KEY_U,              			//	24		18
	KEY_V,              			//	25		19
	KEY_W,              			//	26		1A
	KEY_X,              			//	27		1B
	KEY_Y,              			//	28		1C
	KEY_Z,              			//	29		1D
	KEY_1,              			//	30		1E
	KEY_2,              			//	31		1F
	KEY_3,              			//	32		20
	KEY_4,              			//	33		21
	KEY_5,              			//	34		22
	KEY_6,              			//	35		23
	KEY_7,              			//	36		24
	KEY_8,              			//	37		25
	KEY_9,              			//	38		26
	KEY_0,              			//	39		27
	KEY_ENTER,          		//	40		28
	KEY_BACK,		            	//	41		29
	KEY_BACKSPACE,      		//	42		2A
	KEY_TAB,            			//	43		2B
	KEY_SPACE,          		//	44		2C
	KEY_MINUS,          		//	45		2D
	KEY_EQUAL,          		//	46		2E
	KEY_LEFTBRACE,      		//	47		2F
	KEY_RIGHTBRACE,     		//	48		30
	KEY_HOME,		      		//	49		31
	KEY_RESERVED,       		//	50		32
	KEY_SEMICOLON,      		//	51		33
	KEY_APOSTROPHE,     		//	52		34
	KEY_GRAVE,          		//	53		35
	KEY_COMMA,          		//	54		36
	KEY_DOT,            			//	55		37
	KEY_SLASH,          		//	56		38
	KEY_CAPSLOCK,       		//	57		39
	KEY_TIME,           			//	58		3A
	KEY_F3, 					//	59		3B	//recent program
	KEY_WWW,   				//	60		3C
	KEY_EMAIL,            		//	61		3D
	KEY_SCREENLOCK,         	//	62		3E
	KEY_BRIGHTNESSDOWN,	//	63		3F
	KEY_BRIGHTNESSUP,           //	64		40
	KEY_MUTE,         			//	65		41
	KEY_VOLUMEDOWN,            //	66		42
	KEY_VOLUMEUP,          		//	67		43
	KEY_PLAY,           			//	68		44
	KEY_REWIND,         		//	69		45
	KEY_F15,           			//	70		46	//Android Application screen
	KEY_RESERVED,       		//	71		47
	KEY_FASTFORWARD,    		//	72		48
	KEY_MENU,			     	//	73		49
	KEY_RESERVED,       		//	74		4A
	KEY_RESERVED,       		//	75		4B
	KEY_DELETE,	       		//	76		4C
	KEY_RESERVED,       		//	77		4D
	KEY_RESERVED,       		//	78		4E
	KEY_RIGHT,          		//	79		4F
	KEY_LEFT,           			//	80		50
	KEY_DOWN,  	 	        	//	81		51
	KEY_UP,       	 	        	//	82		52
	KEY_NUMLOCK,        		//	83		53
	KEY_KPSLASH,        		//	84		54
	KEY_APOSTROPHE,     		//	85		55
	KEY_KPMINUS,        		//	86		56
	KEY_KPPLUS,         		//	87		57
	KEY_KPENTER,        		//	88		58
	KEY_KP1,            			//	89		59
	KEY_KP2,            			//	90		5A
	KEY_KP3,            			//	91		5B
	KEY_KP4,            			//	92		5C
	KEY_KP5,            			//	93		5D
	KEY_KP6,            			//	94		5E
	KEY_KP7,            			//	95		5F
	KEY_KP8,            			//	96		60
	KEY_KP9,            			//	97		61
	KEY_KPDOT,          		//	98		62
	KEY_RESERVED,       		//	99		63
	KEY_BACKSLASH,      		//	100		64	//For the UK keyboard
	KEY_F22,           			//	101		65	//quick panel
	KEY_RESERVED,       		//	102		66
	KEY_RESERVED,       		//	103		67
	KEY_RESERVED,       		//	104		68
	KEY_RESERVED,       		//	105		69
	KEY_RESERVED,       		//	106		6A
	KEY_RESERVED,       		//	107		6B
	KEY_RESERVED,       		//	108		6C
	KEY_RESERVED,       		//	109		6D
	KEY_RESERVED,       		//	110		6E
	KEY_RESERVED,       		//	111		6F
	KEY_HANGEUL,        		//	112		70
	KEY_HANJA,          		//	113		71
	KEY_LEFTCTRL,            		//	114		72
	KEY_LEFTSHIFT,      		//	115		73
	KEY_F20,					//	116		74	//voicesearch
	KEY_SEARCH,       			//	117		75
	KEY_RIGHTALT,   			//	118		76
	KEY_RIGHTSHIFT,     		//	119		77
	KEY_F21,            			//	120		78	//Lang
	KEY_RESERVED,       		//	121		79		Right GUI (Windows Key)
	KEY_RESERVED,       		//	122		7A
	KEY_RESERVED,       		//	123		7B
	KEY_RESERVED,       		//	124		7C
	KEY_RESERVED,       		//	125		7D
	KEY_RESERVED,       		//	126		7E
	KEY_F17, 	           		//	127		7F	//SIP SHOW/HIDE
};

#endif  //_SEC_KEYBOARD_H_
