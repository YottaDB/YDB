/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MVAL2FAO_H_INCLUDED
#define MVAL2FAO_H_INCLUDED

int mval2fao(char *message, va_list pfao, UINTPTR_T *outparm, int mcount, int fcount, char *bufbase,
	char *buftop);

#endif
