/********************************************************************
 ************ COPYRIGHT (c) 2019 by KE0FF, Taylor, TX   *************
 *
 *  File name: morse_lut.c
 *
 *  Module:    Data
 *
 *  Summary:   This is the morse keyboard fn (tone and element decode)
 *
 *  This file holds the look-up table (LUT) for the Morse characters supported
 *  by this application.
 *
 ********************************************************************/

#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include "inc/tm4c123gh6pm.h"
#include "typedef.h"
#include "init.h"
#include "morse.h"
#include "morse_lut.h"

	// Morse character patterns
	// "1" bits are DAHs, "0" bits are DITs
	// LSb is last element in character.  1st element is cw_len_map[] bits
	//	left of LSb
	// Max # elements (len_map) is 16.
	// Exception is SPACE_ELEM which is a unique ID that is not a reasonable Morse character
	//
	// Copied from spreadsheet
	// Character element pattern data
//								   <spc>
extern const U16 cw_elem_map[] = { 0xfa5f,
//  <spc>  !       "       #       $      %       &      '       (         )        *       +        ,       -       .       /
	0x0003,0x002B, 0x0012, 0x0047, 0x0019,0x0062, 0x0008,0x001E, 0x0016,   0x002D,  0x0005, 0x000A,  0x0033, 0x0021, 0x0015, 0x0012,
//  0      1       2       3       4      5       6      7       8         9        :       ;        <       =       >       ?
	0x001F,0x000F, 0x0007, 0x0003, 0x0001,0x0000, 0x0010,0x0018, 0x001C,   0x001E,  0x0038, 0x002A,  0x0090, 0x000D, 0x00D0, 0x000C,
//  @      A       B       C       D      E       F      G       H         I        J       K        L       M       N       O
	0x001A,0x0001, 0x0008, 0x000A, 0x0004,0x0000, 0x0002,0x0006, 0x0000,   0x0000,  0x0007, 0x0005,  0x0004, 0x0003, 0x0002, 0x0007,
//  P      Q       R       S       T      U       V      W       X         Y        Z       [        \       ]       ^       _
	0x0006,0x000D, 0x0002, 0x0000, 0x0001,0x0001, 0x0001,0x0003, 0x0009,   0x000B,  0x000C, 0x005A,  0x0011, 0x005B, 0x0029, 0x000D,
//	`      {       |       }       ~      <DEL>   <DEL>  <DEL>   <Aspc>    <shift>  <caplk> <pgup>   <pgdn>  <alt>   <cntl>  <winL>
	0x003C,0x002C, 0x0018, 0x0059, 0x0044,0x0001, 0x0001,0x0001, 0x0052,   0x0005,  0x00A4, 0x01B1,  0x01B4, 0x0029, 0x0154, 0x0034,
//	<win>  <F1>    <F2>    <F3>    <F4>   <F5>    <F6>   <F7>    <F8>      <F9>     <F10>   <F11>    <F12>   <upar>  <dnar>  <rtar>
	0x0032,0x004F, 0x0047, 0x0043, 0x0041,0x0040, 0x0050,0x0058, 0x005C,   0x005E,  0x008F, 0x0087,  0x0083, 0x002A, 0x006A, 0x008A,
// <lftar> <bcksp> <bcksp> <bcksp> <tab>  <enter> <esc>  <cwlok> <pdlrvrs> <wrddel> <wrdbs> <shftlk> <kpswp> <ctrlZ> <stoee> <usrps>
	0x004A,0x0000, 0x0000, 0x0000, 0x0058,0x0011, 0x000A,0x013D, 0x0012,   0x00D4,  0x006B, 0x0004,  0x0056, 0x015C, 0x000A, 0x0005,
//	<sks>  <wpm>
	0x0028,0x00db
	};

	// Number of elements in each character
//									   <spc>
extern const uint8_t cw_len_map[] =  { 254,
//  <spc>  !       "       #       $      %       &      '       (         )        *       +        ,       -       .       /
	4,     6,      6,      7,      8,     8,      5,     6,      5,        6,       6,      5,       6,      6,      6,      5,
//  0      1       2       3       4      5       6      7       8         9        :       ;        <       =       >       ?
	5,     5,      5,      5,      5,     5,      5,     5,      5,        5,       6,      6,       9,      5,      8,      6,
//  @      A       B       C       D      E       F      G       H         I        J       K        L       M       N       O
	6,     2,      4,      4,      3,     1,      4,     3,      4,        2,       4,      3,       4,      2,      2,      3,
//  P      Q       R       S       T      U       V      W       X         Y        Z       [        \       ]       ^       _
	4,     4,      3,      3,      1,     3,      4,     3,      4,        4,       4,      7,       9,      7,      6,      6,
//	`      {       |       }       ~      <DEL>   <DEL>  <DEL>   <Aspc>    <shift>  <caplk> <pgup>   <pgdn>  <alt>   <cntl>  <winL>
	7,     6,      6,      7,      7,     7,      8,     9,      7,        7,       8,      10,      10,     7,      9,      7,
//	<win>  <F1>    <F2>    <F3>    <F4>   <F5>    <F6>   <F7>    <F8>      <F9>     <F10>   <F11>    <F12>   <upar>  <dnar>  <rtar>
	7,     9,      9,      9,      9,     9,      9,     9,      9,        9,       10,     10,      10,     8,      8,      9,
// <lftar> <bcksp> <bcksp> <bcksp> <tab>  <enter> <esc>  <cwlok> <pdlrvrs> <wrddel> <wrdbs> <shftlk> <kpswp> <ctrlZ> <stoee> <usrps>
	8,     6,      7,      8,      7,     5,      8,     10,     8,        9,       8,      7,       7,      9,      7,      4,
//	<sks>  <wpm>
	9,     9
};

