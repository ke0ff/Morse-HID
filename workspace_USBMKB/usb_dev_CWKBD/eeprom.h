/********************************************************************
 ************ COPYRIGHT (c) 2019 by ke0ff, Taylor, TX   *************
 *
 *  File name: eeprom.h
 *
 *  Module:    Control
 *
 *  Summary:   defines and global declarations for eeprom.c
 *
 *  Project scope revision history:
 *    03-23-15 jmh:  creation date
 *
 *******************************************************************/

#include "typedef.h"
#include <stdint.h>
#include "init.h"

#ifndef EEPROM_H
#define EEPROM_H
#endif

//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------
#define	EE_FAIL		0xff								// fail return value
#define	EE_OK		0x00								// OK return value
#define	EE_BLOCK	16									// # words/block

// eeprom address constants
// keypads
#define	KEYP_EEBASE_ADDR	0							// keypad map
#define	KEYP_CHECKSUM_ADDR	(2 * KEYP_ROW * KEYP_COL / 4)	// keypad map checksum

// speed/tone/weight
#define	SPEED_EEADDR	(KEYP_CHECKSUM_ADDR + 1)
#define	TONE_EEADDR		(KEYP_CHECKSUM_ADDR + 2)
#define	WEIGHT_EEADDR	(KEYP_CHECKSUM_ADDR + 3)

// LED brightness
#define	LEDBRT_EEADDR	(KEYP_CHECKSUM_ADDR + 4)
// leave a spare

// user prosign address
#define	USRPS_EEADDR	(KEYP_CHECKSUM_ADDR + 6)
//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Fn prototypes
//-----------------------------------------------------------------------------
U16 eeprom_init(void);
U32 eerd(U16 addr);
U8  eewr(U16 addr, U32 data);

//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------
