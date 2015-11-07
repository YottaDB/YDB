/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
GBLREF	uid_t	user_id;

void getjobnum(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	process_id = getpid();
	user_id = (uint4)getuid();
}
