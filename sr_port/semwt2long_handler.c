/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "semwt2long_handler.h"


void semwt2long_handler(void)
{

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(semwait2long) = TRUE;
	return;
}
