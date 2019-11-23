/********************************************************************
 ************ COPYRIGHT (c) 2019 by ke0ff, Taylor, TX   *************
 *
 *  File name: keypad.c
 *
 *  Module:    Control
 *
 *  Summary:
 *  This is the code file for the scanned Key-Pad.  Copied from the
 *  	KPU project, but heavily modified.
 *  	The keypad code uses a 4x5 array to hold key-down timers.
 *  	This allows multiple keypresses to be simultaneously de-bounced and
 *  	captured.  Modifiers must be pressed first, followed by the action
 *  	key (otherwise, the action key may be recognized before the modifiers).
 *  	This is not unlike a normal keyboard.
 *  Control code for the PWM LED outputs is also included in this file.
 *  	This code simply tracks the LED state and adjusts the PWM value from
 *  	min to max to go from off-to-on (or vis-a-vis').  Compile-time settings
 *  	for max brightness are the only allowance for brightness adjustment.
 *  	The PWM module is initialized in tiva_init.c and the on-off states
 *  	are simply changed by adjusting the compare register for the desired LED
 *  	(there is no interrupt code for this peripheral).
 *
 *******************************************************************/

/********************************************************************
 *  File scope declarations revision history:
 *
 *    04-06-19 jmh:  creation date
 *
 *******************************************************************/

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
// compile defines

#define MAIN_C
#include <stdint.h>
#include "inc/tm4c123gh6pm.h"
#include <stdio.h>
#include <string.h>
#include "init.h"
#include "typedef.h"
#include "version.h"
#include "tiva_init.h"
#include "morse_lut.h"
#include "eeprom.h"
#include <stdbool.h>
#include "utils/uartstdio.h"

//-----------------------------------------------------------------------------
// Definitions
//-----------------------------------------------------------------------------

//  see init.h for keypad.c #defines
#define	GETS_INIT	0xff	// gets_tab() static initializer signal

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Local variables in this file
//-----------------------------------------------------------------------------

//*****************************************************************************
//
// This is the factory-default "Key-to-pseudo-ASCII" keycode map.
//	It holds ASCII and pseudo-ASCII codes which correspond to the physical
//	key at a given row/col intercept.  This key map is simply "a possible" approach,
//	Any ASCII or pASCII code (see morse_lut.h) may be placed in this map matrix.
//	Two keypads are supported and are selected using the Morse pASCII command code
//	MRSE_KSWP.
//
static const char key_pascii_map_rom[2][5][4] = {
	{
		{ MRSE_CNTL, MRSE_BACKCSP, MRSE_SHIFT,  MRSE_SHLK   },	// row 0
		{    '/',       MRSE_UP,      '-',      MRSE_PGUP   },	// row 1
		{ MRSE_LEFT,     '=',      MRSE_RIGHT,  MRSE_KPSWP  },	// row 2
		{  MRSE_SKS ,   MRSE_DN,      '.',      MRSE_PGDN   },	// row 3
		{MRSE_CWLOCK, MRSE_REVRS, MRSE_WORDBS, MRSE_WORDDEL },	// row 4
	},
	{
		{ MRSE_CNTL, MRSE_BACKCSP, MRSE_SHIFT, MRSE_CAPLOCK },	// row 0
		{    '/',       MRSE_UP,      '-',      MRSE_PGUP   },	// row 1
    	{ MRSE_LEFT,     '=',      MRSE_RIGHT,  MRSE_KPSWP  },	// row 2
		{  MRSE_SKS ,   MRSE_ALT,    MRSE_DEL,   MRSE_WIN    },	// row 3
		{MRSE_CWLOCK, MRSE_REVRS, MRSE_WORDBS, MRSE_WORDDEL },	// row 4
	}
};

static char key_pascii_map[2][5][4];
//
//*****************************************************************************

U8		iplt2;							// timer2 ipl flag
S8		err_led_stat;					// err led status
U8		idx;
U16		ipl;							// initial power on state
volatile U8		waittimer;
volatile U8		pace_flag;				// pace timer event flag
volatile U16	flash_timer;			// LED flash event timer
volatile U16	blink_timer;			// LED blink event timer
volatile U8		flash_enable;			// flash enable - bits correspond to specific LED GPIOs.  1=flash the GPIO, 0 = normal control
volatile U8		blink_enable;			// blink enable - bits correspond to specific LED GPIOs.  1=blink the GPIO, 0 = normal control

