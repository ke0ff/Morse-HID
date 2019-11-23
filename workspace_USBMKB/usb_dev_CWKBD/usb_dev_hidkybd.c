//*****************************************************************************
//
// usb_dev_hidkybd.c - Main routines for the Keybd example.
//
// Copyright (c) 2013-2017 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.4.178 of the EK-TM4C123GXL Firmware Package.
//
//*****************************************************************************
//
// Modified March-May, 2019 by Joseph Haas, KE0FF for the Morse->USB HID
//	Keyboard project.  Modified code is supplied as-is including any faults.
//	Use at your own risk.
//
//	This code creates a device that accepts Morse code paddle inputs and provides
//	a "standard" English-language USB Keyboard interface.  The system interprets
//	paddle inputs as Morse or custom code and presents the corresponding
//	character to the PC as a keyboard press-release character event.  A 4x5
//	keypad is implemented that allows custom keypress definintions that are run-
//	time programmable.  PWM driven LEDs (six, total) provide status info, and a
//	DDS tone generator provides side-tone for paddle characters and for status
//	messages.
//
/******************************************************************************
 *  File scope declarations revision history:
 *    Version 6:
 *    08-23-19	jmh:	Trying some prophylactic fixes to the mystery silence issue.
 *    					Fixed problem with ADC update code.  Was using garbaged variables which could cause undefined behavior
 *    						(possibly the mystery silence issue).
 *    					Added "WPM" prosign (/WWW) to read out morse speed (in CW).
 *    Version 5:
 *    07-07-19	jmh:	Final-finalized straight-key logic.  Added /SKS prosign to switch between paddle and straight-key mode.
 *    					Added /SKS prosign to "*" position on default keypad
 *    					Modified keypad_init() to accept a force-init parameter.  morse_init() result is then passed to keypad_init()
 *    						to force update of keypad assignments back to factory defaults.
 *    06-18-19	jmh:	Finalized straight-key timing decode logic (see "morse.c")
 *    06-07-19	jmh:	Added code to capture and decode straight-key input.  See "morse.c" for detailed notes.  Uses DIT GPIO for
 *    						straight-key contact input.
 *    05-30-19	jmh:	Added unlock-on-powerup jumper (PD3, J3-6 on the LaunchPad...Open = lock on powerup, GND = unlock on powerup)
 *    					Added jumper to select iambic or straight key mode (PD2, J3-5 on the LP, Open = iambic, GND = SK).  Placed
 *    						branch tests to provide stubs for the new feature.  Tested in iambic mode to verify that there were no issues.
 *    Version 4:
 *    05-05-19	jmh:	STW lock didn't work if EEPROM wasn't stored (and thus valid).  Modified to allow tone lock to
 *    						function even if EEPROM was invalid (using the pot settings from the point at which the lock
 *    						was detected).  Also added init_stw() to do a fault recover if there is any invalid STW EEPROM
 *    						value (all must be valid or none are valid).
 *    					Revamped not-connected processing to improve the WIN LED blink logic and the code readability.
 *    Version 3:
 *    04-27-19	jmh:	Testing complete.  SW released for public consumption (V3).
 *    04-27-19  jmh:	Added user pro-sign and support to program
 *    					Added "beep" (a single "E" character) to akn backspace characters
 *    					Added "beep-beep" (a single "I" character) to akn delete characters
 *    					Added "FI" boot message if fact init executed
 *    04-21-19  jmh:	Multiple cleanups
 *    					added keyswp code trap and support for ALT KP LED.  Alt keypad support also added.
 *    						Alt keypad programming is the same as before.  The keypad programmed is the one
 *    						that was active when programming started.  To program both keypads, the first
 *    						must be saved, then the other selected, then programming mode entered again (then saved).
 *    					added CTRL-Z support
 *    					added LED support for all (six) LEDs
 *    					Modifier keys are now toggle, so re-issuing a modifier will cancel it.
 *    04-18-19  jmh:	Added support for keypad code map pgm mode
 *    03-30-19  jmh:	creation date
 *
 ******************************************************************************/

//#define	DEBUG_U

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "usblib/usblib.h"
#include "usblib/usbhid.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdhid.h"
#include "usblib/device/usbdhidkeyb.h"
#include "utils/uartstdio.h"
#include "init.h"
#include "morse.h"
#include "morse_lut.h"
#include "tiva_init.h"
#include "version.h"
#include "adc.h"
#include "usb_hidkybd_structs.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>USB HID Keybd Device </h1>
//!
//! This example application turns the evaluation board into a USB keyboard
//! device using the Human Interface Device Keybd class.
//! It was created by starting with the GamePad example for the 123GXL LaunchPad
//!	(TM4C123GH6PM) and copying in elements from the 123G DK example for a
//! USB-DEV-Keyboard.
//
//*****************************************************************************

void SendKey(char key, uint8_t modifier);

