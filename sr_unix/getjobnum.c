/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include "getjobnum.h"

GBLREF	uint4	process_id;

void getjobnum(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	process_id = getpid();
	TREF(user_id) = (uint4)getuid();
}
