/********************************************************************
 ************ COPYRIGHT (c) 2019 by ke0ff, Taylor, TX   *************
 *
 *  File name: adc.c
 *
 *  Module:    Control
 *
 *  Summary:
 *  Tiva adc support functions
 *
 *******************************************************************/

#include <stdint.h>
#include <ctype.h>
#include "inc/tm4c123gh6pm.h"
#include "typedef.h"
#include "init.h"						// App-specific SFR Definitions
#include "adc.h"

//=============================================================================
// local registers


//=============================================================================
// local Fn declarations


//*****************************************************************************
// adc_init()
//  initializes the processor ADC peripheral
//	returns bitmapped initialize result status as U16
//
//*****************************************************************************
U16 adc_init(void)
{
	volatile uint32_t	ui32Loop;

    // ADC init
	SYSCTL_RCGCADC_R |= SYSCTL_RCGCADC_R0;			// enable ADC clock
	ui32Loop = SYSCTL_RCGCADC_R;
	SYSCTL_RCGC0_R |= SYSCTL_RCGC0_ADC0;   			// activate ADC0 (legacy code)
	ui32Loop = SYSCTL_RCGC0_R;

	GPIO_PORTB_AFSEL_R |= CWSPEED | CWTONE;			// enable alt fn
	GPIO_PORTB_AMSEL_R |= CWSPEED | CWTONE;
	ADC0_CC_R = ADC_CC_CS_PIOSC;					// use PIOSC
	ADC0_PC_R = ADC_PC_SR_125K;						// 125KHz samp rate
	// ADC sequencer init
	ADC0_SSPRI_R = 0x1023;							// Sequencer 2 is highest priority
	ADC0_ACTSS_R &= ~ADC_ACTSS_ASEN2;				// disable sample sequencer 2
	ADC0_EMUX_R &= ~ADC_EMUX_EM2_M;					// seq2 is software trigger
	ADC0_SSMUX2_R = 0xbbaa;							// set channels, two ch11 and two ch10 (we'll only use the 2nd of each)
	ADC0_SSCTL2_R = (ADC_SSCTL0_IE3|ADC_SSCTL0_END3);
													// do 4 samples (no temp sense)
	ADC0_SAC_R = ADC_SAC_AVG_64X;					// set 64x averaging
	ADC0_IM_R &= ~ADC_IM_MASK2;						// disable SS2 interrupts
	ADC0_ACTSS_R |= ADC_ACTSS_ASEN2;				// enable sample sequencer 2
	return 0;
}

//*****************************************************************************
// adc_in()
//	Must call adc_start() before calling this routine.
//  if SS2 is finished, this Fn will return ADC results as a list of U16 values
//  placed starting at the pointer 'p' (assumed to point to an array).  In the
//  array, even offsets are status, odd offsets are data.
//	Returns the number of writes to array as a U8.  If this value is zero, the
//	ADC SS2 wasn't ready, so try again later...
//
//	Formulae for the on-chip Tj sensor:
//	float voltage = rawADC * Vref / maxADC ; Vref = 3.3V, maxADC = 0x1000
//	Vtj = 2.7 - ((TJ + 55) / 75)
//	Vtj = rawADC * 3.3 / 4096
//	TJ = (-75 * ((rawADC * 3.3 / 4096) - 2.7)) - 55
//	TJ = 147.5 - (75 * (rawADC * 3.3 / 4096))
//
//*****************************************************************************
U8 adc_in(U16* p)
{
#define NUM_SAMPS	4		// number of samples in sequence
	U8	i = 0;

	if((ADC0_RIS_R & ADC_RIS_INR2) != 0){
		ADC0_ISC_R = ADC_ISC_IN2;						// acknowledge completion (clr flag)
		for(i=0; i<NUM_SAMPS; i++){
			*p++ = ADC0_SSFSTAT2_R & 0xffff;			// get fifo status in 1st buffer word
			*p++ = ADC0_SSFIFO2_R & 0x0fff;				// read result in 2nd buffer word
		}
	}
	return i;
}

//*****************************************************************************
// adc_start()
//  starts SS2.  Call adc_in some time after this Fn and test for a non-zero
//	return (which indicates that SS2 completed, and ADC values were obtained).
//
//*****************************************************************************
void adc_start(void)
{

	ADC0_PSSI_R = ADC_PSSI_SS2;						// initiate SS2
	return;
}
