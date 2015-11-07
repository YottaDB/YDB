/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define ASCII_ESC 		27	/*	this ASCII value is needed on any platform	*/
#define EBCDIC_ESC		39
#define ASCII_CR		13
#define EBCDIC_CR		13
#define ASCII_LF		10
#define EBCDIC_LF		37
#define ASCII_FF		12
#define EBCDIC_FF		12
#define ASCII_BS		8
#define EBCDIC_BS		22
#define VT			11

#define ASCII_TTEOL		"\015\012"
#define EBCDIC_TTEOL 		"\025"

#define ESC			ASCII_ESC
#define NATIVE_ESC		ASCII_ESC
#define NATIVE_CR		ASCII_CR
#define NATIVE_LF		ASCII_LF
#define NATIVE_FF		ASCII_FF
#define NATIVE_BS		ASCII_BS
#define NATIVE_TTEOL		((ascii != io_ptr->out_code_set) ? EBCDIC_TTEOL : ASCII_TTEOL)
#define NATIVE_VT		VT
