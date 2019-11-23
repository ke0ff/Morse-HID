/********************************************************************
 ************ COPYRIGHT (c) 2019 by KE0FF, Taylor, TX   *************
 *
 *  File name: morse.h
 *
 *  Module:    Control
 *
 *  Summary:   This is the morse keyboard function header file
 *
 *******************************************************************/

/********************************************************************
 *  File scope declarations revision history:
 *    03-05-19 jmh:  creation date
 *
 *******************************************************************/

//#include "init.h"
#define	CWEOM 0x19											// /SK (ASCII EM)
#define	CW_KN '('											// /KN (ASCII EOT)
#define	CW_BS 0x08											// backspace (ASCII BS)
#define	CW_CR 0x0d											// end of line (ASCII CR)
#define	CWTAB 0x09											// /TAB (ASCII TAB)
#define	CWESC 27											// /ESC (ASCII ESC)
#define	CWBEL 0x07											// beep (ASCII BEL)
#define	CWDEL 0x7f											// delete last word (ASCII DEL)
#define	CWSPC (' ' | 0x80)									// explicit space char (..--)
#define	CW_FACINT	0xfa									// process_CW return if fact init selected

#define	SPACE_ELEM	0xfa5f									// word space definition
#define	SPACE_COUNT	0xfe
#define	MAX_SPACE_COUNT	17									// max # elements per character + 1
#define	CW_BUFF_END		40									// CW buffer length
#define	CW_BUFF_70		(CW_BUFF_END * 70 / 100)			// CW 70% index
#define	CWT_BUFF_END	128									// CW text buffer length
#define	CWT_BUFF_70		(CWT_BUFF_END * 70 / 100)			// CW text 70% index
// cwstat reg bitmaps
#define	CW_OR		0x80									// CW buffer overrun error
#define	CW_FF		0x40									// CW buffer fifo (70%) full
#define	CW_WF		0x20									// word space boundary met
#define	CW_STC		0x10									// auto space toggle cmd encountered
#define	CW_ST		0x08									// CW auto space mode flag
#define	CW_GS		0x04									// CW got status
#define	CW_RS		0x02									// CW request status
#define	CW_STB		0x01									// CW auto space beep

#define	CWR_PROC_IDLE	0x00								// RX CW Process, idle state
#define	CWR_PROC_CHAR	0x01								// RX CW Process, char processing state
#define	CWS_PROC_IDLE	0x00								// Send CW Process, idle state
#define	CWS_PROC_BEEP	0x01								// Send CW Process, send BEEP

// pstate defines
#define	P_IDLE			0
#define	P_KEYUP			0x40
#define	P_KEYUP_LETTER	0x20
#define	P_KEYDN			NDIT
#define	KEY_HIBIT		0x80000000
#define	KEY_HIBIT_M		(~KEY_HIBIT)

