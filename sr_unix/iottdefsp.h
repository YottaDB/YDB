/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif


#define ASCII_TTEOL		"\012"
#define EBCDIC_TTEOL 		"\025"

#define ESC			ASCII_ESC
#define NATIVE_ESC		ASCII_ESC
#define NATIVE_CR		ASCII_CR
#define NATIVE_LF		ASCII_LF
#define NATIVE_FF		ASCII_FF
#define NATIVE_BS		ASCII_BS
#define NATIVE_VT		VT
#ifdef KEEP_zOS_EBCDIC
#define NATIVE_TTEOL		((ascii != io_ptr->out_code_set) ? EBCDIC_TTEOL : ASCII_TTEOL)
#else
#define NATIVE_TTEOL		ASCII_TTEOL
#endif

/* Editing control characters */
#define CTRL_A	'\001'
#define CTRL_B	'\002'
#define CTRL_D	'\004'
#define CTRL_E	'\005'
#define CTRL_F	'\006'
#define CTRL_K	'\013'
#define CTRL_U	'\025'

#define EDIT_SOL	CTRL_A
#define EDIT_LEFT	CTRL_B
#define EDIT_DELETE	CTRL_D
#define EDIT_EOL	CTRL_E
#define EDIT_RIGHT	CTRL_F
#define EDIT_DEOL	CTRL_K
#define EDIT_ERASE	CTRL_U
