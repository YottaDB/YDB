/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __MVAL2FAO_H__
#define __MVAL2FAO_H__

int mval2fao(char *message, va_list pfao, UINTPTR_T *outparm, int mcount, int fcount, char *bufbase,
	char *buftop);

#endif
