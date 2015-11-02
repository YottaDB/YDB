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

#include "mdef.h"
#include "gtm_unistd.h"
#include "getjobnum.h"

GBLDEF	uint4	user_id;

GBLREF	uint4	process_id;
GBLREF	uint4	image_count;	/* not used in UNIX but defined to preserve VMS compatibility */

void getjobnum(void)
{
	process_id = getpid();
	user_id = (uint4)getuid();
}
