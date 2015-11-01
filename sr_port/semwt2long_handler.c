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

#include "mdef.h"
#include "semwt2long_handler.h"

GBLREF volatile boolean_t semwt2long;

void semwt2long_handler(void)
{
	semwt2long = TRUE;
	return;
}