#define KBD_ERR 0x01
#define KBD_BUFF_END 10
volatile S8		kbd_buff[KBD_BUFF_END];	// keypad data buffer
volatile U8		map_buff[KBD_BUFF_END];	// keypad keycode buffer
volatile U8		kbd_hptr;				// keypad buf head ptr
volatile U8		kbd_tptr;				// keypad buf tail ptr
volatile U8		kbd_stat;				// keypad buff status
volatile U8		kbdn_flag;				// key down or hold
volatile U8		kbup_flag;				// key released
volatile U8		key_press[5][4];		// key press timing/debounce matrix
volatile U8		key_alt;				// alt keypad select
volatile U32	sys_error_flags;		// system error flags
volatile U8		debug_i;
		 U8		led_on;							// led on reg (1/0)
volatile U8		led_enable;						// flash/blink enable reg (1/0)
		 U8		led_1_level;					// led level regs (0-100%)
		 U8		led_2_level;
		 U8		led_3_level;
		 U8		led_4_level;
		 U8		led_5_level;
		 U8		led_6_level;
		 U16	pwm1_reg;						// led pwm regs
		 U16	pwm2_reg;
		 U16	pwm3_reg;
		 U16	pwm4_reg;
		 U16	pwm5_reg;
		 U16	pwm6_reg;
		 U8		pwm_master;						// led master level

//-----------------------------------------------------------------------------
// Local Prototypes
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// set pwm values
//-----------------------------------------------------------------------------
//
// set_pwm() sets the specified PWM to the percent value
//	pwmnum specifies the LED number.  LED 0 is the master setting.  This only sets the master,
//		it does not update any of the LED pwm registers.
//	percent is the percent level, 0 - 100.  If percent > 100, the value is unchanged
//		and the PWM settings are calculated based on stored led and master values
//

/*void set_pwm(U8 pwmnum, U8 percent){
	U32	kk;				// temp U32

	if(percent <= 100){
		switch(pwmnum){						// store level % value
		case 0:								// set master value
			pwm_master = percent;
			break;

		case 3:
			led_3_level = percent;
			break;

		case 4:
			led_4_level = percent;
			break;

		case 5:
			led_5_level = percent;
			break;

		case 6:
			led_6_level = percent;
			break;
		}
	}
	switch(pwmnum){
	case 3:									// LED3
		kk = PWM_PERIOD - ((PWM_PERIOD - PWM_MIN) * (U32)led_3_level * (U32)pwm_master / 10000L) - 1L;
		pwm3_reg = (U16)kk;
		if(led_3_on) PWM1_3_CMPB_R = kk;
		break;

	case 4:									// LED4
		kk = PWM_PERIOD - ((PWM_PERIOD - PWM_MIN) * (U32)led_4_level * (U32)pwm_master / 10000L) - 1L;
		pwm4_reg = (U16)kk;
		if(led_4_on) PWM1_2_CMPB_R = kk;
		break;

	case 5:									// LED5
		kk = PWM_PERIOD - ((PWM_PERIOD - PWM_MIN) * (U32)led_5_level * (U32)pwm_master / 10000L) - 1L;
		pwm5_reg = (U16)kk;
		if(led_5_on) PWM1_3_CMPA_R = kk;
		break;

	case 6:									// LED6
		kk = PWM_PERIOD - ((PWM_PERIOD - PWM_MIN) * (U32)led_6_level * (U32)pwm_master / 10000L) - 1L;
		pwm6_reg = (U16)kk;
		if(led_6_on) PWM1_3_CMPB_R = kk;
		break;
	}
}*/