//*****************************************************************************
//
// A mapping from the ASCII value received from the UART to the corresponding
// USB HID usage code.
//
//*****************************************************************************
static const int8_t g_ppi8KeyUsageCodes[][2] =
{
    { 0, HID_KEYB_USAGE_SPACE },                       //   0x20
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_1 },         // ! 0x21
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_FQUOTE },    // " 0x22
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_3 },         // # 0x23
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_4 },         // $ 0x24
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_5 },         // % 0x25
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_7 },         // & 0x26
    { 0, HID_KEYB_USAGE_FQUOTE },                      // ' 0x27
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_9 },         // ( 0x28
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_0 },         // ) 0x29
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_8 },         // * 0x2a
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_EQUAL },     // + 0x2b
    { 0, HID_KEYB_USAGE_COMMA },                       // , 0x2c
    { 0, HID_KEYB_USAGE_MINUS },                       // - 0x2d
    { 0, HID_KEYB_USAGE_PERIOD },                      // . 0x2e
    { 0, HID_KEYB_USAGE_FSLASH },                      // / 0x2f
    { 0, HID_KEYB_USAGE_0 },                           // 0 0x30
    { 0, HID_KEYB_USAGE_1 },                           // 1 0x31
    { 0, HID_KEYB_USAGE_2 },                           // 2 0x32
    { 0, HID_KEYB_USAGE_3 },                           // 3 0x33
    { 0, HID_KEYB_USAGE_4 },                           // 4 0x34
    { 0, HID_KEYB_USAGE_5 },                           // 5 0x35
    { 0, HID_KEYB_USAGE_6 },                           // 6 0x36
    { 0, HID_KEYB_USAGE_7 },                           // 7 0x37
    { 0, HID_KEYB_USAGE_8 },                           // 8 0x38
    { 0, HID_KEYB_USAGE_9 },                           // 9 0x39
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_SEMICOLON }, // : 0x3a
    { 0, HID_KEYB_USAGE_SEMICOLON },                   // ; 0x3b
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_COMMA },     // < 0x3c
    { 0, HID_KEYB_USAGE_EQUAL },                       // = 0x3d
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_PERIOD },    // > 0x3e
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_FSLASH },    // ? 0x3f
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_2 },         // @ 0x40
    { 0, HID_KEYB_USAGE_A },         // A 0x41
    { 0, HID_KEYB_USAGE_B },         // B 0x42
    { 0, HID_KEYB_USAGE_C },         // C 0x43
    { 0, HID_KEYB_USAGE_D },         // D 0x44
    { 0, HID_KEYB_USAGE_E },         // E 0x45
    { 0, HID_KEYB_USAGE_F },         // F 0x46
    { 0, HID_KEYB_USAGE_G },         // G 0x47
    { 0, HID_KEYB_USAGE_H },         // H 0x48
    { 0, HID_KEYB_USAGE_I },         // I 0x49
    { 0, HID_KEYB_USAGE_J },         // J 0x4a
    { 0, HID_KEYB_USAGE_K },         // K 0x4b
    { 0, HID_KEYB_USAGE_L },         // L 0x4c
    { 0, HID_KEYB_USAGE_M },         // M 0x4d
    { 0, HID_KEYB_USAGE_N },         // N 0x4e
    { 0, HID_KEYB_USAGE_O },         // O 0x4f
    { 0, HID_KEYB_USAGE_P },         // P 0x50
    { 0, HID_KEYB_USAGE_Q },         // Q 0x51
    { 0, HID_KEYB_USAGE_R },         // R 0x52
    { 0, HID_KEYB_USAGE_S },         // S 0x53
    { 0, HID_KEYB_USAGE_T },         // T 0x54
    { 0, HID_KEYB_USAGE_U },         // U 0x55
    { 0, HID_KEYB_USAGE_V },         // V 0x56
    { 0, HID_KEYB_USAGE_W },         // W 0x57
    { 0, HID_KEYB_USAGE_X },         // X 0x58
    { 0, HID_KEYB_USAGE_Y },         // Y 0x59
    { 0, HID_KEYB_USAGE_Z },         // Z 0x5a
    { 0, HID_KEYB_USAGE_LBRACKET },                    // [ 0x5b
    { 0, HID_KEYB_USAGE_BSLASH },                      // \ 0x5c
    { 0, HID_KEYB_USAGE_RBRACKET },                    // ] 0x5d
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_6 },         // ^ 0x5e
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_MINUS },     // _ 0x5f
    { 0, HID_KEYB_USAGE_BQUOTE },                      // ` 0x60
    { 0, HID_KEYB_USAGE_A },                           // a 0x61
    { 0, HID_KEYB_USAGE_B },                           // b 0x62
    { 0, HID_KEYB_USAGE_C },                           // c 0x63
    { 0, HID_KEYB_USAGE_D },                           // d 0x64
    { 0, HID_KEYB_USAGE_E },                           // e 0x65
    { 0, HID_KEYB_USAGE_F },                           // f 0x66
    { 0, HID_KEYB_USAGE_G },                           // g 0x67
    { 0, HID_KEYB_USAGE_H },                           // h 0x68
    { 0, HID_KEYB_USAGE_I },                           // i 0x69
    { 0, HID_KEYB_USAGE_J },                           // j 0x6a
    { 0, HID_KEYB_USAGE_K },                           // k 0x6b
    { 0, HID_KEYB_USAGE_L },                           // l 0x6c
    { 0, HID_KEYB_USAGE_M },                           // m 0x6d
    { 0, HID_KEYB_USAGE_N },                           // n 0x6e
    { 0, HID_KEYB_USAGE_O },                           // o 0x6f
    { 0, HID_KEYB_USAGE_P },                           // p 0x70
    { 0, HID_KEYB_USAGE_Q },                           // q 0x71
    { 0, HID_KEYB_USAGE_R },                           // r 0x72
    { 0, HID_KEYB_USAGE_S },                           // s 0x73
    { 0, HID_KEYB_USAGE_T },                           // t 0x74
    { 0, HID_KEYB_USAGE_U },                           // u 0x75
    { 0, HID_KEYB_USAGE_V },                           // v 0x76
    { 0, HID_KEYB_USAGE_W },                           // w 0x77
    { 0, HID_KEYB_USAGE_X },                           // x 0x78
    { 0, HID_KEYB_USAGE_Y },                           // y 0x79
    { 0, HID_KEYB_USAGE_Z },                           // z 0x7a
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_LBRACKET },  // { 0x7b
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_BSLASH },    // | 0x7c
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_RBRACKET },  // } 0x7d
    { HID_KEYB_LEFT_SHIFT, HID_KEYB_USAGE_BQUOTE },    // ~ 0x7e
    { 0, HID_KEYB_USAGE_DEL },    			   		   // ~ 0x7f
    { 0, HID_KEYB_USAGE_SPACE },    			   	   // ~ 0x80
    { 0, 0 },    			   	   // ~ 0x81	MRSE_SHIFT	(129)
    { 0, HID_KEYB_USAGE_CAPSLOCK },    			   	   // ~ 0x82	MRSE_CAPLOCK	(130)
    { 0, HID_KEYB_USAGE_PAGE_UP },    			   	   // ~ 0x83	MRSE_PGUP		(131)
    { 0, HID_KEYB_USAGE_PAGE_DOWN },    			   // ~ 0x84	MRSE_PGDN		(132)
    { 0, 0 },    			   	   // ~ 0x85	MRSE_ALT		(133)
    { 0, 0 },    			   	   // ~ 0x86	MRSE_CNTL		(134)
    { HID_KEYB_LEFT_GUI, HID_KEYB_USAGE_L },    		// ~ 0x87	MRSE_WINL		(135)
    { 0, HID_KEYB_LEFT_GUI },    			   	   		// ~ 0x88	MRSE_WIN		(136)
    { 0, HID_KEYB_USAGE_F1 },    			   	   		// ~ 0x89	MRSE_F1			(137)
    { 0, HID_KEYB_USAGE_F2 },    			   	   		// ~ 0x8a	MRSE_F2			(138)
    { 0, HID_KEYB_USAGE_F3 },    			   	   		// ~ 0x8b	MRSE_F3			(139)
    { 0, HID_KEYB_USAGE_F4 },    			   	   		// ~ 0x8c	MRSE_F4			(140)
    { 0, HID_KEYB_USAGE_F5 },    			   	   		// ~ 0x8d	MRSE_F5			(141)
    { 0, HID_KEYB_USAGE_F6 },    			   	   		// ~ 0x8e	MRSE_F6			(142)
    { 0, HID_KEYB_USAGE_F7 },    			   	   		// ~ 0x8f	MRSE_F7			(143)
    { 0, HID_KEYB_USAGE_F8 },    			   	   		// ~ 0x90	MRSE_F8			(144)
    { 0, HID_KEYB_USAGE_F9 },    			   	   		// ~ 0x91	MRSE_F9			(145)
    { 0, HID_KEYB_USAGE_F10 },    			   	   		// ~ 0x92	MRSE_F10		(146)
    { 0, HID_KEYB_USAGE_F11 },    			   	   		// ~ 0x93	MRSE_F11		(147)
    { 0, HID_KEYB_USAGE_F12 },    			   	   		// ~ 0x94	MRSE_F12		(148)
    { 0, HID_KEYB_USAGE_UP_ARROW },    			   	   	// ~ 0x95	MRSE_UP			(149)
    { 0, HID_KEYB_USAGE_DOWN_ARROW },    			   	// ~ 0x96	MRSE_DN			(150)
    { 0, HID_KEYB_USAGE_LEFT_ARROW },    			   	// ~ 0x97	MRSE_LEFT		(151)
    { 0, HID_KEYB_USAGE_RIGHT_ARROW },    			   	// ~ 0x98	MRSE_RIGHT		(152)
    { 0, HID_KEYB_USAGE_BACKSPACE },    			   	// ~ 0x99	MRSE_BACKCSP	(153)
    { 0, HID_KEYB_USAGE_TAB },    			   	   		// ~ 0x9a	MRSE_TAB		(154)
    { 0, HID_KEYB_USAGE_ENTER },    			   		// ~ 0x9b	MRSE_CR			(155)
    { 0, HID_KEYB_USAGE_ESCAPE }    			   	   	// ~ 0x9c	MRSE_ESC		(156)
};

