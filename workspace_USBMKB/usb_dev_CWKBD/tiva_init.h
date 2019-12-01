/********************************************************************
 ************ COPYRIGHT (c) 2019 by ke0ff, Taylor, TX   *************
 *
 *  File name: tiva_init.h
 *
 *  Module:    Control
 *
 *  Summary:   defines and global declarations for tiva_init.c
 *
 *******************************************************************/

#include "typedef.h"

#ifndef TIVA_INIT_H_
#define TIVA_INIT_H_
#endif /* TIVA_INIT_H_ */

//-----------------------------------------------------------------------------
// defines
//-----------------------------------------------------------------------------

#define PORTF SYSCTL_RCGCGPIO_R5
#define PORTE SYSCTL_RCGCGPIO_R4
#define PORTD SYSCTL_RCGCGPIO_R3
#define PORTC SYSCTL_RCGCGPIO_R2
#define PORTB SYSCTL_RCGCGPIO_R1
#define PORTA SYSCTL_RCGCGPIO_R0

// tiva_init() ipl status flag bitmap
#define	IPL_UART0INIT	0x0001		// UART0 initialized
#define	IPL_UART1INIT	0x0002		// UART1 initialized
#define	IPL_PWM1INIT	0x0004		// PWM module 1 initialized
#define	IPL_ADCINIT		0x0008		// ADC initialized
#define	IPL_QEI0INIT	0x0010		// UART0 initialized
#define	IPL_QEI1INIT	0x0020		// UART0 initialized
#define	IPL_TIMER3INIT	0x0040		// Timer3 initialized
#define	IPL_TIMER1INIT	0x0080		// Timer1 initialized
#define	IPL_TIMER2INIT	0x0100		// Timer2 initialized
#define	IPL_PLLINIT		0x0200		// PLL initialized
// tiva_init() ipl error flag bitmap
#define	IPL_REGWERR		0x2000		// register bit wait error
#define	IPL_EEPERR		0x4000		// EEPROM configuration error
#define	IPL_HIBERR		0x8000		// HIB configuration error

// PWM defines
#define	PWM_FREQ		10000L		// this is the PWM frequency in Hz
#define	PWM_DIV			2			// bit pattern for the PWM_DIV field in SYSCTL_RCC
									// 0 = /2,  1 = /4,  2 = /8,  3 = /16,
									// 4 = /32, 5 = /64, 6 = /64, 7 = /64
#define	PWM_DIVSR		8L			// = 2^(PWM_DIV + 1)
#define	PWM_PERIOD		(SYSCLK / (PWM_DIVSR * PWM_FREQ)) 	// = 625  - the PWM "zero" point based on a 2000/8E6 PWM frequency
#define	PWM_MIN			(2L * PWM_PERIOD / 8L)				// = 156  - this is the max PWM counter value for the LED output
#define	PWM_OFF			(PWM_PERIOD + 1)

//-----------------------------------------------------------------------------
// Fn prototypes
//-----------------------------------------------------------------------------

U16 proc_init(void);

//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------