//-----------------------------------------------------------------------------
// set_led() turns on/off the LED outputs
// if lednum == 0xff, do a POR init of the registers (value ignored)
// if lednum == 0xfe, update all LEDs (value ignored).  Update is for flash/blink logic
//	else, value = 1/0 for on/off.
//	led_on reg controls on/off
//	flash_enable controls flash
//	blink_enable controls blink
//	flash_enable and blink_enable should not have the same LEDs enabled, but doing so
//		won't break the system, the LED behavior will simply be a mix of the two modes.
//-----------------------------------------------------------------------------
void set_led(U8 lednum, U8 ledon){
	U8	i;				// led# temp
	U8	j;				// temp
	U8	loop = FALSE;	// loop flag
	U8	v;				// ledon temp

	if(lednum == INIT_LEDS){
		led_on = 0;							// init all LED registers off
		blink_enable = 0;
		flash_enable = 0;
#define	LED_LEV	20
		led_1_level = LED_LEV;				// led level regs (0-100%)
		led_2_level = LED_LEV;				// led level regs (0-100%)
		led_3_level = LED_LEV;				// led level regs (0-100%)
		led_4_level = LED_LEV;
		led_5_level = LED_LEV;
		led_6_level = LED_LEV;
		pwm_master = LED_LEV;				// led master level
		// led pwm regs
		pwm1_reg = (U16)((PWM_PERIOD * LED_LEV) / 100L);
		pwm2_reg = (U16)((PWM_PERIOD * LED_LEV) / 100L);
		pwm3_reg = (U16)((PWM_PERIOD * LED_LEV) / 100L);
		pwm4_reg = (U16)((PWM_PERIOD * LED_LEV) / 100L);
		pwm5_reg = (U16)((PWM_PERIOD * LED_LEV) / 100L);
		pwm6_reg = (U16)((PWM_PERIOD * LED_LEV) / 100L);
		PWM1_1_CMPA_R = PWM_OFF;
		PWM1_1_CMPB_R = PWM_OFF;
		PWM1_2_CMPA_R = PWM_OFF;
		PWM1_2_CMPB_R = PWM_OFF;
		PWM1_3_CMPA_R = PWM_OFF;
		PWM1_3_CMPB_R = PWM_OFF;
	}else{
		// process led settings
		if(lednum == UPDATE_LED_ALL){
			loop = TRUE;							// enable loop to process all LEDs
			i = FIRST_LED;
		}else{
			if(lednum > MAX_LED){
				i = 0;
			}else{
				j = lednum;
				i = 0x01;
				while(j){							// convert scalar LED# to bin mask ledon
					i<<=1;
					j--;
				}
			}
			if(ledon){								// LED on/off
				v = i;
				led_on |= i;
			}else{
				v = 0;
				led_on &= ~i;
			}
			loop = FALSE;							// set one time through loop
		}
		do{
			if(lednum == UPDATE_LED_ALL){			// modify ledon (per LED) if update mode
				if((flash_enable & i) || (blink_enable & i)){
					v = led_on & led_enable & i;
				}else{
					v = led_on & i;
				}
			}
			switch(i){
			case LED01:								// New LED, NOT on 123GXL LaunchPad
				if(v) PWM1_1_CMPA_R = pwm1_reg;
				else PWM1_1_CMPA_R = PWM_OFF;
				break;

			case LED02:								// New LED, NOT on 123GXL LaunchPad
				if(v) PWM1_2_CMPA_R = pwm2_reg;
				else PWM1_2_CMPA_R = PWM_OFF;
				break;

			case LED03:								// New LED, NOT on 123GXL LaunchPad
				if(v) PWM1_1_CMPB_R = pwm3_reg;
				else PWM1_1_CMPB_R = PWM_OFF;
				break;

			case LED04:								// RED LED on 123GXL LaunchPad
				if(v) PWM1_2_CMPB_R = pwm4_reg;
				else PWM1_2_CMPB_R = PWM_OFF;
				break;

			case LED05:								// BLUE LED on 123GXL LaunchPad
				if(v) PWM1_3_CMPA_R = pwm5_reg;
				else PWM1_3_CMPA_R = PWM_OFF;
				break;

			case LED06:								// GREEN LED on 123GXL LaunchPad
				if(v) PWM1_3_CMPB_R = pwm6_reg;
				else PWM1_3_CMPB_R = PWM_OFF;
				break;
			}
			i <<= 1;
			i &= VALID_LED_MASK;
			if(!i) loop = FALSE;
		}while(loop);
	}
}

//-----------------------------------------------------------------------------
// flash_led() sets/clears flash_enable bits
//-----------------------------------------------------------------------------
void flash_led(U8 lednum, U8 fon){
	U8	i;				// led# temp
	U8	j;				// temp

	if(lednum > MAX_LED){
		i = 0;									// failsafe
	}else{
		j = lednum;
		i = 0x01;
		while(j){								// convert scalar LED# to bin mask ledon
			i<<=1;
			j--;
		}
	}
	if(fon){
		flash_enable |= i;						// enable flash
	}else{
		flash_enable &= ~i;						// disable flash
	}
	return;
}

