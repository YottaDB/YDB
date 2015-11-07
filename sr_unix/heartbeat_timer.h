/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _HEARTBEAT_TIMER_H
#define _HEARTBEAT_TIMER_H

#define HEARTBEAT_INTERVAL_IN_SECS 		8	/* Need to maintain this in sync with HEARTBEAT_INTERVAL. To avoid
							 * division by 1000 wherever needed, the value is hardcoded to be 8
							 */
/* define heartbeat interval */
#define HEARTBEAT_INTERVAL 			(HEARTBEAT_INTERVAL_IN_SECS * MILLISECS_IN_SEC) /* ms */
#define	NUM_HEARTBEATS_FOR_OLDERJNL_CHECK	8	/* gives a total of 8 * 8 = 64 seconds between checks of older jnl files */

GBLREF boolean_t heartbeat_started;

/* The heartbeat timer is used
 * 	1) To periodically check if we have older generation journal files open and if so to close them.
 *	2) By mutex logic to approximately measure the time spent sleeping while waiting for CRIT or MSEMLOCK.
 * Linux currently does not support MSEMs. It uses the heartbeat timer only for (1).
 */
#define START_HEARTBEAT_IF_NEEDED									\
{													\
	if (!heartbeat_started)										\
	{												\
		start_timer((TID)&heartbeat_timer, HEARTBEAT_INTERVAL, heartbeat_timer, 0, NULL);	\
		heartbeat_started = TRUE; /* should always be set AFTER start_timer */			\
	}												\
}

void heartbeat_timer(void);

#endif

#ifdef DEBUG
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
void set_enospc_if_needed(void);
void choose_random_reg_list(char *enospc_enable_list, int);
void set_enospc_flags(gd_addr *addr_ptr, char enospc_enable_list[], boolean_t ok_to_interrupt);
#endif
