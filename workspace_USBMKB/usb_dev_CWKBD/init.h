/********************************************************************
 ************ COPYRIGHT (c) 2019 by ke0ff, Taylor, TX   *************
 *
 *  File name: init.h
 *
 *  Module:    Control
 *
 *  Summary:   #defines and global declarations for Morse keyboard project
 *  			Also defines GPIO pins, timer constants, and other
 *  			GPIO system constants
 *
 *******************************************************************/

#include "typedef.h"
#include <stdint.h>

#ifndef INIT_H
#define INIT_H
#endif

//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------

// Process defines
#define	INIT_PROCESS	0xa5		// initialize process semaphore

#define SYSCLKL 10000L
#define PIOCLK	(16000000L)			// internal osc freq (in Hz)
#define EXTXTAL           			// un-comment if external xtal is used
									// define EXTXTAL #def to select ext crystal
									// do not define for int osc
#ifdef EXTXTAL
#define XTAL 1
#define XTAL_FREQ	SYSCTL_RCC_XTAL_20MHZ	// set value of external XTAL
#define SYSCLK	(50000000L)			// sysclk freq (bus clk)
//#define SYSCLK	PIOCLK			// sysclk freq (bus clk)
#else
#define XTAL 0
#define SYSCLK	PIOCLK				// sysclk freq (bus clk) same as PIOCLK
#endif

#define OSC_LF 4            		// osc clock selects
#define OSC_HF 0
#define OSC_EXT 1

#define SEC10MS    10           	// timer constants (ms)
#define SEC33MS    33
#define SEC50MS    50
#define SEC75MS    74
#define SEC100MS  100
#define SEC250MS  250
#define SEC500MS  500
#define SEC750MS  750
#define SEC1     1000
#define ONESEC   SEC1
#define SEC2     2000
#define SEC3     3000
#define SEC5     5000
#define SEC10   10000
#define SEC15   15000
#define SEC30   30000
#define SEC60   60000
#define SEC300 300000L
#define	ONEMIN	(SEC60)
#define	REG_WAIT_DLY 200			// 200ms wait limit for register action
#define RESP_SAMP SEC100MS			// sets resp rate

// timer definitions
#define TIMER0_PS 0					// prescale value for timer1
#define	TLC14_FREQ	(2500 * 100)	// timer 1 out freq.  For TLC14, F(t0) = 100Fc, for TLC04, F(t0) = 50Fc
									// Looking for a 2500Hz Fc
#define	TLC04_FREQ	(2500 * 50)	// timer 1 out freq.  For TLC14, F(t0) = 100Fc, for TLC04, F(t0) = 50Fc

#define TIMER1_PS 31				// prescale value for timer1
#define	TIMER1_FREQ	9600			// timer 1 intr freq
#define	KEY_SCAN_FREQ 1000			// keyscan (timer2A) frequency, Hz
#define	FLASH_RATE 500				// number of timer ticks in 1/2 of a flash cycle (flash is a 50% duty cycle)
#define	BLINK_RATE	1500			// number of timer ticks in a blink cycle
#define	BLINK_OFF	1450			// number of timer ticks for the "off" period in a blink cycle (blink is 150/1500, or 10% duty cycle)
#define TIMER3_ILR 0xffff			// timer 3 interval (24 bit)
#define TIMER2_PS 32
#define	TIMER3_FREQ	FSAMP			// timer3 sample freq for DDS pwm
#define	TIMER3_PS 0					// timer3 prescaler value - runs at the Full Monty SYSCLK rate
#define	TPULSE	(100L)				// in usec
#define TMIN	(((SYSCLK / 100) * TPULSE)/10000L)	// minimum pulse width