//-----------------------------------------------------------------------------
// blink_led() sets/clears flash_enable bits
//-----------------------------------------------------------------------------
void blink_led(U8 lednum, U8 fon){
	U8	i;				// led# temp
	U8	j;				// temp

	if(lednum > MAX_LED){
		i = 0;									// failsafe
	}else{
		j = lednum;
		i = 0x01;
		while(j){								// convert scalar LED# to bin mask ledon
			i<<=1;
			j--;
		}
	}
	if(fon){
		blink_enable |= i;						// enable flash
	}else{
		blink_enable &= ~i;						// disable flash
	}
	return;
}

//************************
// WAIT utility functions
//************************
//-----------------------------------------------------------------------------
// wait() uses a dedicated ms timer to establish a defined delay (+/- 1LSB latency)
//-----------------------------------------------------------------------------
void wait(U16 waitms)
{
    waittimer = waitms;
    while(waittimer != 0);
    return;
}

//-----------------------------------------------------------------------------
// wait_reg0() waits for (delay timer == 0) or (regptr* & clrmask == 0)
//	if delay expires, return TRUE, else return FALSE
//-----------------------------------------------------------------------------
U8 wait_reg0(volatile uint32_t *regptr, uint32_t clrmask, U16 delay){
	U8 timout = FALSE;

    waittimer = delay;
    while((waittimer) && ((*regptr & clrmask) != 0));
    if(waittimer == 0) timout = TRUE;
    return timout;
}

//-----------------------------------------------------------------------------
// wait_reg1() waits for (delay timer == 0) or (regptr* & setmask == setmask)
//	if delay expires, return TRUE, else return FALSE
//-----------------------------------------------------------------------------
U8 wait_reg1(volatile uint32_t *regptr, uint32_t setmask, U16 delay){
	U8 timout = FALSE;

    waittimer = delay;
    while((waittimer) && ((*regptr & setmask) != setmask));
    if(waittimer == 0) timout = TRUE;
    return timout;
}

//*****************************************************************************
// keypad_init()
//  Initializes keypad input and LED drive functions
//*****************************************************************************
int
keypad_init(U8 fi)
{
	U8	i;				// loop temps
	U8	j;
	U8	k;
	U8	m;
	U32	check32 = 0;	// checksum temp
	U32	tt;				// temp32
	U16	aa;				// temp addr

    iplt2 = 1;										// init timer1
    key_alt = 0;									// set first keypad
    // copy eeprom keymap to ram & calc checksum on the fly
    aa = KEYP_EEBASE_ADDR;
    UARTprintf("-- Keypad INIT --\n");
	for(m=0; m<MAX_ALT_KP; m++){
#ifdef	DEBUG_K
		if(m){
			UARTprintf("Keypad 0:\n");
		}else{
			UARTprintf("Keypad 1:\n");
		}
#endif
	    for(i=0; i<KEYP_ROW; i++){
	    	tt = eerd(aa);
#ifdef	DEBUG_K
	        UARTprintf("eeprom keypad data\n");
	        UARTprintf("%08x\n",tt);
#endif
	    	for(j=0; j<KEYP_COL; j++){
	        	k = (U8)(tt & 0xffL);
#ifdef	DEBUG_K
	            UARTprintf("%u\n",k);
#endif
	        	key_pascii_map[m][i][j] = k;
	        	check32 += (U32)k;
	        	tt >>= 8;
	    	}
	    	aa += 1;								// advance to next EEPROM cell
	    }
	}
    if((check32 != eerd(KEYP_CHECKSUM_ADDR)) || fi){		// checksum fail or force init, init to rom default
        UARTprintf("Checksum fail - init from ROM\n");
    	for(m=0; m<MAX_ALT_KP; m++){
    		for(i=0; i<KEYP_ROW; i++){					// int keymap
    			for(j=0; j<KEYP_COL; j++){
    				key_pascii_map[m][i][j] = key_pascii_map_rom[m][i][j];
    			}
    		}
    	}
    }
    set_led(INIT_LEDS, 0x00);						// init LED levels
    set_led(UPDATE_LED_ALL, 0);						// turn off all LEDs
    while(iplt2);									// wait for init to IPL
    UARTprintf("-- END Keypad INIT --\n");
	return 0;
}

