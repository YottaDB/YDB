/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMSOURCE_HEARTBEAT_H
#define GTMSOURCE_HEARTBEAT_H

#define gtmsource_stall_heartbeat	heartbeat_stalled = TRUE;
#define gtmsource_restart_heartbeat	heartbeat_stalled = FALSE;
#define gtmsource_is_heartbeat_stalled	(heartbeat_stalled)
#ifndef REPL_DISABLE_HEARTBEAT
#define gtmsource_is_heartbeat_due(now)												\
	(0 != last_sent_time													\
	 && difftime(*(now), last_sent_time) >= (double)jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD])
#else
#define gtmsource_is_heartbeat_due(now) FALSE
#endif

GBLREF	boolean_t			heartbeat_stalled;
GBLREF	repl_heartbeat_que_entry_t	*repl_heartbeat_que_head;
GBLREF	repl_heartbeat_que_entry_t	*repl_heartbeat_free_head;
GBLREF	time_t				last_sent_time;
GBLREF	time_t				earliest_sent_time;

void gtmsource_heartbeat_timer(TID tid, int4 interval_len, int *interval_ptr);

#endif /* GTMSOURCE_HEARTBEAT_H */