//*****************************************************************************
//
// The system tick timer period.
//
//*****************************************************************************
#define SYSTICKS_PER_SECOND     100

//*****************************************************************************
//
// This global indicates whether or not we are connected to a USB host.
//
//*****************************************************************************
volatile bool g_bConnected = false;

//*****************************************************************************
//
// This global indicates whether or not the USB bus is currently in the suspend
// state.
//
//*****************************************************************************
volatile bool g_bSuspended = false;

//*****************************************************************************
//
// Global system tick counter holds elapsed time since the application started
// expressed in 100ths of a second.
//
//*****************************************************************************
volatile uint32_t g_ui32SysTickCount;

//*****************************************************************************
//
// The number of system ticks to wait for each USB packet to be sent before
// we assume the host has disconnected.  The value 50 equates to half a second.
//
//*****************************************************************************
#define MAX_SEND_DELAY          50

//*****************************************************************************
//
// This enumeration holds the various states that the Keybd can be in during
// normal operation.
//
//*****************************************************************************
volatile enum
{
    //
    // Not yet configured.
    //
    eStateNotConfigured,

    //
    // Connected and not waiting on data to be sent.
    //
    eStateIdle,

    //
    // Suspended.
    //
    eStateSuspend,

    //
    // Connected and waiting on data to be sent out.
    //
    eStateSending
}
g_iKeybdState;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

//*****************************************************************************
//
// Macro used to convert the 12-bit unsigned values to an eight bit signed
// value returned in the HID report.  This maps the values from the ADC that
// range from 0 to 2047 over to 127 to -128.
//
//*****************************************************************************
#define Convert8Bit(ui32Value)  ((int8_t)((0x7ff - ui32Value) >> 4))


//*****************************************************************************
//
// This global is set to true if the host sends a request to set or clear
// any keyboard LED.
//
//*****************************************************************************
volatile bool g_bDisplayUpdateRequired;

//*****************************************************************************
//
// This enumeration holds the various states that the keyboard can be in during
// normal operation.
//
//*****************************************************************************
volatile enum
{
    //
    // Unconfigured.
    //
    STATE_UNCONFIGURED,

    //
    // No keys to send and not waiting on data.
    //
    STATE_IDLE,

    //
    // Waiting on data to be sent out.
    //
    STATE_SENDING
}
g_eKeyboardState = STATE_UNCONFIGURED;

//*****************************************************************************
//
// Handles asynchronous events from the HID keyboard driver.
//
// \param pvCBData is the event callback pointer provided during
// USBDHIDKeyboardInit().  This is a pointer to our keyboard device structure
// (&g_sKeyboardDevice).
// \param ui32Event identifies the event we are being called back for.
// \param ui32MsgData is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the HID keyboard driver to inform the application
// of particular asynchronous events related to operation of the keyboard HID
// device.
//
// \return Returns 0 in all cases.
//
//*****************************************************************************
uint32_t
KeyboardHandler(void *pvCBData, uint32_t ui32Event, uint32_t ui32MsgData,
                void *pvMsgData)
{
    switch (ui32Event)
    {
        //
        // The host has connected to us and configured the device.
        //
        case USB_EVENT_CONNECTED:
        {
            g_bConnected = true;
            g_bSuspended = false;
            break;
        }

        //
        // The host has disconnected from us.
        //
        case USB_EVENT_DISCONNECTED:
        {
            g_bConnected = false;
            break;
        }

        //
        // We receive this event every time the host acknowledges transmission
        // of a report. It is used here purely as a way of determining whether
        // the host is still talking to us or not.
        //
        case USB_EVENT_TX_COMPLETE:
        {
            //
            // Enter the idle state since we finished sending something.
            //
            g_eKeyboardState = STATE_IDLE;
            break;
        }

        //
        // This event indicates that the host has suspended the USB bus.
        //
        case USB_EVENT_SUSPEND:
        {
            g_bSuspended = true;
            break;
        }

        //
        // This event signals that the host has resumed signalling on the bus.
        //
        case USB_EVENT_RESUME:
        {
            g_bSuspended = false;
            break;
        }

        //
        // This event indicates that the host has sent us an Output or
        // Feature report and that the report is now in the buffer we provided
        // on the previous USBD_HID_EVENT_GET_REPORT_BUFFER callback.
        //
        case USBD_HID_KEYB_EVENT_SET_LEDS:
        {
            //
            // Set the LED to match the current state of the caps lock LED.
            //
        	if(ui32MsgData & HID_KEYB_CAPS_LOCK){
    			set_led(CAPLK_LED, 1);
        	}else{
    			set_led(CAPLK_LED, 0);
        	}
            break;
        }

        //
        // We ignore all other events.
        //
        default:
        {
            break;
        }
    }

    return(0);
}

