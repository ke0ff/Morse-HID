/********************************************************************
 ************ COPYRIGHT (c) 2019 by KE0FF, Taylor, TX   *************
 *
 *  File name: morse.c
 *
 *  Module:    Control
 *
 *  Summary:   This is the morse keyboard fn (tone and element decode)
 *				Designed for the TM4C123GXL LaunchPad (TM4C123GH6PM)
 *
 ********************************************************************************************************
 *  PB1 (DIT) and PB0 (DAH) are used to input the iambic paddle state.  These GPIOs connect to switch
 *  closures to GND to signify "mark", with "space" signified by the switch in the open state.  Weak
 *  pull-ups are too weak to be practical as the GPIOs can be falsed by ESD events.  Thus, external
 *  pullups, filter capacitors, and TVS diodes are recommended.
 *
 *  GPIO edge (falling) interrupts are used to trap transitions on these pins.  The GPIO ISR (ditdah_isr)
 *  starts the debounce timer and enables the TIMER3A ISR which produces the side tone as a PWM-based
 *  DDS (direct digital synthesis) tone generator and also handles element timing including intra-element
 *  spacing.  Essentially, this creates a Morse Code keyer that traps the sent elements and decodes them
 *  into plain-text characters that can be served to a higher-order application.
 *
 *  TIMER3A is used as a PWM and periodic interrupt.  The PWM DAC is used to generate DDS tones for CW
 *  elements and time those elements and inter-element spaces.  Shaping logic is used to impose a base-2
 *  logarithmic(ish) attack/decay profile when tones are activated or de-activated.
 *
 *  Tones and spaces are based on a DIT_TIME granularity. A state machine topology processes the
 *  transitions between elements.  A debounce timer is also maintained in the TIMER3A ISR.  While it
 *  would be convenient to simply use the edge detect flags, noise on the DIT/DAH contacts makes this
 *  impractical without hardware filtering. The debounce timer allows this function to be implemented
 *  in software.
 *
 *  The state machine captures the element pattern (16b, dits = "0", dahs = "1") and the number of
 *  elements (8b).  The element pattern is right-justified with the last element at the pattern lsb.
 *  A look-up-table (LUT) is used to correlate the element pattern and length with a character or control
 *  code.  Since 16 bits are used to capture the elements, up to 16 elements can be accommodated for
 *  character recognition.  This allows for sufficient latitude to create custom pro-signs, commands,
 *  and control characters.  The LUT holds some commentary regarding the structure of these special
 *  characters.  New characters may be added (up to 255, total).  Use the comment protocol to aid
 *  in the coding and documentation of the LUT.
 *
 *  The CW elem/len data is placed into a wrap-around buffer that is 40 "characters" long. A "head/tail"
 *  index scheme is used to fill and empty the table with a status register capturing over-run conditions.
 *  A "getchar" style function pulls the buffered data and calls a look-up function which returns the
 *  ASCII character that that corresponds to the elem/len data.
 *
 *  Straight-Key support
 *
 *  Straight-key support has been added.  This feature uses a jumper strap to select iambic (no jumper)
 *  	or straight-key (jumper).  Straight-key mode uses the DIT GPIO as the sole CW input. A flag
 *  	register (paddle_key_mode: 1 = iambic, 0 = straight-key) is used to branch code execution
 *  	where the basic algorithms differ between the two modes.
 *
 *  The following functions feature such branch tests:
 *  morse_init() - when configuring the GPIO edge interrupts
 *  get_cw_asc() - since the data capture for straight-key is fundamentally different from iambic,
 *  	this function must use timing data for a character to determined the DAH threshold before
 *  	using the value to construct a DIT/DAH composite word and length used by decode_elem().
 *  get_cwdr() - straight key mode only captures one character in the timing buffer, so a flag
 *  	is used to determine if a character is ready to process, which differs from the iambic method
 *  	which uses a head-tail indexing scheme to buffer bitmaps of multiple characters.
 *  didah_isr() - only the DIT input is captured for straight-key operation, so there is a branch that
 *  	ignores the DAH pin.
 *  Timer3A_ISR() - This ISR is split completely so that each mode can operate with a minimum of
 *  	test-and-branch overhead.  This somewhat increases code size while reducing execution time
 *  	during a high-rate (10000 Hz) interrupt function.
 *
 *  The straight-key mode captures elapsed time, at a resolution of 0.1ms, of key-down (mark) and key-up
 *		(space) events during character entry.  Both values are stored sequentially in a single U32 array
 *		with the high-bit set to indicate key-down.  Once a letter_space time out has expired, the ISR
 *		signals that a character is ready to process and the Process_CW() loop accesses the timing array
 *		and calculates the DIT/DAH pattern and number of elements.  Averaging is employed to allow the
 *		system to track the user's code speed and adjust the threshold values accordingly.  Also, a
 *		moderately complex algorithm is employed to determine the timing threshold for DIT/DAH determination.
 *			For "all-same" element characters (includes single element), the running dit-mark average is used
 *				as the basis.
 *			For multi-element characters, dit-mark and dah-mark values are averaged to create the timing threshold.
 *			The actual timing threshold is equal to 2x the basis value.
 *		This algorithm attempts to find the best fit value for a DIT given a single character's timings
 *		If there is insufficient or conflicting information in the captured timing values, the running
 *		value is used as a fall-back.  The running value is "filter averaged" with new (and valid) values
 *		by adding the new value to the running average and dividing by two.  Given consistent inputs from
 *		the operator, this will result in a running-average result that converges on the operator's normal
 *		speed after 3 to 6 characters.
 *  Initial Key-down (any keydown that occurs after a word space) is captured using the GPIO edge interrupt
 *  	peripheral which initializes character capture registers, disables the GPIO ISR, and enables the
 *  	Timer3A_ISR.  During the Timer3A_ISR in paddle mode, the edge detect peripheral is still used for
 *  	edge captures, but only by polling the ISR registers.  The straight-key mode uses a state machine
 *  	to capture edges by polling the GPIO data register.  Key-up is captured by polling the GPIO data
 *  	register in both modes.  Debounce timing is also provided (inside the Timer3A_ISR).
 *  Once a word space has expired, the system reacts similarly to the iambic case.  For straight-key,
 *  	a flag is set to trigger auto-space (if enabled), Timer3A_ISR is disabled, and the GPIO edge
 *  	interrupt is enabled.  The next key-down restarts the character cycle.
 *  A "reset" feature is provided to allow a long-key-down (about 12 seconds) to reset the running dit_ave
 *  	register back to power on defaults (10 WPM).  The code also range checks new timing values to
 *  	make sure they stay within the 5 to 55 WPM range (timing values outside that range are rejected).
 *  All other features, such as pro-signs, auto-space, etc..., are retained in the straight-key mode.
 *
 *  Text to CW:
 *  The basic code to drive CW->Text is reversed to produce CW from a text input.  This involves
 *  looking up the text character to deliver the element/len data.  (the same data used to match input
 *  elements).  This data is then shifted to produce a DIT/DAH pattern.  This might either use the
 *  same tone output, or a separate tone output (TONE2) with separate code.  The separate method would
 *  allow simultaneous operation but would increase ISR load.  The CW-Text code currently utilizes less
 *  than 9% (ave) of the CPU time on a 180us cycle and less than 16% peak.
 *
 *******************************************************************************************************/