// timing defines
#define FSAMP			10000								// DDS Sample rate
#define	DEBOUNCE_DLY	200									// 20ms of debounce
// DIT time formula: DIT(ms) = 1200 / WPM
#define	WPM_CONST		1200
#define	WPM				20									// need to move to real-time
#define	DIT_TIME		((FSAMP * WPM_CONST) / (1000 * WPM))
#define	DAH_TIME		(DIT_TIME * 3)
#define	WPM_SK_INIT		10									// inital straight key speed settings
#define	DIT_SK_MAX		((FSAMP * WPM_CONST) / (1000 * 3))				// = 4,000
#define	DIT_SK_MIN		((FSAMP * WPM_CONST) / (1000 * 55))				// = 218
#define	DAH_SK_MAX		(3L * (FSAMP * WPM_CONST) / (1000L * 3L))		// = 12,000 fsamp = 12 seconds
#define	DAH_SK_MIN		(3L * (FSAMP * WPM_CONST) / (1000L * 55L))		// = 654
#define	DIT_SK			((FSAMP * WPM_CONST) / (1000 * WPM_SK_INIT))
#define	DAH_SK			(DIT_SK * 2)
#define	LETTER_SK		(3)
#define	WORD_SK			(14)
#define	WPM_MIN_SK		(10)											// min WPM for spacing
#define	WPM_MAX_SK		(20)											// max WPM for spacing
#define	DIT_MIN			((FSAMP * WPM_CONST) / (1000 * WPM_MIN_SK))
#define	DIT_MAX			((FSAMP * WPM_CONST) / (1000 * WPM_MAX_SK))
#define	LETTER_MIN		(LETTER_SK * DIT_MIN)
#define	LETTER_MAX		(LETTER_SK * DIT_MAX)
#define	WORD_MIN		(WORD_SK * DIT_MIN)
#define	WORD_MAX		(WORD_SK * DIT_MAX)
// Aux tone output can have a different code speed
#define	DITB_TIME		((FSAMP * WPM_CONST) / (1000 * 25))
#define	DAHB_TIME		(DITB_TIME * 3)
#define	WSPACEB_TIME	(DITB_TIME * 7)
#define	DEBOUNCE_MS		5									// debounce in ms
#define	DEBOUNCE_TIME	((FSAMP * DEBOUNCE_MS) / 1000)		// debounce timer value
#define	LETTER_SPACE	2									// # dits to delimit a character
#define	WORD_SPACE		14									// # dits to delimit a character
#define	RAMP_MAX		6									// #bit shifts to do rise/fall, 6 binary div/2 ~~ 0
#define	RAMP_TIME		5									// ramp time (attack) (ms)
#define	RAMP_RATE1	((FSAMP * RAMP_TIME) / (RAMP_MAX * 1000))	// # fsamp cycles for each amplitude shift
#define	RAMP_RATE2	((FSAMP * RAMP_TIME/2) / (RAMP_MAX * 1000))	// # fsamp cycles for each amplitude shift
#define	RAMP_RATE3	((FSAMP * RAMP_TIME/5) / (RAMP_MAX * 1000))	// # fsamp cycles for each amplitude shift
#define	BASE_WEIGHT		((FSAMP * RAMP_TIME) / 1000)			// correction to account for rise/fall time
#define	DIT_TIME_15		((FSAMP * WPM_CONST) / (1000 * 15))
#define	DIT_TIME_30		((FSAMP * WPM_CONST) / (1000 * 30))
#define TIMER3_RELOAD (U16)(65536L - (SYSCLK / FSAMP))		// sets Fsamp rate for timer3
#define DDS_MID_DAC	(SYSCLK/(2 * TIMER3_FREQ * (TIMER3_PS + 1)))
#define	TIMER3_IMR_AMASK 0xffffffe0
#define	TIMER3_IMR_BMASK 0xfffff0ff
// DDS defines
#define	PHMASK 0x1fff										// phase mask (= N_SIN)
#define N_SIN 8192L											// number of effective slots in sin look-up table
#define RSHIFT 2											// bits to downshift phase accum to align RADIX
#define RADIX 4L											// = 2^RSHIFT
// fixed tone defines
#define TONE_400 (U16)(400L * (RADIX * N_SIN) / FSAMP)		// 400hz tone
#define TONE_600 (U16)(600L * (RADIX * N_SIN) / FSAMP)		// 700hz tone
#define TONE_700 (U16)(700L * (RADIX * N_SIN) / FSAMP)		// 700hz tone
#define TONE_800 (U16)(800L * (RADIX * N_SIN) / FSAMP)		// 700hz tone
#define TONE_1000 (U16)(1000L * (RADIX * N_SIN) / FSAMP)	// 1000hz tone
#define TONE_1400 (U16)(1400L * (RADIX * N_SIN) / FSAMP)	// 1000hz tone
#define DDSPWM_MIN	100											// minimum PWM compare value
#define DDSPWM_MID	(DDS_MID_DAC)								// mid-range PWM compare value
// SK/PDL defines
#define	FORCE_MSK	0x80
#define	FORCE_PDL	0x81
#define	FORCE_SKS	0x80
#define	FORCE_OFF	0x00
#define	FORCE_TGL	0x01

//-----------------------------------------------------------------------------
// Fn prototype declarations
//-----------------------------------------------------------------------------

char Process_CW(U8 cmd);
U8 morse_init(void);
void init_stw(void);

U16 set_tone(U16 toneadc);
void set_speed(U16 speedadc);
void set_weight(U16 weightadc);
void store_stw_ee(void);
U8 get_stw_ee(void);

U8 get_poweron_lock_strap(void);
U8 get_stw_lock_strap(void);
char set_iambic_mode(void);
U8 get_weight_strap(void);
U8 get_pgm_keypad_enable(void);
U8 get_ctrlz_strap(void);
U8 get_paddle_mode_strap(void);

void paddle_force_set(U8 reg);
U8 paddle_force_read(void);
U8 swap_paddle(void);
void put_cw_text(char txtchr);
U8 lookup_elem(char c);
void put_cw(char c);
char get_cw_asc(void);
char getchar_cw(void);
U8 gotchar_cw(void);
U8 get_cwstat(void);

void didah_isr(void);
void Timer3A_ISR(void);
void Timer3B_ISR(void);

//-----------------------------------------------------------------------------
// End of file
//-----------------------------------------------------------------------------