// ASCII code corresponding to each character entry

//								   <spc>
extern const char cw_text_map[] = { 32,
//<spc> !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /
	32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
//   0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
	48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63, 
//   @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
	64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
//   P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
	80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95, 
//	 `   {   |   }   ~     <DEL>       <Aspc>  <shift>  <caplk>  <pgup>  <pgdn>  <alt>  <cntl>  <winL>
	96,123,124,125,126,  127,127,127,  128,    129,     130,     131,    132,    133,   134,    135,
//	<win> <F1> <F2> <F3> <F4> <F5> <F6> <F7> <F8> <F9> <F10> <F11> <F12> <upar> <dnar> <rtar>
	 136,  137, 138, 139, 140, 141, 142, 143, 144, 145, 146,  147,  148,  149,   150,   151,
//	<lftar> <bcksp> <bcksp> <bcksp> <tab> <enter> <esc> <cwlok> <pdlrvrs> <wrddel> <wrdbs> <shftlk> <kpswp> <ctrlZ> <stoee> <usrps>
	 152,    153,    153,    153,    154,  155,    156,  157,    158,      159,     160,    161,     162,    163,    164,    165,
//	<sks>	 <wpm>
 	 166,    167
};

		// command/cntl key Morse response identifiers
		// matrix is accessed by [key code - MRSE_DEL][chr index[2:0]]
		// three characters per cmd, shorter responses padded with nulls
extern const char cw_text_msg[][CWTXT_LEN] = {

		{"DEL"},	// MRSE_DEL		(127)
		{"ASP"},	// CWSTT		(128)
		{"SFT"},	// MRSE_SHIFT	(129)
		{"CL\0"},	// MRSE_CAPLOCK	(130)
		{"PGU"},	// MRSE_PGUP	(131)
		{"PGD"},	// MRSE_PGDN	(132)
		{"ALT"},	// MRSE_ALT		(133)
		{"CTL"},	// MRSE_CNTL	(134)
		{"WNL"},	// MRSE_WINL	(135)
		{"WIN"},	// MRSE_WIN		(136)
		{"F1\0"},	// MRSE_F1		(137)
		{"F2\0"},	// MRSE_F2		(138)
		{"F3\0"},	// MRSE_F3		(139)
		{"F4\0"},	// MRSE_F4		(140)
		{"F5\0"},	// MRSE_F5		(141)
		{"F6\0"},	// MRSE_F6		(142)
		{"F7\0"},	// MRSE_F7		(143)
		{"F8\0"},	// MRSE_F8		(144)
		{"F9\0"},	// MRSE_F9		(145)
		{"F10"},	// MRSE_F10		(146)
		{"F11"},	// MRSE_F11		(147)
		{"F12"},	// MRSE_F12		(148)
		{"ARU"},	// MRSE_UP		(149)
		{"ARD"},	// MRSE_DN		(150)
		{"ARL"},	// MRSE_LEFT	(151)
		{"ARR"},	// MRSE_RIGHT	(152)
		{"BKS"},	// MRSE_BACKCSP	(153)
		{"TAB"},	// MRSE_TAB		(154)
		{"CR\0"},	// MRSE_CR		(155)
		{"ESC"},	// MRSE_ESC		(156)
		{"LOK"},	// MRSE_CWLOCK	(157)
		{"REV"},	// MRSE_REVRS	(158)
		{"WDL"},	// MRSE_WORDDEL	(159)
		{"WBS"},	// MRSE_WORDBS	(160)
		{"SLK"},	// MRSE_SHLK	(161)
		{"KSP"},	// MRSE_KPSWP	(162)
		{"CTZ"},	// MRSE_CTRLZ	(163)
		{"STR"},	// MRSE_STOEE	(164)
		{"USR"},	// MRSE_USRPS	(165)
		{"SKS"},	// MRSE_SKS		(166)
		{"WPM"}		// MRSE_WPM		(167)
};

//-----------------------------------------------------------------------------
// sizeof_len_map provides size of array to external functions
//-----------------------------------------------------------------------------
U8 sizeof_len_map(void)
{
	return sizeof(cw_len_map);	// s = size of map array
}

