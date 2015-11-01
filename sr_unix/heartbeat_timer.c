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
#ifdef MUTEX_MSEM_WAKE
#include "heartbeat_timer.h"
#include "gt_timer.h"

GBLREF volatile uint4 heartbeat_counter;

void heartbeat_timer(void)
{
	/* It will take heartbeat_counter about 1014 years to overflow. */
	heartbeat_counter++;
	start_timer((TID)heartbeat_timer, HEARTBEAT_INTERVAL, heartbeat_timer, 0, NULL);
}

#endif /* MUTEX_MSEM_WAKE */
