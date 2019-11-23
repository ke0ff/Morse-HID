/********************************************************************
 ************ COPYRIGHT (c) 2019 by ke0ff, Taylor, TX   *************
 *
 *  File name: version.h
 *
 *  Module:    Control
 *
 *  Summary:   This header contains the software version number
 *             as a character string.
 *
 *******************************************************************/


/********************************************************************
 *  File scope declarations revision history:
 *    03-27-19 jmh:  0.1 First field release
 *    04-12-19 jmh:  0.2 Modified to support Morse Keyboard
 *
 *******************************************************************/

#define	VERSION_CHARACTER '6'				// supports serial and Morse output channels

#ifdef VERSOURCE
const S8    version_number[] = { '0', '.', VERSION_CHARACTER, '\0' };
const S8    date_code[]      = {"23-Aug-2019"};
#endif

//-----------------------------------------------------------------------------
// Public Fn Declarations
//-----------------------------------------------------------------------------
void dispSWvers(void);

#define VERSION_INCLUDED
