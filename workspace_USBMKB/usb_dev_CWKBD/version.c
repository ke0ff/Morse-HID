/********************************************************************
 ************ COPYRIGHT (c) 2019 by ke0ff, Taylor, TX   *************
 *
 *  File name: serial.c
 *
 *  Module:    Control
 *
 *  Summary:   This is the serial I/O module for the FF-PGMR11
 *             protocol converter.
 *
 *******************************************************************/


/********************************************************************
 *  File scope declarations revision history:
 *    07-30-14 jmh:  creation date
 *
 *******************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include "inc/tm4c123gh6pm.h"
#include "typedef.h"
#define VERSOURCE
#include "version.h"
#include "stdio.h"
#include "init.h"
#include "utils/uartstdio.h"

//------------------------------------------------------------------------------
// Define Statements
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Local Variable Declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Local Fn Declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// dispSWvers() displays SW version
//-----------------------------------------------------------------------------
void dispSWvers(void){
	U16  i;			// temp
	
	UARTprintf("KE0FF Morse->USB Keyboard\n");
    UARTprintf("Vers: %s, Date: %s\n",version_number,date_code);
    i = get_ipl();
   	UARTprintf("IPL: %04x\n",i);
}
