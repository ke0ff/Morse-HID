/********************************************************************
 ************ COPYRIGHT (c) 2019 by KE0FF, Taylor, TX   *************
 *
 *  File name: morse_lut.h
 *
 *  Module:    Control
 *
 *  Summary:   This is the morse keyboard function header file
 *
 *******************************************************************/

/********************************************************************
 *  File scope declarations revision history:
 *    04-02-19 jmh:  creation date
 *
 *******************************************************************/

#ifndef MORSE_LUT_H_
#define MORSE_LUT_H_

//Auto-space command character element pattern and length
#define	CWSTT_E 0x0052										// auto-space mode toggle, element pattern
#define	CWSTT_L 7											// auto-space mode toggle, element count
#define	CWSTT	128											// This identifies the auto-space toggle pro-sign command
#define	USER_PS	165											// identifies the user-defined prosign

// pseudo-ASCII codes for special add-on Morse key-characters
#define	CWTXT_LEN		3									// length (max) of Morse text response msg

#define	MRSE_DEL		(127)
#define	MRSE_SHIFT		(129)
#define	MRSE_CAPLOCK	(130)
#define	MRSE_PGUP		(131)
#define	MRSE_PGDN		(132)
#define	MRSE_ALT		(133)
#define	MRSE_CNTL		(134)
#define	MRSE_WINL		(135)
#define	MRSE_WIN		(136)
#define	MRSE_F1			(137)
#define	MRSE_F2			(138)
#define	MRSE_F3			(139)
#define	MRSE_F4			(140)
#define	MRSE_F5			(141)
#define	MRSE_F6			(142)
#define	MRSE_F7			(143)
#define	MRSE_F8			(144)
#define	MRSE_F9			(145)
#define	MRSE_F10		(146)
#define	MRSE_F11		(147)
#define	MRSE_F12		(148)
#define	MRSE_UP			(149)
#define	MRSE_DN			(150)
#define	MRSE_LEFT		(151)
#define	MRSE_RIGHT		(152)
#define	MRSE_BACKCSP	(153)
#define	MRSE_TAB		(154)
#define	MRSE_CR			(155)
#define	MRSE_ESC		(156)		// end of "actionable" characters
#define	MRSE_CWLOCK		(157)		// cmd chrs follow
#define	MRSE_REVRS		(158)
#define	MRSE_WORDDEL	(159)
#define	MRSE_WORDBS		(160)
#define	MRSE_SHLK		(161)
#define	MRSE_KPSWP		(162)
#define	MRSE_CTRLZ		(163)
#define	MRSE_STOEE		(164)		// store EEPROM
#define	MRSE_USRPS		(165)		// User programmable pro-sign
#define	MRSE_SKS		(166)		// SKS/PDL swap pro-sign
#define	MRSE_WPM		(167)		// wpm display
#define	LAST_KEY		(167)

#define	KEYP_RELEASE	(255)

#define	ELEM_BS			(0)
#define	LENGTH_BS6		(6)
#define	LENGTH_BS8		(8)

//-----------------------------------------------------------------------------
// Fn prototype declarations
//-----------------------------------------------------------------------------

U8 sizeof_len_map(void);

//-----------------------------------------------------------------------------
// End of file
//-----------------------------------------------------------------------------
#endif
