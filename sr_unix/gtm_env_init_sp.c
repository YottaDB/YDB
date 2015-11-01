/****************************************************************
 *								*
 *	Copyright 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_logicals.h"
#include "trans_numeric.h"
#include "gtm_env_init.h"	/* for gtm_env_init() and gtm_env_init_sp() prototype */

GBLREF	int4	gtm_shmflags;	/* Shared memory flags for shmat() */

/* Unix only environment initializations */
void	gtm_env_init_sp(void)
{
	mstr		val;
	boolean_t	dummy;

	val.addr = GTM_SHMFLAGS;
	val.len = sizeof(GTM_SHMFLAGS) - 1;
	gtm_shmflags = (int4)trans_numeric(&val, &dummy, TRUE);	/* Flags vlaue (0 is undefined or bad) */
}
