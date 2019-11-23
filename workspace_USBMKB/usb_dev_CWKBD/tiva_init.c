/********************************************************************
 ************ COPYRIGHT (c) 2019 by ke0ff, Taylor, TX   *************
 *
 *  File name: tiva_init.c
 *
 *  Module:    Control
 *
 *  Summary:
 *  Tiva processor init functions
 *
 *******************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include "inc/tm4c123gh6pm.h"
#include "typedef.h"
#include "init.h"						// App-specific SFR Definitions
#include "tiva_init.h"
#include "eeprom.h"
#include "adc.h"
#include "version.h"
#include "morse.h"

//=============================================================================
// local registers

U16	ipl;

//=============================================================================
// local Fn declarations


//*****************************************************************************
// proc_init()
//  initializes the processor I/O peripherals
//	returns bitmapped initialize result status as U16
//
//*****************************************************************************
U16 proc_init(void){
	volatile uint32_t ui32Loop;
	U16	temp;					// temp

	ipl = 0;													// initialize response value
	// init Port A
	SYSCTL_RCGCGPIO_R = PORTA;
	GPIO_PORTA_DIR_R = PORTA_DIRV;
	GPIO_PORTA_DEN_R = PORTA_DENV;
	GPIO_PORTA_PUR_R = PORTA_PURV;

	// Enable the GPIO port clocks.
	SYSCTL_RCGCGPIO_R = PORTF|PORTE|PORTD|PORTC|PORTB|PORTA;

	// Do a dummy read to insert a few cycles after enabling the peripheral.
	ui32Loop = SYSCTL_RCGCGPIO_R;

	// Enable the GPIO pins.
	GPIO_PORTF_LOCK_R = 0x4C4F434B;								// unlock PORTF
	GPIO_PORTF_CR_R = 0xff;
	GPIO_PORTF_DIR_R = PORTF_DIRV;
	GPIO_PORTF_DEN_R = PORTF_DENV;
	GPIO_PORTF_AFSEL_R = 0;
	GPIO_PORTF_PUR_R = PORTF_PURV;
	GPIO_PORTE_DEN_R = PORTE_DENV;
	GPIO_PORTE_DIR_R = PORTE_DIRV;
	GPIO_PORTE_ODR_R = PORTE_ODRV;
	GPIO_PORTE_PUR_R = PORTE_PURV;
	GPIO_PORTD_AHB_LOCK_R = 0x4C4F434B;							// unlock PORTD
	GPIO_PORTD_AHB_CR_R = 0xff;
	GPIO_PORTD_AHB_DIR_R = PORTD_DIRV;
	GPIO_PORTD_AHB_DEN_R = PORTD_DENV;
	GPIO_PORTD_AHB_PUR_R = PORTD_PURV;
	GPIO_PORTC_DIR_R &= 0x0f;									// preserve JTAG pin assignments
	GPIO_PORTC_DEN_R &= 0x0f;
	GPIO_PORTC_DIR_R |= (PORTC_DIRV & 0xf0);
	GPIO_PORTC_DEN_R |= (PORTC_DENV & 0xf0);
	GPIO_PORTC_PUR_R &= 0x0f;
	GPIO_PORTC_PUR_R |= PORTC_PURV;
	GPIO_PORTB_DIR_R = PORTB_DIRV;								//0x32
	GPIO_PORTB_DEN_R = PORTB_DENV;								//0xb0;
	GPIO_PORTB_PUR_R = PORTB_PURV;

	GPIO_PORTF_DATA_R = 0x00;
	GPIO_PORTE_DATA_R = 0x00;
	GPIO_PORTD_AHB_DATA_R = 0x00;
	GPIO_PORTC_DATA_R = 0x00;
	GPIO_PORTB_DATA_R = 0x00;

	//*****************************************************************************
	//	Timer0A drives the sample clock for the TLC14 filter chip
	//*****************************************************************************
	SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R0;
	ui32Loop = SYSCTL_RCGCTIMER_R;
	GPIO_PORTB_AFSEL_R |= FIL_CLK;								// enable AF for PB6 (timer0)
	GPIO_PORTB_PCTL_R &= 0xf0ffffff;
	GPIO_PORTB_PCTL_R |= 0x07000000;
	TIMER0_CTL_R = 0;											// disable timer, do RET capture
	TIMER0_CFG_R = TIMER_CFG_16_BIT;
	TIMER0_TAMR_R = TIMER_TAMR_TAMR_PERIOD | TIMER_TAMR_TAAMS;	// PWM mode
	TIMER0_TAPR_R = TIMER0_PS;
	if(GPIO_PORTC_DATA_R & TLC14_SEL){							// select TLC14 clock rate
		temp = (uint16_t)(SYSCLK/(TLC14_FREQ * (TIMER0_PS + 1))); //TIMER0_FREQ
	}else{
		temp = (uint16_t)(SYSCLK/(TLC04_FREQ * (TIMER0_PS + 1)));
	}
	TIMER0_TAILR_R = temp;
	TIMER0_TAMATCHR_R = temp >> 1;								// set min DAC out (or very nearly)

	TIMER0_CTL_R  = TIMER_CTL_TAEVENT_POS;						// set interrupt event
	TIMER0_CTL_R |= (TIMER_CTL_TAEN);							// enable timer

	//*****************************************************************************
	//	timer2A drives appl timer, count down
	//*****************************************************************************
	SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R2;
	ui32Loop = SYSCTL_RCGCTIMER_R;
	TIMER2_CTL_R &= ~(TIMER_CTL_TAEN);							// disable timer
	TIMER2_CFG_R = 0x4;
	TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
	TIMER2_TAPR_R = (TIMER2_PS - 1);							// prescale reg = divide ratio - 1
	TIMER2_TAILR_R = (uint16_t)(SYSCLK/(KEY_SCAN_FREQ * TIMER2_PS));
	TIMER2_IMR_R = TIMER_IMR_TATOIM;							// enable timer intr
	TIMER2_CTL_R |= (TIMER_CTL_TAEN);							// enable timer
	TIMER2_ICR_R = TIMER2_MIS_R;
	NVIC_EN0_R = NVIC_EN0_TIMER2A;								// enable timer intr in the NVIC
	ipl = IPL_TIMER2INIT;

	// init PWMs on PF1-3, PE5
	SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R1;
	ui32Loop = SYSCTL_RCGCPWM_R;								// delay a few cycles
	SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R5 | SYSCTL_RCGCGPIO_R4;
	ui32Loop = SYSCTL_RCGCGPIO_R;								// delay a few cycles
	GPIO_PORTF_AFSEL_R |= LED2|LED4|LED5|LED6;					// enable alt fn, PF0-3
	GPIO_PORTF_PCTL_R &= ~(GPIO_PCTL_PF3_M|GPIO_PCTL_PF2_M|GPIO_PCTL_PF1_M|GPIO_PCTL_PF0_M);
	GPIO_PORTF_PCTL_R |= (GPIO_PCTL_PF3_M1PWM7|GPIO_PCTL_PF2_M1PWM6|GPIO_PCTL_PF1_M1PWM5|GPIO_PCTL_PF0_M1PWM4);
	GPIO_PORTE_AFSEL_R |= LED3;									// enable alt fn, PE5
	GPIO_PORTE_PCTL_R &= ~(GPIO_PCTL_PE5_M);
	GPIO_PORTE_PCTL_R |= (GPIO_PCTL_PE5_M1PWM3);
	GPIO_PORTA_AFSEL_R |= LED1;									// enable alt fn, PA6
	GPIO_PORTA_PCTL_R &= ~(GPIO_PCTL_PA6_M);
	GPIO_PORTA_PCTL_R |= (GPIO_PCTL_PA6_M1PWM2);
	SYSCTL_RCC_R = (SYSCTL_RCC_R & ~SYSCTL_RCC_PWMDIV_M) | (PWM_DIV << 17) | SYSCTL_RCC_USEPWMDIV;
	PWM1_1_CTL_R = 0;
	PWM1_1_GENA_R = PWM_1_GENA_ACTCMPAD_ONE|PWM_1_GENA_ACTLOAD_ZERO;	// M1PWM2 (LED1)
	PWM1_1_GENB_R = PWM_1_GENB_ACTCMPBD_ONE|PWM_1_GENB_ACTLOAD_ZERO;	// M1PWM3 (LED3)
	PWM1_1_LOAD_R = PWM_PERIOD;
	PWM1_1_CMPA_R = PWM_OFF;		// LED1
	PWM1_1_CMPB_R = PWM_OFF;		// LED3
	PWM1_2_CTL_R = 0;
	PWM1_2_GENA_R = PWM_2_GENA_ACTCMPAD_ONE|PWM_2_GENA_ACTLOAD_ZERO;	// M1PWM4 (LED2)
	PWM1_2_GENB_R = PWM_2_GENB_ACTCMPBD_ONE|PWM_2_GENB_ACTLOAD_ZERO;	// M1PWM5 (LED4)
	PWM1_2_LOAD_R = PWM_PERIOD;
	PWM1_2_CMPA_R = PWM_OFF;		// LED2
	PWM1_2_CMPB_R = PWM_OFF;		// LED4
	PWM1_3_CTL_R = 0;
	PWM1_3_GENA_R = PWM_3_GENA_ACTCMPAD_ONE|PWM_3_GENA_ACTLOAD_ZERO;	// M1PWM6 (LED5)
	PWM1_3_GENB_R = PWM_3_GENB_ACTCMPBD_ONE|PWM_3_GENB_ACTLOAD_ZERO;	// M1PWM7 (LED6)
	PWM1_3_LOAD_R = PWM_PERIOD;
	PWM1_3_CMPA_R = PWM_OFF;		// LED5
	PWM1_3_CMPB_R = PWM_OFF;		// LED6
	PWM1_1_CTL_R = PWM_1_CTL_ENABLE;									// enable PWM pairs
	PWM1_2_CTL_R = PWM_2_CTL_ENABLE;
	PWM1_3_CTL_R = PWM_3_CTL_ENABLE;
	PWM1_ENABLE_R = PWM_ENABLE_PWM7EN|PWM_ENABLE_PWM6EN|PWM_ENABLE_PWM5EN|PWM_ENABLE_PWM4EN|PWM_ENABLE_PWM3EN|PWM_ENABLE_PWM2EN;
	ipl |= IPL_PWM1INIT;

	// init ADC
	adc_init();
	ipl |= IPL_ADCINIT;

	// init EEPROM
	ipl |= eeprom_init();

	// adjust ISR priorities
	NVIC_PRI10_R &= NVIC_PRI10_INT42_M;			// USB = pri1
	NVIC_PRI10_R |= 1 << NVIC_PRI10_INT42_S;
	NVIC_PRI9_R &= NVIC_PRI9_INT36_M;			// timer3b = pri2
	NVIC_PRI9_R |= 2 << NVIC_PRI9_INT36_S;
	NVIC_PRI5_R &= NVIC_PRI5_INT23_M;			// timer2a = pri3
	NVIC_PRI5_R |= 3 << NVIC_PRI5_INT23_S;
	NVIC_PRI1_R &= NVIC_PRI1_INT5_M;			// uart0 = pri4
	NVIC_PRI1_R |= 4 << NVIC_PRI1_INT5_S;
	NVIC_PRI0_R &= NVIC_PRI0_INT1_M;			// gpio pb = pri5
	NVIC_PRI0_R |= 5 << NVIC_PRI0_INT1_S;

	// init force paddle register
	paddle_force_set(FORCE_OFF);

	return ipl;
}

//*****************************************************************************
// get_ipl()
//  returns ipl status (declared in init.h)
//
//*****************************************************************************
U16 get_ipl(void){
	return ipl;
}
