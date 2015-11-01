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

#ifndef _DSKSPACE_MSG_TIMER_H
#define _DSKSPACE_MSG_TIMER_H

/* define heartbeat interval */
#define DSKSPACE_MSG_INTERVAL 	60 * 1000 /* one minute */

void dskspace_msg_timer(void);

#endif