//-----------------------------------------------------------------------------
// save_keymap() stores keymap to eeprom.
//-----------------------------------------------------------------------------
void save_keymap(void){
	U8	i;				// loop temps
	U8	j;
	U8	k;
	U8	m;
	U32	check32 = 0;	// checksum temp
	U32	tt;				// temp32
	U16	aa;				// addr temp

    // copy ram keymap to eeprom & calc checksum on the fly
	aa = KEYP_EEBASE_ADDR;
	for(m=0; m<MAX_ALT_KP; m++){
		for(i=0; i<KEYP_ROW; i++){
			tt = 0;
			for(j=KEYP_COL; j!=0; j--){
				k = key_pascii_map[m][i][j-1];
				tt <<= 8;
				tt |= (U32)(k);
				check32 += (U32)k;
			}
			eewr(aa, tt);
			aa += 1;
		}
    }
	eewr(KEYP_CHECKSUM_ADDR, check32);
	return;
}

//-----------------------------------------------------------------------------
// store_keycode() stores keycode to keymap.
//-----------------------------------------------------------------------------
void store_keycode(char pc, U8 kcode){
	U8	i;				// loop temps
	U8	j;

	i = kcode >> 4;
	j = kcode & 0x03;
	key_pascii_map[key_alt][i][j] = pc;
	return;
}

//-----------------------------------------------------------------------------
// got_key() returns true if key is pressed.
//-----------------------------------------------------------------------------
U8 got_key(void){
	char	rtn = FALSE;	// return value

	if(kbd_hptr != kbd_tptr) rtn = TRUE;			// key in buffer means a key was pressed
	return rtn;
}

//-----------------------------------------------------------------------------
// get_key() returns next keypad ASCII key or 0x00 if none
//-----------------------------------------------------------------------------
char get_key(void){
	char c = '\0';

	if(kbd_hptr != kbd_tptr){						// if the circular buffer is not empty,
		c = kbd_buff[kbd_tptr++];					// get the ASCII key code and advance the circular ptr
	}
	if(kbd_tptr >= KBD_BUFF_END){
		kbd_tptr = 0;								// wrap buffer ptr
	}
	return c;
}

//-----------------------------------------------------------------------------
// get_keycode() returns current keycode.  Must be called before get_key
//-----------------------------------------------------------------------------
char get_keycode(void){

	return map_buff[kbd_tptr];
}

//-----------------------------------------------------------------------------
// kp_asc() converts captured keycodes to ASCII
//	Uses debug timing matrix key_press[][] to determine what keys are pressed.
//	places detected keys into the kbd_hptr.  KEYP_RELEASE semaphores are buffered
//	to indicate key release.  The released key-code follows these semaphores in
//	the buffer.
//-----------------------------------------------------------------------------

void kp_asc(void){
	U8	k;			// temps
	U8	i;

	for(k=0; k<5; k++){
		for(i=0; i<4; i++){
			if(key_press[k][i] == KP_DEBOUNCE_DN){	// key detected
				key_press[k][i] += 1;				// disable future detects
				map_buff[kbd_hptr] = (k << 4) | i;
				kbd_buff[kbd_hptr++] = key_pascii_map[key_alt][k][i];
				if(kbd_hptr >= KBD_BUFF_END){
					kbd_hptr = 0;					// wrap buffer ptr
				}
			}
			if(key_press[k][i] == 0x80){			// key released
				key_press[k][i] = 0;				// reset key
				kbd_buff[kbd_hptr++] = KEYP_RELEASE;
				if(kbd_hptr >= KBD_BUFF_END){
					kbd_hptr = 0;					// wrap buffer ptr
				}
				kbd_buff[kbd_hptr++] = key_pascii_map[key_alt][k][i];
				if(kbd_hptr >= KBD_BUFF_END){
					kbd_hptr = 0;					// wrap buffer ptr
				}
			}
		}
	}
	return;
}

//-----------------------------------------------------------------------------
// init_kp() initializes key press matrix (sets all entries to 0)
//-----------------------------------------------------------------------------
//
void init_kp(void){
	U8	i;		// temps
	U8	j;

	for(i=0; i<ROW_MAX; i++){
		for(j=0; j<COL_MAX; j++){
			key_press[i][j] = 0;
		}
	}
	return;
}

//-----------------------------------------------------------------------------
// set_kpalt() sets the key_alt register.  returns value of key_alt reg.
//	call with value > MAX_ALT_KP to read without setting
//-----------------------------------------------------------------------------
//
U8 set_kpalt(U8 altval){

	if(altval < MAX_ALT_KP){
		key_alt = altval;
	}
	return key_alt;
}