// LED defines
#define	LED01	0x02				// LED register bit mask IDs
#define	LED02	0x04
#define	LED03	0x08
#define	LED04	0x10
#define	LED05	0x20
#define	LED06	0x40
#define	FIRST_LED		LED01		// first valid LED#
#define	MAX_LED	7					// max# LEDs allowed
#define	VALID_LED_MASK	(LED06|LED05|LED04|LED03|LED02|LED01)
#define	UPDATE_LED_ALL	0xfe
#define	INIT_LEDS		0xff
#define	ALTKYP_LED	1				// LED "name" aliases
#define	WIN_LED		2
#define	ALT_LED		3
#define	SHFLK_LED	4
#define	CAPLK_LED	5
#define	CTRL_LED	6

// timerA/B interrupt masks
#define TIMER_MIS_BMASK	(TIMER_MIS_TBMMIS | TIMER_MIS_CBEMIS | TIMER_MIS_CBMMIS | TIMER_MIS_TBTOMIS)

#define TIMER_MIS_AMASK	(TIMER_MIS_TAMMIS | TIMER_MIS_CAEMIS | TIMER_MIS_CAMMIS | TIMER_MIS_TATOMIS)

// Port A defines
#define TXD0			0x01		// out		uart0
#define RXD0			0x02		// in		uart0
#define KCOL0			0x04		// J2-10	in
#define KCOL1			0x08		// J2-9		in
#define KCOL2			0x10		// J2-8		in
#define KCOL3			0x20		// J1-8		in
#define LED1			0x40		// J1-9		out		i2c							ALTKYP
#define STW_LOCK_N		0x80		// J1-10	in		i2c
#define PORTA_DIRV		(TXD0|LED1)
#define	PORTA_DENV		(TXD0|RXD0|KCOL0|KCOL1|KCOL2|KCOL3|LED1|STW_LOCK_N)
#define	PORTA_PURV		(KCOL0|KCOL1|KCOL2|KCOL3|STW_LOCK_N)

// Port B defines
#define	NDAH			0x01		// J1-3		in	PB0 = /DAH			GPIO (i)
#define	NDIT			0x02		// J1-4		in	PPB1 = /DIT			GPIO (i)
#define	TONEA			0x04		// J2-2		out	PB2 = tonea		TIMER3A (o)		DDS Tone, PWMDAC (txt->cw)
#define	TONEB			0x08		// J4-3		out	PB3 = toneb		TIMER3B (o)		DDS Tone, PWMDAC (cwkypd)
#define CWSPEED			0x10		// J1-7		anin		ANIN10
#define CWTONE			0x20		// J1-2		anin		ANIN11
#define FIL_CLK			0x40		// J2-7		out		T1CCP0
#define IAMBIC_BSEL		0x80		// J2-6		in		T1CCP1
#define PORTB_DIRV		(TONEB | TONEA | FIL_CLK)
#define	PORTB_DENV		(NDAH | NDIT | TONEB | TONEA | FIL_CLK | IAMBIC_BSEL)
#define	PORTB_PURV		(NDAH |NDIT|IAMBIC_BSEL)

// Port C defines
#define PADDLE_ORIENT	0x10		// J4-4		in		power-on paddle orientation
#define FACTORY_DEFAULT	0x20		// J4-5		in		reset to factory defaults
#define PORTC6			0x40		// J4-6		out
#define TLC14_SEL 		0x80		// J4-7		in		select TLC14 clock (250 KHz), else TLC04 (125 KHz)
#define PORTC_DIRV		(PORTC6)
#define	PORTC_DENV		(TLC14_SEL|PORTC6|FACTORY_DEFAULT|PADDLE_ORIENT)
#define	PORTC_PURV		(FACTORY_DEFAULT|TLC14_SEL|PADDLE_ORIENT)

// Port D defines
#define PORTD0			0x01		// J3-3		in (connected to PB6 on LP)
#define PORTD1			0x02		// J3-4		in (connected to PB7 on LP)
#define PADL_KEY		0x04		// J3-5		in			selects paddles or straight key
#define PWR_ON_LOCK		0x08		// J3-6		in			ground to disable power-on lock feature
#define PORTD4			0x10		// USB_DM
#define PORTD5			0x20		// USB_DP
#define	WEIGHT_ADJ_N	0x40		// J4-8		in			selects weight adjust (0) or tone adjust (1)
#define	KEYPAD_PGM_N	0x80		// J4-9		in			enables keypad prog (0)
#define PORTD_DIRV		(PORTD4 | PORTD5)
#define	PORTD_DENV		(0xff)
#define	PORTD_PURV		(PWR_ON_LOCK|PADL_KEY|WEIGHT_ADJ_N|KEYPAD_PGM_N)