//***************************************************************************
//
// Wait for a period of time for the state to become idle.
//
// \param ui32TimeoutTick is the number of system ticks to wait before
// declaring a timeout and returning \b false.
//
// This function polls the current keyboard state for ui32TimeoutTicks system
// ticks waiting for it to become idle.  If the state becomes idle, the
// function returns true.  If it ui32TimeoutTicks occur prior to the state
// becoming idle, false is returned to indicate a timeout.
//
// \return Returns \b true on success or \b false on timeout.
//
//***************************************************************************
bool
WaitForSendIdle(uint_fast32_t ui32TimeoutTicks)
{
    uint32_t ui32Start;
    uint32_t ui32Now;
    uint32_t ui32Elapsed;

    ui32Start = g_ui32SysTickCount;
    ui32Elapsed = 0;

    while(ui32Elapsed < ui32TimeoutTicks)
    {
        //
        // Is the keyboard is idle, return immediately.
        //
        if(g_eKeyboardState == STATE_IDLE)
        {
            return(true);
        }

        //
        // Determine how much time has elapsed since we started waiting.  This
        // should be safe across a wrap of g_ui32SysTickCount.
        //
        ui32Now = g_ui32SysTickCount;
        ui32Elapsed = ((ui32Start < ui32Now) ? (ui32Now - ui32Start) :
                     (((uint32_t)0xFFFFFFFF - ui32Start) + ui32Now + 1));
    }

    //
    // If we get here, we timed out so return a bad return code to let the
    // caller know.
    //
    return(false);
}

//*****************************************************************************
//
// Sends a string of characters via the USB HID keyboard interface.
//
//*****************************************************************************
void
SendString(char *pcStr)
{
    uint32_t ui32Char;

    //
    // Loop while there are more characters in the string.
    //
    while(*pcStr)
    {
        //
        // Get the next character from the string.
        //
        ui32Char = *pcStr++;

        //
        // Skip this character if it is a non-printable character.
        //
        if((ui32Char < ' ') || (ui32Char > MRSE_ESC))
        {
            continue;
        }

        //
        // Convert the character into an index into the keyboard usage code
        // table.
        //
        ui32Char -= ' ';

        //
        // Send the key press message.
        //
        g_eKeyboardState = STATE_SENDING;
        if(USBDHIDKeyboardKeyStateChange((void *)&g_sKeyboardDevice,
                                         g_ppi8KeyUsageCodes[ui32Char][0],
                                         g_ppi8KeyUsageCodes[ui32Char][1],
                                         true) != KEYB_SUCCESS)
        {
            return;
        }

        //
        // Wait until the key press message has been sent.
        //
        if(!WaitForSendIdle(MAX_SEND_DELAY))
        {
            g_bConnected = 0;
            return;
        }

        //
        // Send the key release message.
        //
        g_eKeyboardState = STATE_SENDING;
        if(USBDHIDKeyboardKeyStateChange((void *)&g_sKeyboardDevice,
                                         0, g_ppi8KeyUsageCodes[ui32Char][1],
                                         false) != KEYB_SUCCESS)
        {
            return;
        }

        //
        // Wait until the key release message has been sent.
        //
        if(!WaitForSendIdle(MAX_SEND_DELAY))
        {
            g_bConnected = 0;
            return;
        }
    }
}

//*****************************************************************************
//
// Sends a key strike/release event via the USB HID keyboard interface.
//
//*****************************************************************************
void
SendKey(char key, uint8_t modifier)
{
    uint32_t ui32Char = key;

    //;
    // Loop while there are more characters in the string.
    //
    if((ui32Char >= ' ') || (ui32Char <= LAST_KEY)){

        //
        // Convert the character into an index into the keyboard usage code
        // table.
        //
        ui32Char -= ' ';

        //
        // Send the key press message.
        //
        g_eKeyboardState = STATE_SENDING;

        if(USBDHIDKeyboardKeyStateChange((void *)&g_sKeyboardDevice,
// The original example used hard-coded modifiers for the send string function
//                                        g_ppi8KeyUsageCodes[ui32Char][0],
        								 modifier,
                                         g_ppi8KeyUsageCodes[ui32Char][1],
                                         true) != KEYB_SUCCESS)
        {
            return;
        }

        //
        // Wait until the key press message has been sent.
        //
        if(!WaitForSendIdle(MAX_SEND_DELAY))
        {
            g_bConnected = 0;
            return;
        }

        //
        // Send the key release message.
        //
        g_eKeyboardState = STATE_SENDING;
        if(USBDHIDKeyboardKeyStateChange((void *)&g_sKeyboardDevice,
                                         0, g_ppi8KeyUsageCodes[ui32Char][1],
                                         false) != KEYB_SUCCESS)
        {
            return;
        }

        //
        // Wait until the key release message has been sent.
        //
        if(!WaitForSendIdle(MAX_SEND_DELAY))
        {
            g_bConnected = 0;
            return;
        }
    }
}

//*****************************************************************************
//
// This is the interrupt handler for the SysTick interrupt.  It is used to
// update our local tick count which, in turn, is used to check for transmit
// timeouts.
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
    g_ui32SysTickCount++;
}