/*******************************************************************************************************
 *  File scope declarations revision history:
 *    Version 5:
 *    06-18-19 jmh:  Included straight-key mode and finalized decode logic
 *    Version 2:
 *    04-27-19 jmh:  Had Iambic modes A/B transposed.  Corrected the nomenclature and made mode
 *    					"B" the no-jumper default.
 *    				 Fixed fact init bug
 *    04-20-19 jmh:  Added strap-options.  If grounded, select option:
 *    					PC4: power-on paddle reverse
 *    					PC5: Fact init
 *    					PC7: TLC04 select
 *    					PD6: Weight adjust enable (on tone pot)
 *    					PD7: PGM Keypad enable
 *    					PA7: Lock speed/tone/weight
 *    					PF4: Enable CTRL-Z shortcut pro-sign
 *    					PB7: Set iambic "A" mode (else "B" mode if open)
 *    				 Added weight control (uses tone pot data fed from main()) and a strap-option)
 *    				 	to allow +/- 25% variation in intra-element (silence) spacing.
 *
 *    04-16-19 jmh:  Fixed bug with DR flag in cw status.  Eliminated the flag and transitioned
 *    					to a head/tail test.
 *    				 Added suppress_s and logic to suppress autospaces after non-printing characters
 *    				 Changed auto-space command to /CR.  /BT is now the only "Enter" key.  /SN is
 *    				    retired (permanently - it is too easy to send by accident).
 *    				 POT inputs and /LOK fully integrated
 *
 *    04-12-19 jmh:  Added hooks for pot inputs to support run-time tone and speed settings
 *
 *    04-09-19 jmh:  Added hooks to support paddle-swap in software.
 *    				 Added "/LOK" prosign and functions to set/swap the paddle registers
 *
 *	  Version 1:
 *    04-06-19 jmh:  Port of ACU morse interpreter complete, basic Morse->USB keyboard functional.
 *
 *
 *******************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "inc/tm4c123gh6pm.h"
#include "typedef.h"
#include "init.h"
#include "morse.h"
#include "morse_lut.h"
#include "sine_c.h"
#include "eeprom.h"

//#define	DEBUG_CW							// define this semaphore to display CW capture statistics

//=============================================================================
// external defines
extern const U16 	cw_elem_map[];		// Morse element map (morse_lut.c)
extern const uint8_t cw_len_map[];		// length (number of "voiced" dit/dah elements) (morse_lut.c)
extern const char	cw_text_map[];		// ASCII character map (corresponds to Morse element map index) (morse_lut.c)
extern const char cw_text_msg[][CWTXT_LEN]; // Morse response identifiers

//=============================================================================
// local registers
// Morse input registers
// Straight key registers
volatile U32		key_timer;					// key activity timer (straight-key mode)
volatile U32		letter_space;				// letter & word registers (straight-key mode)
volatile U32		word_space;
volatile U32		cw_timing_buf[CW_BUFF_END];	// cw element buffer (straight-key mode)
volatile U8			cwelem_idx;					// element buffer index
volatile U8			letter_complete;			// letter complete flag
		 U32		dit_ave;					// averaging registers
		 U32		dah_ave;
		 U32		spc_ave;
volatile U8			aspace;						// straight key autospace detected flag

// paddle mode (keyer) & common registers
volatile U8			paddle_force_mode;			// register allows user psorign to override paddle mode setting, set to 0 on reset
volatile U8			paddle_key_mode;			// if true, process straight key mode (dit input is key)
volatile U16		dit_time_reg;				// dit/dah timing registers (for real-time speed control)
volatile U16		ditie_time_reg;				// dit-inter-element-space
volatile U16		dah_time_reg;
volatile U16		weight_reg;					// amount to subtract from base DIT time for inter-element dit space
volatile U8			dit_port;					// dit/dah port pin IDs
volatile U8			dah_port;					// dit/dah port pin IDs
volatile U16		tone_reg;					// tone holding register
volatile U16		delph1;						// tone 1 delta phase
volatile U16		phaccum1;					// tone 1 phacc
volatile U16		tone_timer;					// timer, runs at 1/fasmp rate
volatile U8			keyer_mode;					// keyer mode, 'A' or 'B'
volatile U8			initial;					// aos from CW paddles == idle
volatile U16		trap_word;					// CW element capture reg
volatile uint8_t	trap_count;					// CW element counter
volatile uint8_t	trap_bit;					// CW element reg
volatile uint8_t	silence_count;				// silence counter
volatile uint8_t	pstate;						// paddle edge state
		 uint8_t	suppress_s;					// autospace suppress
volatile U16		debounce_timer;				// timer to debounce falling edges of switch inputs

volatile U16		cw_elem_buf[CW_BUFF_END];	// cw element buffer
volatile uint8_t	cw_len_buf[CW_BUFF_END];	// cw length buffer
volatile uint8_t	cw_head;					// element buffer pointers
volatile uint8_t	cw_tail;
volatile uint8_t	cw_stat;					// status reg
volatile uint8_t	cw_astat;					// app status reg
volatile uint8_t	rampup;						// attack tracking reg
volatile uint8_t	rampdn;						// decay tracking reg
volatile uint8_t	rampcyc;					// ramp rate reg
volatile U8			ramp_rate_reg;				// ramp rate reg (varies with speed)
volatile U8			gpiob_mem;					// ramp rate reg (varies with speed)

char	cw_text_buf[CWT_BUFF_END];	// Morse input text buffer (circular)
uint8_t	cwt_head;					// text buffer pointers
uint8_t	cwt_tail;
uint8_t	cwt_stat;					// text buffer status reg

//Morse send registers
volatile U16		tone2_reg;					// reply tone holding register
volatile U16		tone3_reg;					// beep tone holding register
volatile U16		delph2;						// tone 2 delta phase
volatile U16		phaccum2;					// tone 2 phacc
volatile U16		toneb_timer;				// timer, runs at 1/fasmp rate
volatile U8			initialb;					// aos from CW paddles == idle
volatile U8			sendingb;					// !=0 if processing CW letter
volatile U16		trapb_word;					// CW element capture reg
volatile uint8_t	trapb_count;				// CW element counter
volatile uint8_t	rampupb;					// attack tracking reg
volatile uint8_t	rampdnb;					// decay tracking reg
volatile uint8_t	rampcycb;					// ramp rate reg
volatile U8			ramp_rateb_reg;				// ramp rate reg (varies with speed)

//=============================================================================
// local Fn declarations
void timer3A_init(U32 sys_clk);
void timer3B_init(U32 sys_clk);
U8 get_cwdr(void);
char decode_elem(U16 elem, U8 len);
void set_ramp_rate(void);
U8 get_poweron_lock_strap(void);

//=============================================================================

//-----------------------------------------------------------------------------
// Process_CW() routine.  Initializes Morse keyboard systems and variables
//	PORTB is DIT/DAH inputs (active low), uses MCU pullups
//	timer3A drives debounce & tone timers, DDS algorithm, and element trap state
//	machine.
//
// This function runs as a periodic state machine and should be called from the
//	main program loop during idle periods.  The bulk of the time critical code
//	do input Morse elements happens in interrupts, so the timing of this function
//	isn't critical as long as it is executed more often than about 10x the DIT
//	rate.
//
// Keypad processing also happens here by calling kp_asc().  kp_asc() processes valid
//	keypresses and transfers them to a circular buffer that is extracted by calling
//	get_key()
//-----------------------------------------------------------------------------
char Process_CW(U8 cmd){
	char	c;					// temp char
	U8		i;					// temp ints
	U8		s;
	U8		rtn = 0;			// default return
	static	U8	cws_process_cmd;
	static	U8	char_status;

	if(cmd == INIT_PROCESS){								// initialize the system (power on)
 		if(morse_init()){
 			rtn = CW_FACINT;
 		}
		cws_process_cmd = 0;
		char_status = 0;
	    keypad_init(rtn);
	}else{
		kp_asc();											// process key-presses
		set_led(UPDATE_LED_ALL, 0);							// process LED blink/flash
		s = get_cwstat();									// check if an activity in status register
		if(s) char_status |= s;								// if so, capture status (prevents contention if interrupt later changes the SR)
		if(get_cwdr()){										// if CW char ready
			c = get_cw_asc();								// get char
			switch(c){
			case '\0':										// ignore invalid characters
         		suppress_s = TRUE;							// suppress next autospace
				char_status = 0;							// clear char ready if no char in buffer
         		s &= ~CW_STB;								// disable beep
				break;

			case MRSE_DEL:									// no autospace for modifier/delete characters
			case CWSTT:
			case MRSE_SHIFT:
			case MRSE_CAPLOCK:
			case MRSE_PGUP:
			case MRSE_PGDN:
			case MRSE_ALT:
			case MRSE_CNTL:
			case MRSE_WINL:
			case MRSE_WIN:
			case MRSE_F1:
			case MRSE_F2:
			case MRSE_F3:
			case MRSE_F4:
			case MRSE_F5:
			case MRSE_F6:
			case MRSE_F7:
			case MRSE_F8:
			case MRSE_F9:
			case MRSE_F10:
			case MRSE_F11:
			case MRSE_F12:
			case MRSE_UP:
			case MRSE_DN:
			case MRSE_LEFT:
			case MRSE_RIGHT:
			case MRSE_BACKCSP:
			case MRSE_TAB:
			case MRSE_CR:
			case MRSE_ESC:
			case MRSE_CWLOCK:
			case MRSE_REVRS:
			case MRSE_WORDDEL:
			case MRSE_WORDBS:
			case MRSE_SHLK:
			case MRSE_KPSWP:
			case MRSE_CTRLZ:
				// enter any new command/modifier characters as "case" statements here
         		suppress_s = TRUE;							// supress next autospace
         		s &= ~CW_STB;								// disable beep
			default:
				cw_text_buf[cwt_head] = c;
				if(c == CW_CR){
					rtn = CW_CR; 							// return CR if CR
					cws_process_cmd = CWEOM;				// "/sk" is CR response
				}
				if(c == CWESC){
					rtn = CW_CR;							// return CR if ESC
					cwt_tail = cwt_head;					// scrub buffer
				}
				if(c == CWTAB){
					rtn = CW_CR;							// return CR if tab
				}
				if(++cwt_head >= CWT_BUFF_END){
					cwt_head = 0;
				}
				if(cwt_head == cwt_tail){
					cwt_stat |= CW_OR;						// buffer overrun, set error
				}
				break;
			}
			if(cwt_head > cwt_tail) i =  cwt_head - cwt_tail;
			else i =  CWT_BUFF_END - (cwt_tail - cwt_head);
			if(i > CWT_BUFF_70){
				cwt_stat |= CW_FF;
			}
		}
		if(!sendingb){										// no send in progress
			if((s & CW_ST) && (s & CW_STB)){				// if auto space and beep activated
				trapb_word = 0;								// set up to send a word space
				trapb_count = 0;
				initialb = 0x04;							// trigger beep to signal that an "auto-space" has been issued
				TIMER3_IMR_R = (TIMER3_IMR_R & TIMER3_IMR_BMASK) | TIMER_IMR_CBEIM;
				cw_stat &= ~CW_STB;
			}else{
				if(cws_process_cmd){
					put_cw(cws_process_cmd);				// send char
					cws_process_cmd = 0;
				}
			}
		}
	}
	return rtn;
}

//-----------------------------------------------------------------------------
// morse_init() routine.  Initializes morese kybd systems and variables
//	portB is DIT/DAH inputs (active low)
//	timer3A drives debounce & tone timers, DDS alg, and element trap state machine
//	timer3B drives a second DDS tone/Morse generator.
//-----------------------------------------------------------------------------
U8 morse_init(void){
	U8	i;				// loop temps
	U8	m;
	U8	rtn = FALSE;	// fact init return
	U32	aa;

	// detect straight key jumper
	paddle_key_mode = get_paddle_mode_strap();
//	paddle_key_mode = 0;	// debug patch to test without needing to move a jumper
	// init GPIO debounced edge detect register
	pstate = 0;
	// init port IDs
	if(GPIO_PORTC_DATA_R & PADDLE_ORIENT){			// set power-on paddle orientation
		dit_port = NDIT;							// normal
		dah_port = NDAH;
	}else{
		dit_port = NDAH;							// reversed
		dah_port = NDIT;
	}
	if(~GPIO_PORTC_DATA_R & FACTORY_DEFAULT){		// reset EEPROM
		rtn = TRUE;
		for(m=0; m<MAX_ALT_KP; m++){
		    for(i=0; i<KEYP_ROW; i++){
				eewr(aa, 0xffffffff);
				aa += 1;
		    }
		}
		eewr(KEYP_CHECKSUM_ADDR, 0xffffffff);		// reset keypad
		eewr(SPEED_EEADDR, 0xffffffff);				// reset STW and LED brt
		eewr(TONE_EEADDR, 0xffffffff);
		eewr(WEIGHT_EEADDR, 0xffffffff);
		eewr(LEDBRT_EEADDR, 0xffffffff);
		eewr(LEDBRT_EEADDR+1, 0xffffffff);
		eewr(USRPS_EEADDR, 0xffffffff);				// clear user pro-sign
	}
	// init timing/freq regs
	init_stw();
	// init GPIO interrupt for paddles
	GPIO_PORTB_IM_R = 0;							// mask isr
	GPIO_PORTB_IS_R = 0;							// edge detect
	GPIO_PORTB_IBE_R = 0;							// 1 edge
	GPIO_PORTB_IEV_R = 0;							// falling edge/low level
	if(!paddle_key_mode){							// straight key mode (dit only)
		GPIO_PORTB_ICR_R = NDIT;					// pre-clear edge detect flags
		// enable GPIO PB intr (PB0 & PB1)
		NVIC_EN0_R = (NVIC_EN0_GPIOB);
		GPIO_PORTB_IM_R = NDIT;					// un-mask isr
	}else{											// paddle mode
		GPIO_PORTB_ICR_R = (dah_port | dit_port);	// pre-clear edge detect flags
		// enable GPIO PB intr (PB0 & PB1)
		NVIC_EN0_R = (NVIC_EN0_GPIOB);
		GPIO_PORTB_IM_R = dah_port | dit_port;		// un-mask isr
	}
	// Init file-local variables
	set_iambic_mode();								// set iambic mode per jumper
	rampcyc = 0;									// init attack/decay counter
	tone_timer = 0;									// timer, runs at 1/fasmp rate
	initial = 0;									// set paddles idle
	trap_word = 0;									// CW element capture reg
	trap_count = 0;									// CW element counter
	trap_bit = 0;									// CW element reg
	silence_count = 0;								// end of letter counter (in DIT times)
	cw_head = 0;									// init element buffer indices
	cw_tail = 0;
	cwt_head = 0;									// init text buffer indices
	cwt_tail = 0;
	cw_stat = 0;									// no auto-space
	cwt_stat = 0;
	suppress_s = 0;									// disable autospace supress
	// init tone2 locals
	tone2_reg = TONE_800;							// reply tone holding register
	tone3_reg = TONE_1000;							// beep holding register
	delph2 = 0;										// tone 1 delta phase
	phaccum2 = 0;									// tone 1 phacc
	toneb_timer = 0;								// timer, runs at 1/fasmp rate
	initialb = 0;									// aos from CW paddles == idle
	sendingb = 0;									// !=0 if processing CW letter
	trapb_word = 0;									// CW element capture reg
	trapb_count = 0;								// CW element counter
	rampcycb = 0;									// ramp rate reg
	// init timers
	timer3A_init(SYSCLK);							// Init DDS (timer3A)
	timer3B_init(SYSCLK);							// Init DDS (timer3B)
	cwelem_idx = 0;
	letter_space = LETTER_SK * DIT_SK;				// letter & word registers (straight-key mode)
	word_space = WORD_SK * DIT_SK;
	dit_ave = DIT_SK;
	dah_ave = DIT_SK * 3;
	spc_ave = DIT_SK;
	letter_complete = FALSE;
	aspace = FALSE;
	gpiob_mem = ~GPIO_PORTB_DATA_R & NDIT;			// cancel edge detect for first power-on run
	return rtn;
}

//*****************************************************************************
//	Initialize timer3A (drives the A-DDS tone gen and paddle input tracking)
//		in PWM mode
//*****************************************************************************
void timer3A_init(U32 sys_clk){
	uint16_t		temp;			// temp 16bit
	volatile U32	ui32Loop;

	NVIC_DIS1_R = NVIC_EN1_TIMER3A;								// disable t3a in the NVIC_EN regs
	SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R3;
	ui32Loop = SYSCTL_RCGCTIMER_R;
	GPIO_PORTB_AFSEL_R |= TONEA;								// enable alt.fn. for PB2 (timer3)
	GPIO_PORTB_PCTL_R &= 0xfffff0ff;
	GPIO_PORTB_PCTL_R |= 0x00000700;
	TIMER3_CTL_R &= ~0x007f;									// disable timer
	TIMER3_CFG_R = TIMER_CFG_16_BIT;
	TIMER3_TAMR_R = TIMER_TAMR_TAMR_PERIOD | TIMER_TAMR_TAAMS;	// PWM mode
	TIMER3_TAPR_R = TIMER3_PS;
	temp = (uint16_t)(sys_clk/(TIMER3_FREQ * (TIMER3_PS + 1)));
	TIMER3_TAILR_R = temp;
	TIMER3_TAMATCHR_R = DDSPWM_MID;								// set min DAC out (or very nearly)

	TIMER3_CTL_R  = TIMER_CTL_TAEVENT_POS;						// set interrupt event
	TIMER3_TAMR_R |= TIMER_TAMR_TAPWMIE;						// enable timer int
	TIMER3_CTL_R |= (TIMER_CTL_TAEN);							// enable timer
	NVIC_PRI8_R &= NVIC_PRI8_INT35_M;
	NVIC_EN1_R = NVIC_EN1_TIMER3A;								// enable t5a in the NVIC_EN regs
	TIMER3_IMR_R = (TIMER3_IMR_R & TIMER3_IMR_AMASK) | TIMER_IMR_CAEIM;
}

//*****************************************************************************
//	Initialize timer3B (drives the B-DDS tone gen) in PWM mode
//*****************************************************************************
void timer3B_init(U32 sys_clk){
	uint16_t		temp;			// temp 16bit
	volatile U32	ui32Loop;

	sendingb = 0;
	NVIC_DIS1_R = NVIC_EN1_TIMER3B;								// disable t3b in the NVIC_EN regs
	SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R3;
	ui32Loop = SYSCTL_RCGCTIMER_R;
	GPIO_PORTB_AFSEL_R |= TONEB;								// enable alt.fn. for PB3 (timer3B)
	GPIO_PORTB_PCTL_R &= 0xffff0fff;
	GPIO_PORTB_PCTL_R |= 0x00007000;
	TIMER3_CTL_R &= ~0x6f00;									// disable B timer
	TIMER3_CFG_R = TIMER_CFG_16_BIT;
	TIMER3_TBMR_R = TIMER_TBMR_TBMR_PERIOD | TIMER_TBMR_TBAMS;	// PWM mode
	TIMER3_TBPR_R = TIMER3_PS;

	TIMER3_CTL_R  &= ~TIMER_CTL_TBEVENT_M;
	TIMER3_TBMR_R |= TIMER_TBMR_TBPWMIE;						// enable timer int
	temp = (uint16_t)(sys_clk/(TIMER3_FREQ * (TIMER3_PS + 1)));
	TIMER3_TBILR_R = temp;
	TIMER3_TBMATCHR_R = DDSPWM_MID;								// set min DAC out
	TIMER3_CTL_R |= (TIMER_CTL_TBEN);							// enable timer
	NVIC_EN1_R = NVIC_EN1_TIMER3B;								// enable t3b in the NVIC_EN regs
	TIMER3_IMR_R = (TIMER3_IMR_R & TIMER3_IMR_BMASK) & ~TIMER_IMR_CBEIM;
}

//*****************************************************************************
//                             MORSE OUTPUT Fns
//*****************************************************************************

//-----------------------------------------------------------------------------
// init_stw() does a fault recovery init of STW settings to the header-file
//	define values
//-----------------------------------------------------------------------------
void init_stw(void)
{
	weight_reg = 0;									// init Morse timing parameters
	dit_time_reg = DIT_TIME;
	ditie_time_reg = DIT_TIME + BASE_WEIGHT + weight_reg;
	dah_time_reg = 3 * dit_time_reg;
	set_ramp_rate();
	tone_reg = TONE_600;							// set base tone freq in working register
	delph1 = 0;										// diable tone
	return;
}

//-----------------------------------------------------------------------------
// set_tone() sets the side-tone freq
//	tone_reg = the ADC result, averaged and divided by 8.  0 => 300Hz, 512 => 2100 Hz
//-----------------------------------------------------------------------------
U16 set_tone(U16 toneadc)
{
	U32		i;		// temp

	// From the header file, the tone formula is:
	//	(U16)(freq * (RADIX * N_SIN) / FSAMP)		; where "freq" is in Hz
	//
	// ADC value range is 0 - 512, which corresponds to a desired tone range of 300 - 2100 Hz,
	//	or about 3.52 Hz per ADC value LSb.  This gives:
	//	(toneadc * 3.52) + 300 = tone frequency (Hz)
	// To stay in integer space, but keep a reasonable resolution, we pre-multiply the tone
	// constant by 100, then divide by 100 to get more resolution:
	//	((toneadc * 352)/100) + 300 = tone frequency

	i = (((U32)toneadc * 352L) / 100L) + 300;
	tone_reg = (U16)(i * (RADIX * N_SIN) / FSAMP);
	return tone_reg;
}

//-----------------------------------------------------------------------------
// set_speed() sets the Morse code speed
//	speedadc = WPM - 5
//-----------------------------------------------------------------------------
void set_speed(U16 speedadc)
{
	// From the header file, the DIT timing value formula is:
	// DIT time = ((FSAMP * WPM_CONST) / (1000 * wpm)) (in peripheral timer units)
	//
	// The speed value range is 0 - 64, corresponding to a WPM range of 5 to 69, or 1
	// WPM per ADC LSb.
	//	speedadc  + 5 = WPM

	dit_time_reg = ((FSAMP * WPM_CONST) / (1000 * (speedadc + 5)));
	ditie_time_reg = dit_time_reg + BASE_WEIGHT + weight_reg;
	dah_time_reg = dit_time_reg * 3;
	set_ramp_rate();
	return;
}

//-----------------------------------------------------------------------------
// store_stw_ee() stores speed/tone/weight to EEPROM
//-----------------------------------------------------------------------------
void store_stw_ee(void)
{
	U32	ii;			// temps

	ii = (U32)dit_time_reg << 16;
	ii |= (U32)ditie_time_reg;
	eewr(SPEED_EEADDR,ii);
	eewr(TONE_EEADDR,(U32)tone_reg);
	eewr(WEIGHT_EEADDR,(U32)weight_reg);
	return;
}

//-----------------------------------------------------------------------------
// get_stw_ee() reads speed/tone/weight from EEPROM.  If non 0xffffffff, store
//	to registers.  Returns FALSE if any STW location is erased
//-----------------------------------------------------------------------------
U8 get_stw_ee(void)
{
	U32	ii;			// temps
	U8	rtn = TRUE;

	ii = eerd(SPEED_EEADDR);
	if(ii != 0xffffffff){
		dit_time_reg = (U16)(ii >> 16);
		ditie_time_reg = (U16)(ii);
		dah_time_reg = dit_time_reg * 3;
		set_ramp_rate();
	}else{
		rtn = FALSE;
	}
	ii = eerd(TONE_EEADDR);
	if(ii != 0xffffffff){
		tone_reg = (U16)ii;
	}else{
		rtn = FALSE;
	}
	ii = eerd(WEIGHT_EEADDR);
	if(ii != 0xffffffff){
		weight_reg = (U16)ii;
	}else{
		rtn = FALSE;
	}
	return rtn;
}

//-----------------------------------------------------------------------------
// get_poweron_lock_strap() returns the power-on lock mode strap bit status
//	TRUE = strap open (power up = locked), FALSE = strap to GND (power on unlocked)
//-----------------------------------------------------------------------------
U8 get_poweron_lock_strap(void)
{

	return GPIO_PORTD_AHB_DATA_R & PWR_ON_LOCK;
}

//-----------------------------------------------------------------------------
// get_paddle_mode_strap() returns the paddle-mode strap bit status
//	TRUE = strap open (paddle/keyer mode), FALSE = strap GND (straight-key mode)
//-----------------------------------------------------------------------------
U8 get_paddle_mode_strap(void)
{
	U8	rtn;

	switch(paddle_force_mode){
	default:
	case FORCE_OFF:
		rtn = GPIO_PORTD_AHB_DATA_R & PADL_KEY;
		break;

	case FORCE_PDL:
		rtn = PADL_KEY;
		break;

	case FORCE_SKS:
		rtn = 0;
		break;
	}
	return rtn;
}

//-----------------------------------------------------------------------------
// paddle_force_set() writes value to the SK/PDL override register
//-----------------------------------------------------------------------------
void paddle_force_set(U8 reg)
{
	paddle_force_mode = reg;
	return;
}

//-----------------------------------------------------------------------------
// paddle_force_read() reads value from the SK/PDL override register
//-----------------------------------------------------------------------------
U8 paddle_force_read(void)
{

	return paddle_force_mode;
}

//-----------------------------------------------------------------------------
// get_stw_lock_strap() returns the stw lock strap bit status
//	TRUE = strap installed (locked), FALSE = no strap (pots control)
//-----------------------------------------------------------------------------
U8 get_stw_lock_strap(void)
{
	return (~GPIO_PORTA_DATA_R) & STW_LOCK_N;
}

//-----------------------------------------------------------------------------
// set_ramp_rate() sets the tone-shaping ramp rate based on dit rate
//-----------------------------------------------------------------------------
void set_ramp_rate(void)
{

	ramp_rate_reg = RAMP_RATE1;
	ramp_rateb_reg = RAMP_RATE1;
	return;
}

//-----------------------------------------------------------------------------
// set_weight() sets the Morse code weight from the user adjustment (ADC pot)
//	This value is added the inter-element (DIT) space as a signed value.
//	expected ADC value is 0-511.  The adjustment range is 0-50%.
//	Centered at 256, this gives +/- 25%
//-----------------------------------------------------------------------------
void set_weight(U16 weightadc)
{
	U16		w;			// weight temp
	U8		neg;		// sign temp

	if(weightadc >= 256){											// convert 0-511 to unsigned values using a center at 256 and set the sign
		w = weightadc - 256;										// positive 0 - 25%
		neg = 0;
	}else{
		w = 256 - weightadc;										// negative 1 - 25%
		neg = 1;
	}
	w = (U16)(((U32)w * (U32)dit_time_reg) / 1000L);				// calculate abs value of weight_reg
	if(neg){
		weight_reg = (0xffff - w) + 1;								// negative value
	}else{
		weight_reg = w;												// positive value
	}
	ditie_time_reg = dit_time_reg + BASE_WEIGHT + weight_reg;		// apply to intra-element delay
	return;
}

//-----------------------------------------------------------------------------
// set_iambic_mode() sets the iambic mode based on strap bit status
//-----------------------------------------------------------------------------
char set_iambic_mode(void)
{
	if(GPIO_PORTB_DATA_R & IAMBIC_BSEL){
		keyer_mode = 'B';
	}else{
		keyer_mode = 'A';
	}
	return keyer_mode;
}

//-----------------------------------------------------------------------------
// get_weight_strap() returns the Morse code weight strap bit status
//-----------------------------------------------------------------------------
U8 get_weight_strap(void)
{
	return GPIO_PORTD_AHB_DATA_R & WEIGHT_ADJ_N;
}

//-----------------------------------------------------------------------------
// get_pgm_keypad_enable() returns the pgm keypad strap bit status
//-----------------------------------------------------------------------------
U8 get_pgm_keypad_enable(void)
{
	return (~GPIO_PORTD_AHB_DATA_R & KEYPAD_PGM_N);
}

//-----------------------------------------------------------------------------
// get_ctrlz_strap() returns the CTRL-Z enable strap bit status
//	TRUE = strap installed (enabled), FALSE = no strap (no CTRL-Z)
//-----------------------------------------------------------------------------
U8 get_ctrlz_strap(void)
{
	return (~GPIO_PORTF_DATA_R) & CTRLZ_EN_N;
}

//-----------------------------------------------------------------------------
// put_cw_text() sends Morse code msg corresponding to txtchr
//-----------------------------------------------------------------------------
void put_cw_text(char txtchr)
{
	U8	i;			// temps
	U8	j;

	put_cw(' ');							// space out the response
	if(txtchr < MRSE_DEL){
		put_cw(txtchr);
	}else{
		if(txtchr < (LAST_KEY + 1)){
			i = txtchr - MRSE_DEL;
			for(j=0; j<CWTXT_LEN; j++){
				put_cw(cw_text_msg[i][j]);
			}
		}
	}
	return;
}

//-----------------------------------------------------------------------------
// swap_paddle() swaps the sense of the dit and dah port (reverses the paddles).
//	returns the swapped state (1 = swapped, 0 = not swapped)
//-----------------------------------------------------------------------------
U8 swap_paddle(void)
{
	U8	i = 0;		// temp

	if(dit_port == NDIT){			// swap dit and dah
		dit_port = NDAH;
		dah_port = NDIT;
	}else{							// set normal orientation
		dit_port = NDIT;
		dah_port = NDAH;
	}
	if(dit_port == NDAH){
		i = 1;						// report current state
	}
	return i;
}

//-----------------------------------------------------------------------------
// lookup_elem() does a table lookup of ascii to identify the character
//	index.  this index then produces an elem/len data from the elem/len LUTs.
//	Used for text->Morse output.
//-----------------------------------------------------------------------------
U8 lookup_elem(char c)
{
	U8		i = 0;					// LUT index, start at begining
	U8		s = sizeof_len_map();	// s = size of map array

	// test if elem and len match the table at index "i".  Stop if index
	//	is past end of LUT
	while((cw_text_map[i] != c) && (i < s)){
		i++;
	}
	if(i == s){
		i = 0xff;					// if end of table, return no match
	}
	return i;
}

//-----------------------------------------------------------------------------
// put_cw() outputs Morse code for the character in c.
//-----------------------------------------------------------------------------
void put_cw(char c)
{
	U8		i;					// LUT index
	U8		j;					// temps
	U16		jj;

	i = lookup_elem(c);
	while(sendingb);							// wait for current letter to finish
	if(i != 0xff){
		if((c == ' ') || (c == CWSPC)){
			trapb_word = 0;						// set up to send a word space
			trapb_count = 0;
			initialb = 0x02;
		}else{
			trapb_count = cw_len_map[i];		// update element count
			trapb_word = 0;
			jj = cw_elem_map[i];				// shift element into trap reg
			for(j=0;j<trapb_count;j++){			// bit reverse LUT element pattern
				trapb_word <<= 1;
				if(jj & 0x01) trapb_word |= 0x01;
				jj >>= 1;
			}
			initialb = 1;
		}
		// start B timer Morse output
		TIMER3_IMR_R = (TIMER3_IMR_R & TIMER3_IMR_BMASK) | TIMER_IMR_CBEIM;
		while(!sendingb);						// wait for current letter to start
	}
}

//*****************************************************************************
//                             MORSE INPUT Fns
//*****************************************************************************

//-----------------------------------------------------------------------------
// decode_elem() does a table lookup of element/len to identify the ASCII LUT
//	character index.  this index then produces an ASCII character from the
//	text LUT.  Null is returned for any unrecognized character.
//-----------------------------------------------------------------------------
char decode_elem(U16 elem, U8 len)
{
	U8		i = 0;					// LUT index, start at begining
	U8		s = sizeof_len_map();	// s = size of map array
	char	c;						// return temp

	// test if elem and len match the table at index "i".  Stop if index
	//	is past end of LUT
	while(((cw_elem_map[i] != elem) || (cw_len_map[i] != len)) && (i < s)){
		i++;
	}
	if(i == s){
		c = '\0';					// if end of table, return null (no match)
	}else{
		c = cw_text_map[i];			// else, return character from text LUT
	}
	return c;
}

//-----------------------------------------------------------------------------
// get_cw_asc() converts element/len data in capture buffer to ASCII and
//	returns as a single char.  If buffer is empty, return Null.
//-----------------------------------------------------------------------------
char get_cw_asc(void)
{
	char	c = '\0';		// return temp (default to empty buffer)
	U8		e = cwelem_idx;
	U8		i;				// temp index
	U8		s;				// start of buf index
	U8		j;				// temp
	U8		k;
	U8		l;
	U16		ee;				// temp elements
	U32		aa;				// temp average space
	U32		ii;				// temp max space
	U32		jj;				// temp U32
	U32		mm;				// min mark
	U32		xx;				// max mark
	U32		di;				// ave dit mark
	U32		da;				// ave dah mark

	//
	// If straight key, use a tiered algorigthm to determine the dit
	//	timing to use for the current character.
	//	* First, calc the max space (ii), ave space (aa), min space (ms), max mark (xx), min mark (mm) and ave dit mark (am).
	//	* if one element, use thresh = dit_ave * 2
	//	* else, if max_mark / min_mark >= 2, use thresh = (xx + mm) / 2 (update dit_ave)
	//	* 		else, if xx/ii >= 2, use ave_space thresh = (xx + ms)	 	{THIS MEANS WE HAVE ALL DAHS, because xx/mm < 2}
	//				  else, use thresh = aa * 2								{THIS MEANS WE HAVE ALL DITS}
	//
	if(!paddle_key_mode){							// THIS IS THE STRAIGH-KEY BRANCH ****************************************************
		if(suppress_s && aspace){											// if supress and autospace,
			aspace = FALSE;
			suppress_s = 0;													// clear suppress_s if a char is complete
	  		letter_complete = FALSE;
	  		cwelem_idx = 0;												// reset element index
		}else{
			if(aspace){														// else, if autospace set beep
		  		cw_stat |= CW_STB;											// enable beep
		  		cwelem_idx = 0;												// reset element index
		  		letter_complete = FALSE;
		  		c = ' ';
				aspace = FALSE;
				suppress_s = 0;												// clear suppress_s if a char is complete
			}else{
				if((e < CW_BUFF_END) && (e > 0)){							// make sure end of data index is valid
#ifdef DEBUG_CW
					UARTprintf("\nSK elem debug:\n");						// debug print to UART0
#endif
					s = 0;
					i = 0;
					// calc ave/min/max SPACE, and display captured timing buffer *****************************************************************************************
					j = 0;
					ii = 0;													// init max space
					aa = 0;													// init ave space
					mm = 0xffffffff;										// init min mark
					xx = 0;													// init max mark
					do{
						if(!(cw_timing_buf[i] & KEY_HIBIT)){				// is space: find max space
							if(i != 0){										// (skip the first array entry, a space here is an error)
								if(cw_timing_buf[i] > ii){
									ii = cw_timing_buf[i];
								}
								aa += cw_timing_buf[i];						// sum for ave
								j += 1;										// count them for the average calc
#ifdef DEBUG_CW
								UARTprintf("[%u] ", cw_timing_buf[i]);		// debug print space timing to UART0
#endif
							}else{
								s = 1;
							}
						}else{												// is mark
							jj = cw_timing_buf[i] & KEY_HIBIT_M;			// find min/max mark
#ifdef DEBUG_CW
							UARTprintf("%u ", jj);							// debug print mark timing to UART0
#endif
							if(jj < mm){									// min
								mm = jj;
							}
							if(jj > xx){									// max
								xx = jj;
							}
						}
					}while(++i < e);
					if((s == 1) && (e == 2)){
						e = 1;
					}
					// sum timings **************************************************************************************************************
					//	if di == 0 after this step, then da could be dit or dah
					i = s;
					k = 0;
					l = 0;
					di = 0;
					da = 0;
					do{
						if(cw_timing_buf[i] & KEY_HIBIT){
							jj = cw_timing_buf[i] & KEY_HIBIT_M;			// ave dit mark
							if((xx / jj) >= 2){
								di += jj;									// it was a dit
								k += 1;
							}else{
								da += jj;									// it was a dah, OR all of the elements are the same
								l += 1;
							}
						}
					}while(++i < e);
					if(aa != 0){											// Don't update if there is nothing to update (duh)
						aa /= j;											// calc average
						spc_ave = (spc_ave + aa) / 2;						// update running space average
					}
					if(di != 0){
						di /= k;
						dit_ave = (dit_ave + di) / 2;						// update running dit
					}
					if(da != 0){
						da /= l;
						dah_ave = (dah_ave + da) / 2;						// update running dah
					}
					// aa = space ave
					// di = dit-mark ave
					// da = dah-mark ave
					// ii = max space
					// mm = min mark
					// xx = max mark
					if((xx < DAH_SK_MAX) && (mm > DIT_SK_MIN)){				// at least 2 elements...see if mark timings are within valid range
						if(e > 1){
							if(di == 0){
								ii = 2 * dit_ave;							// chr is all dits or all dahs...use the historical dit_ave to calc timing threshold
							}else{
								ii = (di + da) / 2;							// is mixed mode, with dits and dahs: use an average of the dit/dah averages to set the threshold
							}
						}else{
							ii = dit_ave * 2;								// only 1 element: use running (old) averaged dit-mark to set the timing threshold
						}
					}else{
#ifdef DEBUG_CW
					UARTprintf("TIM-ERR!\n");				// debug print to UART0
#endif
						return c;											// if min or max invalid, abort (c = null at this point)
					}
					// ii now should equal the timing value ***********************************************************************************
					if(xx > (DAH_SK_MAX << 1)){								// uber-long keydown resets dit average back to 10WPM value
						dit_ave = DIT_SK;									// reset dit_ave to power-on default (10WPM)
						dah_ave = DIT_SK * 3;								// reset dit_ave to power-on default (10WPM)
						spc_ave = DIT_SK;									// reset dit_ave to power-on default (10WPM)
#ifdef DEBUG_CW
						UARTprintf("dit ave reset\n");						// debug print to UART0
#endif
						ii = dit_ave * 2;									// reset threshold to default
					}
#ifdef DEBUG_CW
					UARTprintf("\n");
					UARTprintf("bufsz: %u \n", e);							// debug print to UART0
					UARTprintf("DITave: %u \n", dit_ave);					// debug print to UART0
					UARTprintf("DAHave: %u \n", dah_ave);					// debug print to UART0
					UARTprintf("SPCave: %u \n", spc_ave);					// debug print to UART0
					UARTprintf("min: %u \n", mm);							// debug print to UART0
					UARTprintf("max: %u \n", xx);							// debug print to UART0
#endif

					if(ii < DIT_MAX){										// if speed too fast, bracket the letter/word space regs
						letter_space = LETTER_MAX;							// set letter & word registers (straight-key mode)
						word_space = WORD_MAX;
					}else{
						if(ii > DIT_MIN){									// same for if speed too slow
							letter_space = LETTER_MIN;						// set letter & word registers (straight-key mode)
							word_space = WORD_MIN;
						}else{												// Goldilocks zone, use calculated dit value to set spaces
							letter_space = LETTER_SK * ii;					// set letter & word registers (straight-key mode)
							word_space = WORD_SK * ii;
						}
					}
#ifdef DEBUG_CW
					UARTprintf("thresh: %u \n", ii);						// debug print to UART0
#endif
					// init loop variables
					i = s;													// array index
					ee = 0;													// element bitmap register
					j = 0;													// character length (in voiced elements)
					do{														// loop to build element word and length
						if(cw_timing_buf[i] & KEY_HIBIT){
							ee <<= 1;										// shift element word (default bit state is DIT)
							j += 1;
							if((cw_timing_buf[i] & KEY_HIBIT_M) >= ii){
								ee |= 0x01;									// is a DAH (else is a DIT)
							}
						}
					}while(++i < e);
					if((ee == CWSTT_E) && (j == CWSTT_L)){					// test for toggle autospace prosign
						if(cw_stat & CW_ST) cw_stat &= ~CW_ST;				// this if-else does the toggle
						else cw_stat |= CW_ST;
						cw_stat |= CW_STC;									// set toggle encountered flag
						c = CWSTT;
						suppress_s = TRUE;									// set suppress_s to prevent initial space
					}else{
				  		c = decode_elem(ee,j);								// decode element word and length
						suppress_s = 0;											// clear suppress_s if a char is complete
					}
			  		cwelem_idx = 0;											// now that char is decoded, re-init index and complete flag
			  		letter_complete = FALSE;
				}
				if(e >= CW_BUFF_END){										// if index is invalid, restart
			  		cwelem_idx = 0;											// init index & clear flags (return char is null by default)
			  		letter_complete = FALSE;
					suppress_s = 0;
				}
			}
		}
	}else{												// THIS IS THE IAMBIC PADDLE BRANCH ****************************************************
		if(cw_tail != cw_head){													// if buffer not empty,
			if(suppress_s && (cw_elem_buf[cw_tail] == SPACE_ELEM));				// if supress and autospace, do nothing
			else{
				if(cw_elem_buf[cw_tail] == SPACE_ELEM){							// else, if autospace set beep
			  		cw_stat |= CW_STB;		// enable beep
				}
				if(!(suppress_s && (cw_elem_buf[cw_tail] == SPACE_ELEM))){		//		 and process character
			  		c = decode_elem(cw_elem_buf[cw_tail],cw_len_buf[cw_tail]);	// 		 get elem/len and decode
				}
			}
			suppress_s = 0;														// suppress_s only valid for one character
			cw_tail += 1;														// advance tail
			if(cw_tail >= CW_BUFF_END){											// process tail wrap-around
				cw_tail = 0;
			}
		}
	}
	return c;																	// return decoded character
}

//-----------------------------------------------------------------------------
// getchar_cw() gets char from decoded text buffer.  Returns Null if buffer is
//	empty.
//-----------------------------------------------------------------------------
char getchar_cw(void)
{
	char	c = '\0';			// return char

	if(cwt_tail != cwt_head){				// if buffer not empty,
		c = cw_text_buf[cwt_tail++];		// get next char
		if(cwt_tail >= CWT_BUFF_END){		// process tail wrap-around
			cwt_tail = 0;
		}
	}
	return c;								// return char
}

//-----------------------------------------------------------------------------
// gotchar_cw() returns true if CW buffer has data.  Does not alter buffer.
//-----------------------------------------------------------------------------
U8 gotchar_cw(void)
{
	U8	rtn = FALSE;

	if(cwt_tail != cwt_head){				// if buffer not empty,
		rtn = TRUE;
	}
	return rtn;								// return char
}

//-----------------------------------------------------------------------------
// get_cwstat() returns/clears CW status
//-----------------------------------------------------------------------------
U8 get_cwstat(void)
{
	U8	i;			// temp

	cw_astat = CW_RS;
	if(TIMER3_IMR_R & TIMER_IMR_CAEIM){		// if timer3A ISR running, use status handshake
		while(cw_astat == CW_RS);
		i = cw_astat;
	}else{									// otherwise, access status directly
		i = cw_stat;
//		cw_stat &= CW_ST;					// keep the space_time flag
	}
	return i;									// return status
}

//-----------------------------------------------------------------------------
// get_cwdr() returns data ready status
//	if head != tail, DR is true, else false
//-----------------------------------------------------------------------------
U8 get_cwdr(void)
{
	U8	rtn;			// temp

	if(!paddle_key_mode){						// straight key mode (dit only)
		if(letter_complete){
			rtn = TRUE;
		}else{
			rtn = FALSE;
		}
	}else{
		if(cw_head == cw_tail){					// head==tail, no data
			rtn = FALSE;
		}else{
			rtn = TRUE;						// head!=tail, data ready
		}
	}
	return rtn;
}

//=============================================================================
// DIT/DAH isr fn
// This function is triggered by a DIT or DAH edge (PP4 & PP5, GPIO edge interrupt)
// The DIT/DAH isr triggers the debounce timer to filter closure noise (processed
//	in the timer3A isr).  If not activated, this ISR also enables the timer3A isr
//	and passes an initialization signal to that isr.
//=============================================================================

void didah_isr(void){

	if(!paddle_key_mode){										// straight key mode (dit only)
//		initial = GPIO_PORTB_RIS_R & NDIT;						// capture first edge
		initial = NDIT;											// only one edge to choose from...
		GPIO_PORTB_ICR_R = NDIT;								// clear edge detect flags
		GPIO_PORTB_IM_R = 0;									// mask GPIO isr
		gpiob_mem = 0;											// clear memory (used for falling edge detection)
		// timer 3A ISR enable
		TIMER3_IMR_R = (TIMER3_IMR_R & TIMER3_IMR_AMASK) | TIMER_IMR_CAEIM;
	}else{														// paddle mode
		initial = GPIO_PORTB_RIS_R & (dah_port | dit_port);		// capture first edge
		GPIO_PORTB_ICR_R = (dah_port | dit_port);				// clear edge detect flags
		GPIO_PORTB_IM_R = 0;									// mask GPIO isr
		// timer 3A ISR enable
		TIMER3_IMR_R = (TIMER3_IMR_R & TIMER3_IMR_AMASK) | TIMER_IMR_CAEIM;
	}
	return;
}

//=============================================================================
// timer 3A isr fn
// This is the DDS sine-tone generator & paddle input interrupt.  This produces the
//	CW side-tone for operator feedback and captures paddle state for Morse input
//	(feeds the element capture buffer for later decoding).
//	Because it is periodic, this isr also processes the debounce and tone duration timers
//	The element capture (trap) function is handled when a tone-off event occurs.
//
// IAMBIC OPERATION
//	Morse code is comprised of elements.  The basic (smallest) time unit is that of the "dit".
//	A "dah" is 3 times as long as a dit.  Each "on" element (dit or dah) within a character are
//	separated by a dit space.  Characters are separated by 3 dits, and words are
//	separated by at least 7 dits.
//
//	Two separate paddles are used to signal iambic keying patterns.  One is for "dit"s,
//	the other is for "dah"s.  When a paddle is pressed, the corresponding element is produced.
//	As long as that paddle remains pressed, the element is repeated.  If BOTH paddles are
//	pressed, the elements alternate between dit and dah.  When starting a character, the first
//	element sent corresponds to the first paddle pressed.
//
//	TYPE A vs TYPE B
//	In type A iambic keyers, the paddle "squeeze" mode is detected at the start of the element,
//	and the new element is the opposite of the previous one.  If there are no paddles pressed at
//	the start of element, then no further elements are sent.
//
//	In type B iambic keyers, if both paddles were pressed during an element, but released
//	prior to the end of the element, an additional element is sent that is opposite the last.
//	Otherwise, the paddle state is latched at the start of the element.
//
//	The GPIO ISR traps the initial paddle press and starts the timer3A ISR which processes the
//	rest of the character(s) up to a word space.  If a word space is detected, the timer3A ISR
//	is disabled, a space is loaded into the trap buffers, and the trap registers are reset.
//
//	Once timer3A is running, it handles the timers, iambic state machine, and DDS tone generation.
//	The state machine is implemented unconventionally.  The states are roughly INITIAL,
//	TONEON (element), and TONEOFF (inter-element or intra-word space).  During TONEON, the
//	DIT/DAH states are latched if active, or ignored if not.  This is the elem_lat.
//
// After a word space has elapsed, the timer3A ISR turns itself off, and turns on the GPIO
//	edge detect ISR.  The PWM continues to run at its previous setting, but the timer ISR
//	stops.  This reduces processor overhead with no loss of response to unscheduled operator
//	input.  Allowing the PWM to continue to operate also prevents "clicks" or "pops" from
//	occurring when the DDS/CW system is started/stopped.
//
// STRAIGHT-KEY OPERATION
//
// This ISR is segregated into two sections, one for straight-key and one for iambic.  The two
//	sections are independent with separate "return" exit points.  This results in duplicated code,
//	but represents the most time-efficient structure for code execution and also serves to keep the
//	code functions completely segregated which minimizes interactions between the two modes.
//
// The general structure of the straight-key section follows that of the iambic section.  However,
//	the buffer used stores element timing rather than character element maps.  timing values encode
//	the key-down status in the high bit.  The timing values are then processed outside the interrupt
//	to create an element map and element count.  These values are then fed into the same code stream
//	used for the iambic mode to process characters and feed them up to the USB Keyboard app.
//
// The DDS function is built on previous DDS efforts for the HC11 (asm) and SiLabs processors (C).
//	The system uses a quarter-sine table (0 to 90 degrees) with 12 bits of resolution.  The table
//	is signed, so the values vary from 0 at 0deg to +MAX at 90deg.  The DDS algorithm maps this table
//	into the other quadrants before being applied to the PWMDAC.  An Fsamp of 10000Hz is used and
//	provisions are made for an rampup (attack) and rampdn (decay) feature to provide for smooth
//	tone keying.
//
//	The PWMDAC as implemented here has a total dynamic range of about 5000 timing units.  However,
//	due to interrupt latencies, only about 6400 units are useable.  To provide reasonable margin,
//	4096 units are used (12 bits), centered at 2500.
//
//	The tone frequency is determined by the following formula:
//		TONE_f (Hz) = (f * (RADIX * N_SIN) / FSAMP)
//	RADIX is the number of right bit-shifts to align the phase accumulator decimal point just to
//	the right of the lsb of the result.  N_SIN is the length of the lookup table if it were expanded
//	to cover a full 360deg sinewave.  FSAMP is 10000.  These defines are present in the morse.h header
//	file and may be adjusted as appropriate if the DDS system is modified or ported.  This equation
//	can also be moved to a run-time function if user adjustment is desired.  The DDS tone output
//	is disabled by simply setting delph1 to 0.
//
//	The DAC value is recovered from the PWM output using a low-pass filter.  The PWM output is
//	RC filtered to reduce the PWM clock component (5.6K and 4700pF give an Fc of about 6KHz) capacitively
//	coupled to the analog processing chain and a LPF applied.  Here, a TLC14 is used to provide an
//	8-pole LPF.  The filtered output is then provided to the audio processing or PA system.
//
//=============================================================================

void Timer3A_ISR(void){
	U8			i;				// temp regs
	U8			state_trap;
	U16			index;
	U16			pac;			// phase accumulator temp
	S16			sign;			// pwm dac temps
	S16			pdac1;

	TIMER3_ICR_R = TIMER3_MIS_R & TIMER_MIS_AMASK;				// clear timer3A interrupt flag
	// if straight key mode, process straight key
	if(!paddle_key_mode){
		if(initial){											// inital paddle capture after a word silence
			initial = 0;										// clear inital state
			key_timer = 0;
			pstate = P_KEYDN;
			rampup = RAMP_MAX;									// init attack (toneon)
			rampdn = 0;
			rampcyc = ramp_rate_reg; 							// set RAMP_RATE;
			delph1 = tone_reg;									// start tone
			gpiob_mem = NDIT;									// disarm first edge trap
			debounce_timer = DEBOUNCE_DLY;						// start debounce delay
		}
		if((!debounce_timer) || letter_complete){									// ignore GPIO status until debounce timer expires
			i = ~GPIO_PORTB_DATA_R & NDIT;						// capture paddle state (inverted, true = keydn, false = keyup)
			if(i != gpiob_mem){
				gpiob_mem = i;
				debounce_timer = DEBOUNCE_DLY;					// start debounce delay
				if(i){								// is a transit to mark
					if(cwelem_idx < CW_BUFF_END){				// keep index in range
						cw_timing_buf[cwelem_idx++] = key_timer;	// capture space timing value
					}
					key_timer = 0;
					rampup = RAMP_MAX;							// init attack (toneon)
					rampcyc = ramp_rate_reg; //RAMP_RATE;
					delph1 = tone_reg;							// start tone
					pstate = P_KEYDN;
				}else{								// is a transit to space
					if(cwelem_idx < CW_BUFF_END){				// keep index in range
						cw_timing_buf[cwelem_idx++] = key_timer | KEY_HIBIT;	// capture keydn timing value
					}
					key_timer = 0;
					rampdn = RAMP_MAX;							// set decay (to tone off)
					rampcyc = ramp_rate_reg; //RAMP_RATE;
					pstate = P_KEYUP;
				}
			}
		}
		if(delph1){
			// process DDS
			// process phase accumulator 1		tone 1
			phaccum1 += delph1;									// add delta for tone 1
			pac = (phaccum1 >> RSHIFT) & PHMASK;				// calc base index into sine table
			if(pac < 2048){										// if 1st quadrant, use table as-is
				index = pac;
				sign = 1;
			}
			if((pac > 2047) && (pac < 4096)){					// if 2nd quad, set index to offset from the
				index = (4095 - pac);							// top of the table (accesses 2nd 90 degree segment)
				sign = 1;
			}
			if((pac > 4095) && (pac < 6144)){					// if 3rd quad, set index to read table directly,
				index = pac - 4096;
				sign = -1;										// and set sign = negative (accesses 3rd 90 degree segment)
			}
			if(pac > 6143){										// if 4th quad, set index to offset from the
				index = (8191 - pac);							//		top of the table.
				sign = -1;										// and set sign = negative (accesses last 90 degree segment)
			}
			pdac1 = SINE[index];
			if(rampup){											// do attack shaping (before applying sign)
				pdac1 >>= rampup;
				if(--rampcyc == 0){
					rampup--;
					rampcyc = ramp_rate_reg; //RAMP_RATE;
				}
			}
			if(rampdn){											// do decay shaping (before applying sign)
				pdac1 >>= ((RAMP_MAX + 1) - rampdn);
				if(--rampcyc == 0){
					rampdn--;
					rampcyc = ramp_rate_reg; //RAMP_RATE;
					if(rampdn == 0){
						delph1 = 0;								// turn off tone
					}
				}
			}
			pdac1 *= sign;										// apply sign and
			pdac1 += DDS_MID_DAC;								// align signal "0" point with PWMDAC midrange
			TIMER3_TAMATCHR_R = pdac1;							// update PWMDAC
		}else{
			if((key_timer >= letter_space) && (pstate == P_KEYUP)){
				// capture key-up timer
				letter_complete = TRUE;
				pstate = P_KEYUP_LETTER;
			}
			if((key_timer > word_space) && (pstate == P_KEYUP_LETTER)){
				if((cw_stat & CW_ST)){
					aspace = TRUE;								// set an autospace
					letter_complete = TRUE;
				}
				TIMER3_IMR_R &= ~TIMER_IMR_CAEIM;				// disable Timer3A
				GPIO_PORTB_ICR_R = NDIT;						// pre-clear paddle edge capture flags
				GPIO_PORTB_IM_R = NDIT;							// un-mask GPIO edge isr
			}
		}
		if(cw_astat == CW_RS){									// perform status handshake if app status == request status
			cw_astat = cw_stat | CW_GS;							// move status + got staus flag to app status
			cw_stat &= CW_ST;									// clear status (keep space_time)
		}
		key_timer += 1;											// update key timer
		if(debounce_timer){
			debounce_timer -= 1;								// debounce timer is count-down-from-N-and-halt
		}
		return;													// exit interrupt
	}
	//*******************************************************************************************************************
	// Everything below this line is paddle-mode code
	//
	if(tone_timer != 0) tone_timer--;						// update tone timer
	if(initial){											// inital paddle capture after a word silence
		rampup = RAMP_MAX;									// init attack (toneon)
		rampcyc = ramp_rate_reg; //RAMP_RATE;
		delph1 = tone_reg;
		pstate = 0;
		if(initial & dit_port){
			tone_timer = dit_time_reg;						// set dit delay
			trap_bit = 0x00;								// trap bit = dit
		}else{
			tone_timer = dah_time_reg;						// set dah delay
			trap_bit = 0x01;								// trap bit = dah
		}
		initial = 0;
		debounce_timer = DEBOUNCE_DLY;						// start debounce delay
	}
	if(delph1){
		if(!debounce_timer){								// ignore GPIO status until debounce timer expires
			pstate |= ~GPIO_PORTB_DATA_R & (dah_port | dit_port);	// capture paddle state (inverted)
			GPIO_PORTB_ICR_R = (dah_port | dit_port);				// clear paddle edge capture flags
		}
		// process element timer
		if(tone_timer == 0){
			silence_count = 0;								// set up to process inter-element space
			rampdn = RAMP_MAX;								// set decay (to tone off)
			rampcyc = ramp_rate_reg; //RAMP_RATE;
			tone_timer = ditie_time_reg;					// reset timer to cover decay period
		}
		// process DDS
		// process phase accumulator 1		tone 1
		phaccum1 += delph1;									// add delta for tone 1
		pac = (phaccum1 >> RSHIFT) & PHMASK;				// calc base index into sine table
		if(pac < 2048){										// if 1st quadrant, use table as-is
			index = pac;
			sign = 1;
		}
		if((pac > 2047) && (pac < 4096)){					// if 2nd quad, set index to offset from the
			index = (4095 - pac);							// top of the table (accesses 2nd 90 degree segment)
			sign = 1;
		}
		if((pac > 4095) && (pac < 6144)){					// if 3rd quad, set index to read table directly,
			index = pac - 4096;
			sign = -1;										// and set sign = negative (accesses 3rd 90 degree segment)
		}
		if(pac > 6143){										// if 4th quad, set index to offset from the
			index = (8191 - pac);							//		top of the table.
			sign = -1;										// and set sign = negative (accesses last 90 degree segment)
		}
		pdac1 = SINE[index];
		if(rampup){											// do attack shaping (before applying sign)
			pdac1 >>= rampup;
			if(--rampcyc == 0){
				rampup--;
				rampcyc = ramp_rate_reg; //RAMP_RATE;
			}
		}
		if(rampdn){											// do decay shaping (before applying sign)
			pdac1 >>= ((RAMP_MAX + 1) - rampdn);
			if(--rampcyc == 0){
				rampdn--;
				rampcyc = ramp_rate_reg; //RAMP_RATE;
				if(rampdn == 0){
					delph1 = 0;								// turn off tone
					trap_word <<= 1;						// shift element into trap reg
					trap_word |= trap_bit & 0x01;
					if(trap_count < MAX_SPACE_COUNT) trap_count++;	// update element count (if valid)
					tone_timer = ditie_time_reg;					// set dit-silence delay
					GPIO_PORTB_ICR_R = (dah_port | dit_port);		// pre-clear paddle edge capture flags
				}
			}
		}
		pdac1 *= sign;										// apply sign and
		pdac1 += DDS_MID_DAC;								// align signal "0" point with PWMDAC midrange
		TIMER3_TAMATCHR_R = pdac1;							// update PWMDAC
	}else{
		// process CW paddles
		// if in between letter and word space, trap paddle press instantly
		if(silence_count >= LETTER_SPACE){
			initial = GPIO_PORTB_RIS_R & (dah_port | dit_port);		// capture paddle edge state (inverted)
			if(initial){
				GPIO_PORTB_ICR_R = (dah_port | dit_port);	// clear paddle edge capture flags
				tone_timer = 1;								// ensure that we don't get caught in a timing race
			}
		}
		if(tone_timer == 0){
			if(keyer_mode == 'A'){
				// for A mode, capture the current paddle state
				pstate = ~GPIO_PORTB_DATA_R & (dah_port | dit_port);
			}else{
				// for B mode, if previous state was dit&dah, carry this over
				if(pstate != (dah_port | dit_port)){
					// otherwise, capture the current paddle state
					pstate = ~GPIO_PORTB_DATA_R & (dah_port | dit_port);
					// include any paddle "flashes" -- a paddle press (usu. dit) in the middle of an opposite element
					pstate |= GPIO_PORTB_RIS_R & (dah_port | dit_port);
				}
			}
			GPIO_PORTB_ICR_R = (dah_port | dit_port);		// clear edge detects
			state_trap = 0;
			if( pstate == (dit_port|dah_port)){				// both paddles, flip to the opposite element
				state_trap = 1;
				trap_bit ^= 0x01;							// invert trap bit
				rampup = RAMP_MAX;							// init attack (toneon)
				rampcyc = ramp_rate_reg; //RAMP_RATE;
				delph1 = tone_reg;
				if(trap_bit){
					tone_timer = dah_time_reg;
				}else{
					tone_timer = dit_time_reg;
				}
			}else{
				if(pstate == dit_port){						// init for DIT
					state_trap = 1;
					rampup = RAMP_MAX;						// init attack (toneon)
					rampcyc = ramp_rate_reg; //RAMP_RATE;
					delph1 = tone_reg;
					trap_bit = 0x00;						// trap bit = dit
					tone_timer = dit_time_reg;
				}else{
					if(pstate == dah_port){					// init for DAH
						state_trap = 1;
						rampup = RAMP_MAX;					// init attack (toneon)
						rampcyc = ramp_rate_reg; //RAMP_RATE;
						delph1 = tone_reg;
						trap_bit = 0x01;					// trap bit = dah
						tone_timer = dah_time_reg;
					}
				}
			}
			if(!state_trap){
				// no paddles active, start counting dit times until letter and/or word space is reached
				tone_timer = dit_time_reg;
				if(silence_count++ >= WORD_SPACE){
					// put space into buffer
					if((cw_stat & CW_ST)){
						cw_elem_buf[cw_head] = SPACE_ELEM;	// feed the buffer
						cw_len_buf[cw_head++] = SPACE_COUNT;
						if(cw_head >= CW_BUFF_END){
							cw_head = 0;					// wrap buffer ptr
						}
						if(cw_head == cw_tail){
							cw_stat |= CW_OR;				// buffer overrun, set error
						}
					}
					// set buffer 70% full flag
					if(cw_head >= cw_tail) i =  cw_head - cw_tail;
					else i = CW_BUFF_END - (cw_tail - cw_head);
					if(i > CW_BUFF_70) cw_stat |= CW_FF;
					cw_stat |= CW_WF;						// set word delimit flag
					// turn off t3a isr (PWM continues to run)
					TIMER3_IMR_R &= ~TIMER_IMR_CAEIM;
					// enable GPIO interrupt
					GPIO_PORTB_IM_R = (dah_port | dit_port);	// un-mask GPIO isr
				}
				if(silence_count == LETTER_SPACE){
					if(trap_count < MAX_SPACE_COUNT){		// if element count is valid, (if invalid, character is discarded)
						if((trap_word == CWSTT_E) && (trap_count == CWSTT_L)){
							if(cw_stat & CW_ST) cw_stat &= ~CW_ST;
							else cw_stat |= CW_ST;
							cw_stat |= CW_STC;				// set toggle encountered flag
						}else{
							cw_elem_buf[cw_head] = trap_word; // feed the buffer
							cw_len_buf[cw_head++] = trap_count;
							if(cw_head >= CW_BUFF_END){
								cw_head = 0;				// wrap buffer ptr
							}
							if(cw_head == cw_tail){
								cw_stat |= CW_OR;			// buffer overrun, set error
							}
							if(cw_head > cw_tail) i =  cw_head - cw_tail;
							else i =  CW_BUFF_END - (cw_tail - cw_head);
							if(i > CW_BUFF_70){
								cw_stat |= CW_FF;
							}
						}
					}
					// clear trap regs
					trap_word = 0;							// CW element capture reg
					trap_count = 0;							// CW element counter
					trap_bit = 0;							// CW element reg
				}
			}
			pstate = 0;										// clear edge detects
		}
	}
	if(cw_astat == CW_RS){									// perform status handshake if app status == request status
		cw_astat = cw_stat | CW_GS;							// move status + got staus flag to app status
		cw_stat &= CW_ST;									// clear status (keep space_time)
	}
	if(debounce_timer){
		debounce_timer -= 1;								// debounce timer is count-down-from-N-and-halt
	}
	return;
}

//=============================================================================
// timer3B_ISR() handles text->CW function.  Uses same LUTs as for CW keyboard
//
// A separate PWMDAC (TIMER3B) is used to provide a text->CW capability.  While this
//	simplifies the code (the two functions are simpler when examined alone and can run
//	concurrently) this comes at the expense of increased code size (not an issue at
//	the moment).
//
// This function includes a "beep" feature that allows the system level application
//	to initiate a signaling beep.  The main need is to signal word spaces during
//	CW entry.
//=============================================================================

void Timer3B_ISR(void){
	U16			index;
	U16			pac;
	S16			sign;
	S16			pdac;
	volatile U8 tengo;

	TIMER3_ICR_R = TIMER3_MIS_R & TIMER_MIS_BMASK;		// clear timer3B interrupt flag
	if(toneb_timer != 0) toneb_timer--;					// update tone timer
	if(initialb){
		sendingb = 1;
		if(initialb == 0x02){
			toneb_timer = WSPACEB_TIME;					// set word delay
			delph2 = 0;
		}else{
			if(initialb == 0x04){
				rampupb = RAMP_MAX;						// init attack (toneon)
				rampcycb = ramp_rateb_reg; //RAMP_RATE;
				delph2 = tone3_reg;
				toneb_timer = (DITB_TIME/2);			// set beep delay
				sendingb = 2;							// set beep mode
			}else{
				rampupb = RAMP_MAX;						// init attack (toneon)
				rampcycb = ramp_rateb_reg; //RAMP_RATE;
				delph2 = tone2_reg;
				if(trapb_word & 0x01){
					toneb_timer = DAHB_TIME;			// set dah delay
				}else{
					toneb_timer = DITB_TIME;			// set dit delay
				}
			}
		}
		initialb = 0;
	}
	if(delph2){											// process element timer
		if(toneb_timer == 0){
			rampdnb = RAMP_MAX;							// set decay (to tone off)
			rampcycb = ramp_rateb_reg; //RAMP_RATE;
			toneb_timer = DITB_TIME;					// reset timer to cover decay period
		}
		// process DDS - process phase accumulator 1, tone 1
		phaccum2 += delph2;								// add delta for tone 1
		pac = (phaccum2 >> RSHIFT) & PHMASK;			// calc base index into sine table
		if(pac < 2048){									// if 1st quadrant, use table as-is
			index = pac;
			sign = 1;
		}
		if((pac > 2047) && (pac < 4096)){				// if 2nd quad, set idx to offs fr the
			index = (4095 - pac);						// top of the table (accesses 2nd 90
			sign = 1;									// degree segment)
		}
		if((pac > 4095) && (pac < 6144)){				// if 3rd quad, set index to read
			index = pac - 4096;							// table directly, and set sign = neg
			sign = -1;									// (accesses 3rd 90 degree segment)
		}
		if(pac > 6143){									// if 4th quad, set index to offset
			index = (8191 - pac);						// from the top of the table.
			sign = -1;									// and set sign = negative
		}												// (accesses last 90 degree segment)
		if(index > 2047){
			tengo++;
		}
		pdac = SINE[index];
		if(rampupb){									// do attack shaping (first)
			pdac >>= rampupb;
			if(--rampcycb == 0){
				rampupb--;
				rampcycb = ramp_rateb_reg; //RAMP_RATE;
			}
		}
		if(rampdnb){									// do decay shaping (first)
			pdac >>= ((RAMP_MAX + 1) - rampdnb);
			if(--rampcycb == 0){
				rampdnb--;
				rampcycb = ramp_rateb_reg; //RAMP_RATE;
				if(rampdnb == 0){
					delph2 = 0;							// turn off tone
					toneb_timer = DITB_TIME;			// set dit delay
				}
			}
		}
		pdac *= sign;									// apply sign and
		pdac += DDS_MID_DAC;							// move signal to PWMDAC midrange
		TIMER3_TBMATCHR_R = pdac;						// update PWMDAC
	}else{
		if(sendingb == 0x02){							// stop tones
			TIMER3_IMR_R = (TIMER3_IMR_R & TIMER3_IMR_BMASK) & ~TIMER_IMR_CBEIM;
			sendingb = 0;
		}else{
			// process CW space
			if(toneb_timer == 0){
				if(trapb_count == 0){
					// disable isr
					TIMER3_IMR_R = (TIMER3_IMR_R & TIMER3_IMR_BMASK) & ~TIMER_IMR_CBEIM;
					sendingb = 0;
				}else{
					trapb_word >>= 1;					// shift element into trap reg
					trapb_count--;						// update element count
					if(trapb_count){
						rampupb = RAMP_MAX;				// init attack (toneon)
						rampcycb = ramp_rateb_reg; //RAMP_RATE;
						delph2 = tone2_reg;
						if(trapb_word & 0x01){
							toneb_timer = DAHB_TIME;	// set dah delay
						}else{
							toneb_timer = DITB_TIME;	// set dit delay
						}
					}else{
						toneb_timer = DAHB_TIME;		// set character delay
					}
				}
			}
		}
	}
	return;
}