// Port E defines
#define KROW0			0x01		// J2-3		out
#define KROW1			0x02		// J3-7		out
#define KROW2			0x04		// J3-8		out
#define	KROW3			0x08		// J3-9		out
#define KROW4			0x10		// J1-5		out
#define LED3			0x20		// J1-6		out		M1PWM3	{LED3}					ALT
#define PORTE_DIRV		(KROW4|KROW3|KROW2|KROW1|KROW0|LED3)
#define	PORTE_DENV		(KROW4|KROW3|KROW2|KROW1|KROW0|LED3)
#define	PORTE_ODRV		(KROW4|KROW3|KROW2|KROW1|KROW0)
#define	PORTE_PURV		(KROW4|KROW3|KROW2|KROW1|KROW0)
// keypad defines
#define	KEYP_ROW		5			// # rows in keymap
#define	KEYP_COL		4			// # columns in keymap

#define	KB_ROW_START	(KROW4)		// start for kb row vector
#define	KB_ADDR_START	(5)			// start for kb row addr
#define	KB_ROW_M		(KROW4|KROW3|KROW2|KROW1|KROW0)		// mask for kb row vector
#define	KB_COL_M		(KCOL3|KCOL2|KCOL1|KCOL0)	// mask for kb col 1of4
#define	KB_NOKEY		(KCOL3|KCOL2|KCOL1|KCOL0)	// no keypressed semaphore
#define	KP_DEBOUNCE_DN	3			// 24 ms of keypad debounce
#define	KP_DEBOUNCE_UP	25			// 25 ms of keypad debounce
#define	MAX_ALT_KP		2			// number of alt keypads
#define	RETURN_KPALT_VAL 0xff		// force return of current keypad setting
#define	MAIN_KP_SEL		0			// first keypad number

// Port F defines
#define LED2			0x01		// J2-4		out		M1PWM4	led2					WIN		user sw2
#define LED4			0x02		// J3-10	out		M1PWM5	{123GXL LP red LED}		SHFLK
#define LED5			0x04		// J4-1		out		M1PWM6	{123GXL LP blue LED}	CAPLK
#define LED6			0x08		// J4-2		out		M1PWM7	{123GXL LP green LED}	CTRL
#define CTRLZ_EN_N		0x10		// J4-10	in		user sw1
#define PORTF_DIRV		(LED2|LED4|LED5|LED6)
#define	PORTF_DENV		(LED2|LED4|LED5|LED6|CTRLZ_EN_N)
#define	PORTF_PURV		(CTRLZ_EN_N)

#define	ROW_MAX			5
#define	COL_MAX			4

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Fn prototype declarations
//-----------------------------------------------------------------------------

U16 get_ipl(void);
void set_led(U8 lednum, U8 value);
void Timer2_ISR(void);
void wait(U16 waitms);
U8 wait_reg0(volatile uint32_t *regptr, uint32_t clrmask, U16 delay);
U8 wait_reg1(volatile uint32_t *regptr, uint32_t setmask, U16 delay);

// keypad.c declarations (keypad)
int keypad_init(U8 fi);
U8 got_key(void);
U8 not_key(U8 flag);
void kp_asc(void);
char get_key(void);
char get_keycode(void);
void store_keycode(char pc, U8 kcode);
void save_keymap(void);
U8 get_pace_flag(void);
U8 set_kpalt(U8 altval);
void store_userps(char pc);
char get_userps(void);

// keypad.c declarations (LED)
//void set_pwm(U8 pwmnum, U8 percent);
void set_led(U8 lednum, U8 ledon);
void flash_led(U8 lednum, U8 fon);
void blink_led(U8 lednum, U8 fon);

//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------