//-----------------------------------------------------------------------------
// store_userps() stores keycode user ps to EEPROM.
//-----------------------------------------------------------------------------
void store_userps(char pc){

	eewr(USRPS_EEADDR, (U32)pc);
	return;
}

//-----------------------------------------------------------------------------
// get_userps() returns the user-ps from EEPROM.
//	if EEPROM invalid, returns null
//-----------------------------------------------------------------------------
char get_userps(void){
	uint32_t	c;		// temp

	c = eerd(USRPS_EEADDR);
	if(c > 255){
		c = '\0';				// return a null if invalid
	}
	return (char)c;
}

//-----------------------------------------------------------------------------
// get_pace_flag() returns the pace_flag (clears after reading)
//	the pace_flag is set in the Timer2_ISR at the rate specified therein.
//  This is used to provide a regular, timed flag for controlling the rate
//  of an operation.
//-----------------------------------------------------------------------------
U8 get_pace_flag(void)
{
	U8	i;			// temp return

	i = pace_flag;					// set the return value
	if(i){							// mitigate contention with the ISR...
		pace_flag = 0;				// if flag set, clear
	}
    return i;
}

//-----------------------------------------------------------------------------
// Timer2_ISR
//-----------------------------------------------------------------------------
//
// Called when timer2 A overflows (NORM mode):
//	intended to operate @ 1ms per lsb.  Supports wait timer and pacing timer used
//	by main() to pace the ADC reads.
//
//	For this application, the keypad must be capable of multi-keypress detection.
//	The system scans the matrix and captures pressed keys into a timing matrix.
//	If the timing value exceeds a minimum setpoint, the key is considered "pressed"
//
//-----------------------------------------------------------------------------

void Timer2_ISR(void){
		U8	i;						// loop temps
		U8	j;
		U8	k;
		U8	key_temp;
static	U8	key_addr;
static	U8	key_row;
static	U8	idle_count;
static	U8	pacetimer;
#define	PACE_TIME 10				// pace timer ms

	TIMER2_ICR_R = TIMER2_MIS_R;					// clear IFR
	if(iplt2){										// if flag is set, init local statics
		iplt2 = 0;
		key_row = KB_ROW_START;
		key_addr = KB_ADDR_START;
		GPIO_PORTE_DATA_R = (GPIO_PORTE_DATA_R & ~KB_ROW_M) | (~key_row & KB_ROW_M);
		pacetimer =	PACE_TIME;
		pace_flag = 0;
	}
	// local keypad
	// a matrix of debounce timings is maintained for when a keypress is detected.  This
	//	allows multiple presses to be detected simultaneously.

	if(++idle_count & 0x01){
		key_temp = (~GPIO_PORTA_DATA_R) & KB_COL_M;		// read keypad col bits
		k = key_addr - 1;
		for(i=0, j=KCOL0; i<4; i++, j<<=1){
			if(key_temp & j){
				if(key_press[k][i] < KP_DEBOUNCE_DN){	// process press
					key_press[k][i] += 1;
				}
			}
			if(!(key_temp & j)){
				if(key_press[k][i] != 0x80){
					if(key_press[k][i] >= KP_DEBOUNCE_DN){
						key_press[k][i] = 0x80;			// process release
					}else{
						key_press[k][i] = 0;			// reset
					}
				}
			}
		}
		key_addr -= 1;									// advance & update address
		key_row >>= 1;
		if(!key_row){
			key_row = KB_ROW_START;
			key_addr = KB_ADDR_START;
		}
		GPIO_PORTE_DATA_R = (GPIO_PORTE_DATA_R & ~KB_ROW_M) | (~key_row & KB_ROW_M);
	}
	if(waittimer){										// update count-from-N-and-halt timer
		waittimer -= 1;
	}
	if(!(--pacetimer)){									// update pacing timer and flag
		pace_flag = 1;
		pacetimer = PACE_TIME;
	}
	// process LED flash
	if(flash_timer == 0){
		led_enable ^= flash_enable;
		flash_timer = FLASH_RATE;
	}
	flash_timer -= 1;
	if(blink_timer == 0){
		blink_timer = BLINK_RATE;
		led_enable |= blink_enable;
	}
	if(blink_timer == BLINK_OFF){
		led_enable &= ~blink_enable;
	}
	blink_timer -= 1;
}

//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------

