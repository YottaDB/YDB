/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _HEARTBEAT_TIMER_H
#define _HEARTBEAT_TIMER_H

/* define heartbeat interval */
#define HEARTBEAT_INTERVAL 			8 * 1000 /* ms */
#define	NUM_HEARTBEATS_FOR_OLDERJNL_CHECK	8	/* gives a total of 8 * 8 = 64 seconds between checks of older jnl files */

void heartbeat_timer(void);

#endif