//*****************************************************************************
//
// Configure the UART and its pins.  This must be called before UARTprintf().
//
//*****************************************************************************
void
ConfigureUART(void)
{
    //
    // Enable the GPIO Peripheral used by the UART.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Enable UART0
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Configure GPIO Pins for UART mode.
    //
    ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
    ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Use the internal 16MHz oscillator as the UART clock source.
    //
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, 16000000);
}

//*****************************************************************************
//
// This is the main loop that runs the application.
//
//*****************************************************************************
int
main(void)
{
    bool bLastSuspend;
    char	c;						// Morse code character
    char	kc;						// keypad character
    char	pc;						// keypad program character
    U8		kcode;					// keypad keycode reg
    U8		kalt;					// alt keypad select
    U8		kpgm_dirty;
    U8		update_ee = TRUE;		// if true, fetch EEPROM settings
    char	buf[30];
    U8		i;						// temp
    U8		finit;					// fact init temp
#define	ADC_BUF_MAX	16
    U8		ai;						// current adc buffer index
#define	ADC_PACE	8
    uint8_t	mod_mem = 0;
    uint8_t	sticky_shift = 0;
    uint8_t	sticky_cntl = 0;
    volatile uint8_t t;
    uint8_t	lock_kybd;				// keyboard lock status reg locked
    U16		morse_speed = 0xffff;	// force init
    U16		morse_tone = 0xffff;
    U16		morse_weight = 0xffff;
    U16		ts = 1024;				// temp speed
    U16		tt = (1024 >> 3);		// temp tone (init to lower-mid-range)
    U16		tw = 0;					// temp weight
	U16		adc_buf[8];				// adc buffer
	U16*	ip;						// U16 pointer
	U16		cws_buf[ADC_BUF_MAX];	// Morse speed adc buffer
	U16		cwt_buf[ADC_BUF_MAX];	// Morse tone adc buffer
	S32		si;						// signed temp
	U32		ei;						// temp sum regs for ADC
	U32		fi;

    //
    // Set the clocking to run from the PLL at 50MHz
    //
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                       SYSCTL_XTAL_16MHZ);

    //
    // Open UART0 and show the application name on the UART.
    //

    ConfigureUART();

    //
    // Not configured initially.
    //
    g_iKeybdState = eStateNotConfigured;

    //
    // Init Morse input subsystem
    //
    SysCtlGPIOAHBEnable(SYSCTL_PERIPH_GPIOD);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	// prophylactic init of stw parameters
	init_stw();													// init timing/tone settings
    proc_init();
    finit = Process_CW(INIT_PROCESS);
    UARTprintf("\n\n--------------------\n");
    dispSWvers();
    //
    // Enable the GPIO peripheral used for USB, and configure the USB
    // pins.
    //
    ROM_GPIOPinTypeUSBAnalog(GPIO_PORTD_AHB_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    //
    // Tell the user what we are up to.
    //
    UARTprintf("Configuring USB\n");

    //
    // Set the USB stack mode to Device mode.
    //
    USBStackModeSet(0, eUSBModeForceDevice, 0);

    //
    // Pass the device information to the USB library and place the device
    // on the bus.
    //
    USBDHIDKeyboardInit(0, &g_sKeyboardDevice);

    //
    // Set the system tick to fire 100 times per second.
    //
    ROM_SysTickPeriodSet(ROM_SysCtlClockGet() / SYSTICKS_PER_SECOND);
    ROM_SysTickIntEnable();
    ROM_SysTickEnable();

    //
    // Tell the user what we are doing and provide some basic instructions.
    //
    UARTprintf("\nMorse->USBKbd started.\n");
    UARTprintf("--------------------\n");

	//
	// Set the power-on lock status
	lock_kybd = get_poweron_lock_strap();

    // send Morse SW version to signal startup to user
	put_cw(' ');
    put_cw('V');
    put_cw(VERSION_CHARACTER);									// send SW version
    put_cw(set_iambic_mode());									// set & send iambic mode
    if(finit == CW_FACINT){
        put_cw(' ');											// signal factory init
        put_cw('F');
        put_cw('I');
    }
    if(!lock_kybd){												// test for PON-UNLK jumper placement
        put_cw(' ');											// signal unlock
        put_cw('U');
        put_cw('L');
        lock_kybd = 0;											// unlock
    }
    //
    // init LED and ADC subsystems
    //
    set_led(INIT_LEDS, 0);
	adc_start();												// start an ADC conversion

	// clean any spurious Morse/key inputs from IPL
	//
    c = getchar_cw();
   	kc = get_key();
   	if(lock_kybd){
   		set_led(SHFLK_LED, 1);									// if paddles locked to start...make the shift-lock LED blink
   		blink_led(SHFLK_LED, 1);
   	}
	kalt = MAIN_KP_SEL;											// init keypad select
	set_kpalt(kalt);
	if(get_stw_lock_strap()){									// if STW locked, restore values from EEPROM
		if(!get_stw_ee()){
			init_stw();											// fault recover timing/tone settings
		}
	}
    //
    // The main loop starts here.  We begin by waiting for a host connection
    // then drop into the main keyboard handling section.  If the host
    // disconnects, we return to the top and wait for a new connection.
    //

    while(1){
//********************************  START OF MAIN LOOP  ************************************************
    	// Tell the user what we are doing and provide some basic instructions.
        UARTprintf("\nWaiting For Host...\n");
        // Wait here until USB device is connected to a host.
/*        i = 0;													// set up interlock
        while(!g_bConnected)
        {														// Process Morse paddles, but nothing will happen except side-tones while not connected
            Process_CW(0);										// Any paddle or keypad entries will be lost...
			set_led(WIN_LED, 1);								// blink WIN LED if not connected
			blink_led(WIN_LED, 1);
			i = 1;												// engage interlock
        }*/
        if(!g_bConnected){										// if not connected ...	Process Morse paddles, but nothing else will happen except side-tones while not connected
			set_led(WIN_LED, 1);								// blink WIN LED if not connected
			blink_led(WIN_LED, 1);
	        while(!g_bConnected)
	        {
	        	Process_CW(0);									// Any paddle or keypad entries will be lost...
	        }
	        // setup system to enter connected mode
    		set_led(WIN_LED, 0);								// turn off WIN LED blink & resume loop...
    		blink_led(WIN_LED, 0);
    		set_led(ALT_LED, 0);								// turn off ALT LED
    		blink_led(ALT_LED, 0);								// no blink in any case
    		mod_mem = 0;										// clear any pending modifiers
//    		do{
//    		}while(get_key());									// clear out keypad buffer
        }
        // Update the status.
        UARTprintf("\nHost connected...\n");
        // Enter the idle state.
        g_eKeyboardState = STATE_IDLE;
        // Assume that the bus is not currently suspended if we have just been
        // configured.
        bLastSuspend = false;
        // Run loop while USB connected:
        // Keep processing characters/keypresses from the CW app to the USB host for as
        // long as we are connected to the host.
//******************************************************************************************************

//***************************  START OF USB-CONNECTED LOOP  ********************************************
        while(g_bConnected){
        	// Process CW paddle input
            Process_CW(0);
        	// Has the suspend state changed since last time we checked?
            if(bLastSuspend != g_bSuspended){
                // Yes - the state changed so update the display.
                bLastSuspend = g_bSuspended;
                if(bLastSuspend){
                    UARTprintf("\nUSB suspended...\n");
        			set_led(ALT_LED, 1);						// blink ALT LED if not connected
        			blink_led(ALT_LED, 1);
               }else{
                    UARTprintf("\nHost reconnected...\n");
            		if(!(mod_mem & HID_KEYB_LEFT_ALT)){			// if ALT modifier active, already selected, keep it
            			set_led(ALT_LED, 0);					// turn off ALT LED
            		}
            		blink_led(ALT_LED, 0);						// no blink in any case
                 }
            }
//******************************************************************************************************

//********************************  PROCESS ADC INPUTS  ************************************************
            //
            // update ADC inputs.
            //
            if(get_pace_flag()){ 								// wait for pacing flag..
            	if(!get_stw_lock_strap()){						// only process ADC inputs if not locked
            		tt = morse_tone;							// pre-init variables to prevent random changes
            		ts = morse_speed;
            		tw = morse_weight;
                	ip = adc_buf;								// get ADC values
        			while(!adc_in(ip));							// failsafe to catch adc not ready
        			cws_buf[ai] = adc_buf[3];					// speed pot
        			cwt_buf[ai] = adc_buf[7];					// tone pot
                    if(++ai >= ADC_BUF_MAX){
                    	ai = 0;									// process pointer rollover
                    }
                    ei = 0;										// get running average
                    fi = 0;
                    for(i=0; i<ADC_BUF_MAX; i++){				// add up all the samples
                    	ei += (U32)cws_buf[i];
                    	fi += (U32)cwt_buf[i];
                    }
                    ts = ((U16)(ei/ADC_BUF_MAX)) >> 6; 			// divide by num samps to get average, then /64 to get raw result
                    if(get_weight_strap()){
                        tt = ((U16)(fi/ADC_BUF_MAX)) >> 3;  	// divide by num samps to get average, then /8 to get raw result
                        										// this gives about 4.54 Hz resolution for the tone, and 0.5 WPM for speed
                    }else{
                    	tw = ((U16)(fi/ADC_BUF_MAX)) >> 3;		// set morse weight (ADC/8 = 512 max, which is the % adjustment * 10)
                    }
    				// if changes, transfer settings to morse.c module
                    if(tt != morse_tone){
                    	morse_tone = tt;
                        tt = set_tone(morse_tone);
                        update_ee = TRUE;						// enable ee restore if locked later
#ifdef	DEBUG_U
                        UARTprintf("tone = %u Hz\n",tt);
#endif
                    }
                    if(ts != morse_speed){
                    	morse_speed = ts;
                        set_speed(morse_speed);
                        update_ee = TRUE;						// enable ee restore if locked later
#ifdef	DEBUG_U
            	        UARTprintf("speed = %u WPM\n",ts+5);
#endif
                    }
                    if(tw != morse_weight){
                    	morse_weight = tw;
                    	set_weight(morse_weight);
                        update_ee = TRUE;						// enable ee restore if locked later
                        si = (S32)morse_weight;
                        si -= 256;								// adjust to signed value
#ifdef	DEBUG_U
            	        UARTprintf("5 = %d %%\n",si);
#endif
                    }
                	adc_start();								// start ADC for next pass
            	}else{
            		if(update_ee){								// if enabled, recall from EEPROM
            			if(!get_stw_ee()){
            				init_stw();							// fault recover timing/tone settings
                	        UARTprintf("init_stw\n");
            			}
                        update_ee = FALSE;						// clear enable flag
            		}
            	}
            }
//******************************************************************************************************

//*************************  GET INPUT DATA FROM MORSE/KEYPAD  *****************************************
            //
            // Get Morse key-code data
            c = getchar_cw();

            // If no Morse char, get keypad data
            //	since Morse is a relatively low data rate source, this serves to simply
            //	merge the two data streams without conflict.  Priority is given to the
            //	Morse data, but because it is relatively low rate, but this shouldn't cause
            //  any noticeable issues since all data streams are buffered.
            if(!c && got_key()){
            	kcode = get_keycode();							// get keycode for keymap pgm mode (must call before get_key)
            	kc = get_key();
            	if(kc == KEYP_RELEASE){
                	kc = get_key();
                	kc = '\0';									// ignore key release for now
            	}
            }

            // process a keypad Morse Lock command IFF not keypad program enabled
            if((kc == MRSE_CWLOCK) && !get_pgm_keypad_enable()){
            	c = kc;
            }
//******************************************************************************************************

//**************************  PROCESS KEY-LOCK TOGGLE/PGM-ENAB  ****************************************
            // process key lock toggle & keypad program enable
            if((c == MRSE_CWLOCK)){
            	if(!lock_kybd){
                	set_led(SHFLK_LED, 1);						// make shift-lock LED blink if paddles locked
                	blink_led(SHFLK_LED, 1);
                	lock_kybd = 1;
            		put_cw('K');
            		put_cw('L');
            		kpgm_dirty = 0;
            	}else{
        			if(!get_pgm_keypad_enable()){
        				if(kpgm_dirty){
                    		put_cw('P');
                    		put_cw('G');
                    		put_cw('M');
                    		put_cw(' ');
                   			save_keymap();
                    		kpgm_dirty = 0;
                    		if(!kalt){
                    			set_led(ALTKYP_LED, 0);			// clear ALTKYP LED
                    		}
                			blink_led(ALTKYP_LED, 0);			// clear blink/flash ALTKYP LED
                			flash_led(ALTKYP_LED, 0);
        				}
            			lock_kybd = 0;
                		put_cw('O');
                		put_cw('K');
                    	flash_led(SHFLK_LED, 0);
                    	blink_led(SHFLK_LED, 0);
                		if(sticky_shift){						// restore shift lock LED status
                    		set_led(SHFLK_LED, 1);
                		}else{
                			set_led(SHFLK_LED, 0);
                		}
                		set_iambic_mode();						// update iambic mode on each unlock
        			}else{
                		put_cw('P');
                		put_cw('K');
                		put_cw('L');
        			}
            	}
            	c = '\0';
        	}
//******************************************************************************************************

//****************************  PROCESS KEYPAD PROGRAMMING  ********************************************
            // if paddles locked AND a character is valid AND pgm_keypad is enabled...
            //	execute the pgm keyboard state machine...
            //	While active, this state machine will consume all character and keypress
            //	events except paddle lock (MRSE_CWLOCK)
            if((c || kc) && lock_kybd && get_pgm_keypad_enable()){
            	if(c == USER_PS){
            		// store pc to eeprom (uses a fn call to keypad.c)
            		if(pc){
            			put_cw('U');
            			store_userps(pc);
            		}else{
            			// send current assignment
            			pc = get_userps();
            		}
            		if(pc){
            			put_cw_text(pc);						// send msg identifying the Morse chr programmed
            		}else{
            			put_cw('N');
            			put_cw('U');
            			put_cw('L');
            		}
            		pc = '\0';									// only one chr per pgm attempt
            		c = '\0';
            		kc = '\0';									// clear the astronomical race condition
            	}
            	if(c){
            		pc = c;
            	}
            	if(kc){
            		// store pc to keymap matrix (uses a fn call to keypad.c)
            		if(pc){
            			put_cw('S');
            			store_keycode(pc, kcode);
                		kpgm_dirty = 1;							// keymap is dirty, needs to be saved
            		}else{
            			// send current assignment
            			pc = kc;
            		}
            		put_cw_text(pc);							// send msg identifying the Morse chr programmed
        			set_led(ALTKYP_LED, 1);						// ALTKYP LED always on for program mode
            		if(kalt){
            			blink_led(ALTKYP_LED, 1);				// blink or flash ALTKYP LED if programming
            		}else{
            			flash_led(ALTKYP_LED, 1);
            		}
            		pc = '\0';									// only one chr per pgm attempt
            	}
            	c = '\0';										// consume all input here
            	kc = '\0';
            }
//******************************************************************************************************

//*************************  PROCESS CHARACTERS AND MODIFIERS  *****************************************
            // merge keypad and Morse data streams
            if(!c && kc){
            	c = kc;
            	kc = '\0';
            }

            // process store EE...IFF: cmd==store AND paddles locked AND eeprom is forced invalid
            if((c == MRSE_STOEE) && lock_kybd && get_stw_lock_strap()){
            	store_stw_ee();									// store STW values to eeprom
            	put_cw('S');									// acknowledgement
            	put_cw('T');
            	put_cw('R');
        		put_cw(' ');
            	c = '\0';										// clear cmd chr
            }

            // Process user pro-sign
            if(c == USER_PS){
            	c = get_userps();								// get user ps (null if invalid)
            }

            // Process SKS/PDL swapk
            if(c == MRSE_SKS){
            	if(get_paddle_mode_strap() == PADL_KEY){
                	paddle_force_set(FORCE_SKS);
            		put_cw('S');
            		put_cw('K');
            		put_cw('S');
            	}else{
                	paddle_force_set(FORCE_PDL);
            		put_cw('P');
            		put_cw('D');
            		put_cw('L');
            	}
        		put_cw(' ');
            	morse_init();									// reconfigure setting
            	c = '\0';
            }

			// Process wpm  itg
			if(c == MRSE_WPM){
				ts = morse_speed + 5;							// get speed in WPM
				i = (U8)(ts/10) + '0';							// get 10's
				if((i > '9') || (i < '0')){
					i = '?';									// unknown error
				}
				if(i != '0'){									// supress leading 0's
					put_cw(i);
				}
				i = (U8)(ts - ((U16)(i-'0') * 10)) + '0';		// get 1's
				if((i > '9') || (i < '0')){
					i = '?';									// unknown error
				}
				put_cw(i);
				put_cw(' ');
				put_cw('W');
				put_cw('P');
				put_cw('M');
				c = '\0';
			}

            // process characters to keyboard...
            if(c && !lock_kybd){
             	t++;
            	switch(c){
            	case MRSE_SHIFT:								// SHIFT KEY ***************************
            		// if already shift or sshift, clear
            		if((sticky_shift & HID_KEYB_LEFT_SHIFT) || (mod_mem & HID_KEYB_LEFT_SHIFT)){
                		sticky_shift &= ~HID_KEYB_LEFT_SHIFT;	// sticky shift off
                		mod_mem &= ~HID_KEYB_LEFT_SHIFT;		// shift off
                    	put_cw('N');
            			set_led(SHFLK_LED, 0);					// clear LED
                    	flash_led(SHFLK_LED, 0);
            		}else{
            			mod_mem = HID_KEYB_LEFT_SHIFT;
                    	put_cw('S');
            			set_led(SHFLK_LED, 1);					// flash SHFLK LED for just shift
                    	flash_led(SHFLK_LED, 1);
            		}
            		break;

            	case MRSE_SHLK:									// SHIFT-LOCK KEY **********************
            		sticky_shift ^= HID_KEYB_LEFT_SHIFT;		// toggle sticky shift
            		if(sticky_shift){
                		put_cw('S');
                		put_cw('L');
                		set_led(SHFLK_LED, 1);					// shift lock msg and set LED
            		}else{
            			put_cw('N');
            			put_cw('S');
            			set_led(SHFLK_LED, 0);					// no shift msg and clear LED
            		}
            		put_cw(' ');
            		break;

            	case MRSE_ALT:									// ALT KEY *****************************
            		if(mod_mem & HID_KEYB_LEFT_ALT){			// if already selected, kill it
                		mod_mem &= ~HID_KEYB_LEFT_ALT;
                		set_led(ALT_LED, 0);					// clear modifier LED
            			put_cw('N');
            		}else{
                		mod_mem |= HID_KEYB_LEFT_ALT;			// otherwise, set for ALT
                		set_led(ALT_LED, 1);					// set modifier LED
            			put_cw('A');
            		}
            		break;

            	case MRSE_CNTL:									// CNTL KEY ****************************
            		if(mod_mem & HID_KEYB_LEFT_CTRL){			// if already selected, kill it
                		mod_mem &= ~HID_KEYB_LEFT_CTRL;
                		set_led(CTRL_LED, 0);					// clear modifier LED
            			put_cw('N');
            		}else{
                		mod_mem |= HID_KEYB_LEFT_CTRL;			// otherwise, set for CTRL
                		set_led(CTRL_LED, 1);					// set modifier LED
            			put_cw('C');
            		}
            		break;

            	case MRSE_CTRLZ:								// CTRL-Z KEY **************************
            		if(get_ctrlz_strap()){						// if strap enabled...
            			mod_mem = 0;							// clear all previous modifiers
            			SendKey('Z', HID_KEYB_LEFT_CTRL);		// send CTRL-Z now
            			put_cw('C');
            			put_cw('Z');
                		put_cw(' ');
            		}
            		break;

            	case MRSE_WIN:									// WIN KEY *****************************
            		if(mod_mem & HID_KEYB_LEFT_GUI){			// if already selected, kill it
                		mod_mem &= ~HID_KEYB_LEFT_GUI;
                		set_led(WIN_LED, 0);					// clear modifier LED
            			put_cw('N');
            		}else{
                		mod_mem |= HID_KEYB_LEFT_GUI;			// otherwise, set for WIN
                		set_led(WIN_LED, 1);					// set modifier LED
            			put_cw('W');
            		}
            		break;

            	case MRSE_WINL:									// WIN-L KEY ***************************
            		mod_mem = 0;								// clear all the previous modifiers
                 	SendKey('L', HID_KEYB_LEFT_GUI);			//	and send the key now
        			put_cw('L');
            		break;

            	case MRSE_KPSWP:								// KEYPAD-SWAP KEY *********************
            		kalt += 0x01;								// advance keypad selection (supports multiple keypads)
            		if(kalt >= MAX_ALT_KP){						// rollover to 0 (only keypads 0 and 1 are currently supported)
            			kalt = 0;
            		}
            		if(kalt){									// update ALTKYP LED
            			set_led(ALTKYP_LED, 1);
            		}else{
            			set_led(ALTKYP_LED, 0);
            		}
            		set_kpalt(kalt);							// invoke the switch to new keypad
        			put_cw('K');								// ID the switch
        			put_cw('L');
        			put_cw('T');
        			if(kalt){									// id which keypad is now selected
            			put_cw('1');
        			}else{
            			put_cw('0');
        			}
            		put_cw(' ');
        			break;

            		case MRSE_REVRS:							// SWAP-PADDLE KEY *********************
            		if(swap_paddle()){
            			put_cw('S');							// "reversed" message
            			put_cw('W');
            			put_cw('P');
            		}else{
            			put_cw('N');							// "normal" message
            			put_cw('R');
            			put_cw('M');
            		}
            		put_cw(' ');
            		break;

            	case MRSE_WORDDEL:								// WORD-DEL KEY ************************
                 	SendKey(MRSE_DEL, HID_KEYB_RIGHT_CTRL);
            		put_cw('D');
            		break;

            	case MRSE_WORDBS:								// WORD-BS KEY *************************
                 	SendKey(MRSE_BACKCSP, HID_KEYB_RIGHT_CTRL);
            		put_cw('B');
            		break;

            	case 0 ... (' ' - 1):							// DISCARD INVALID KEYS ****************
					break;										//	We should never get here, but just in case...

            	default:
            		// if we get a "printable" character, we end up here...
            		//	Send character and active modifers to PC and then clear temp modifiers
            		if(c == MRSE_BACKCSP){
                		put_cw('E');        			    	// send a simple "beep" to ackn. the BS key/chr
            		}
            		if(c == MRSE_DEL){
                		put_cw('I');        			    	// send a simple "beep-beep" to ackn. the DEL key/chr
            		}
					mod_mem |= sticky_shift | sticky_cntl;
            		if((c >= ' ') && (c <= MRSE_ESC)){			// allow "actionable" charcters, discard all others (e.g., command chrs)
            			mod_mem |= g_ppi8KeyUsageCodes[c-' '][0];
                		if(c == MRSE_CAPLOCK){					// CAPLOCK is processed by PC, but we signal it here for the user
                    		put_cw('C');
                    		put_cw('L');
                    		put_cw(' ');
                		}
                     	SendKey(c, mod_mem);
                		mod_mem = 0;							// Clear-out temporary modifiers and update LEDs
                		if(!sticky_shift){
                			set_led(SHFLK_LED, 0);				// if no shftlk, make sure SHFLK LED is off
                			flash_led(SHFLK_LED, 0);
                		}
                		set_led(ALT_LED, 0);					// turn off modifier LEDs
                		set_led(CTRL_LED, 0);
                		set_led(WIN_LED, 0);
            		}
            		break;
            	}
            	buf[0] = c;										// debug: send pASCII keycode to UART
            	buf[1] = '\0';
        		if(c<0x7f){
                    UARTprintf(buf);							// "printable" key
            	}else{
            		buf[0] = '0';								// non-printing key, send as hex
            		buf[1] = 'x';								// (if we ever need this anywhere else, we'll make it a Fn...)
            		buf[2] = (c>>4) + '0';
            		if(buf[2] > '9') buf[2] += 7;
            		buf[3] = (c&0xf) + '0';
            		if(buf[3] > '9') buf[3] += 7;
            		buf[4] = '\0';
                    UARTprintf(buf);
            	}
            }
//******************************************************************************************************

//********************************* PROCESS USB SUSPEND ************************************************
            if(g_bSuspended){
            	USBDHIDKeyboardRemoteWakeupRequest((void *)&g_sKeyboardDevice);
            }
        }
	}
}
//******************************************************************************************************
// end of main()
