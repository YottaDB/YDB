/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_string.h"
#include "gtm_unistd.h"

#include "gtm_inet.h"
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "repl_log.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "gt_timer.h"
#include "gtmsource_heartbeat.h"
#include "jnl.h"
#include "repl_filter.h"
#include "util.h"
#include "repl_comm.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "sgtm_putmsg.h"
#include "copy.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	int			gtmsource_sock_fd;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_log_fd;
GBLREF 	FILE			*gtmsource_log_fp;
GBLREF	int			gtmsource_filter;
GBLREF	volatile time_t		gtmsource_now;
GBLREF	uint4			log_interval;
#ifdef UNIX
GBLREF	boolean_t		last_seen_freeze_flag;
#endif
error_def(ERR_REPLWARN);
#ifdef UNIX
error_def(ERR_REPLINSTFREEZECOMMENT);
error_def(ERR_REPLINSTFROZEN);
error_def(ERR_REPLINSTUNFROZEN);
#endif

int gtmsource_poll_actions(boolean_t poll_secondary)
{
	/* This function should be called only in active mode, but cannot assert for it */

	gtmsource_local_ptr_t	gtmsource_local;
	time_t			now;
	repl_heartbeat_msg_t	overdue_heartbeat;
	char			*time_ptr;
	char			time_str[CTIME_BEFORE_NL + 1];
	char			print_msg[1024], msg_str[1024];
	boolean_t 		log_switched = FALSE;
	int			status;
	time_t			temp_time;
	gtm_time4_t		time4;

	gtmsource_local = jnlpool.gtmsource_local;
	if (SHUTDOWN == gtmsource_local->shutdown)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Shutdown signalled\n");
		gtmsource_end(); /* Won't return */
	}
#	ifdef UNIX
	if (jnlpool.jnlpool_ctl->freeze != last_seen_freeze_flag)
	{
		last_seen_freeze_flag = jnlpool.jnlpool_ctl->freeze;
		if (last_seen_freeze_flag)
		{
			sgtm_putmsg(print_msg, VARLSTCNT(3) ERR_REPLINSTFROZEN, 1,
					jnlpool.repl_inst_filehdr->inst_info.this_instname);
			repl_log(gtmsource_log_fp, TRUE, FALSE, print_msg);
			sgtm_putmsg(print_msg, VARLSTCNT(3) ERR_REPLINSTFREEZECOMMENT, 1, jnlpool.jnlpool_ctl->freeze_comment);
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
		}
		else
		{
			sgtm_putmsg(print_msg, VARLSTCNT(3) ERR_REPLINSTUNFROZEN, 1,
					jnlpool.repl_inst_filehdr->inst_info.this_instname);
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
		}
	}
#	endif
	if (GTMSOURCE_START == gtmsource_state)
		return (SS_NORMAL);
	if (GTMSOURCE_CHANGING_MODE != gtmsource_state && GTMSOURCE_MODE_PASSIVE_REQUESTED == gtmsource_local->mode)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Changing mode from ACTIVE to PASSIVE\n");
		gtmsource_state = GTMSOURCE_CHANGING_MODE;
		gtmsource_local->mode = GTMSOURCE_MODE_PASSIVE;
		UNIX_ONLY(gtmsource_local->gtmsource_state = gtmsource_state;)
		return (SS_NORMAL);
	}
	if (poll_secondary && GTMSOURCE_CHANGING_MODE != gtmsource_state && GTMSOURCE_WAITING_FOR_CONNECTION != gtmsource_state)
	{
		now = gtmsource_now;
		if (gtmsource_is_heartbeat_overdue(&now, &overdue_heartbeat))
		{
			/* Few platforms don't allow unaligned memory access. Passing ack_time to GTM_CTIME(ctime) may
			 * cause sig. time4 and temp_time are used as temporary variable for converting time to string.*/
			GET_LONG(time4, &overdue_heartbeat.ack_time[0]);
			temp_time = time4;
			GTM_CTIME(time_ptr, &temp_time);
			memcpy(time_str, time_ptr, CTIME_BEFORE_NL);
			time_str[CTIME_BEFORE_NL] = '\0';
			VMS_ONLY(SPRINTF(msg_str, "No response received for heartbeat sent at %s with SEQNO %llu in %0.f seconds. "
					"Closing connection\n", time_str, *(seq_num *)&overdue_heartbeat.ack_seqno[0],
				 	difftime(now, temp_time)));
			NON_GTM64_ONLY(SPRINTF(msg_str,
					"No response received for heartbeat sent at %s with SEQNO %llu in %0.f seconds. "
					"Closing connection\n", time_str, *(seq_num *)&overdue_heartbeat.ack_seqno[0],
				 	difftime(now, temp_time)));
			GTM64_ONLY(SPRINTF(msg_str, "No response received for heartbeat sent at %s with SEQNO %lu in %0.f seconds. "
					"Closing connection\n", time_str, *(seq_num *)&overdue_heartbeat.ack_seqno[0],
				 	difftime(now, temp_time)));
			sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_STR(msg_str));
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
			repl_close(&gtmsource_sock_fd);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
			gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
			UNIX_ONLY(gtmsource_local->gtmsource_state = gtmsource_state;)
			return (SS_NORMAL);
		}

		if (GTMSOURCE_IS_HEARTBEAT_DUE(&now) && !heartbeat_stalled)
		{
			gtmsource_send_heartbeat(&now);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state ||
		    	    GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
		}
	}
	if (0 != gtmsource_local->changelog)
	{
		if (gtmsource_local->changelog & REPLIC_CHANGE_LOGINTERVAL)
		{
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Changing log interval from %u to %u\n",
					log_interval, gtmsource_local->log_interval);
			log_interval = gtmsource_local->log_interval;
			gtmsource_reinit_logseqno(); /* will force a LOG on the first send following the interval change */
		}
		if (gtmsource_local->changelog & REPLIC_CHANGE_LOGFILE)
		{
			log_switched = TRUE;
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Changing log file to %s\n", gtmsource_local->log_file);
#ifdef UNIX
			repl_log_init(REPL_GENERAL_LOG, &gtmsource_log_fd, gtmsource_local->log_file);
			repl_log_fd2fp(&gtmsource_log_fp, gtmsource_log_fd);
#elif defined(VMS)
			util_log_open(STR_AND_LEN(gtmsource_local->log_file));
#else
#error unsupported platform
#endif
		}
	        if ( log_switched == TRUE )
        	        repl_log(gtmsource_log_fp, TRUE, TRUE, "Change log to %s successful\n", gtmsource_local->log_file);
		gtmsource_local->changelog = 0;
	}
	if (!gtmsource_logstats && gtmsource_local->statslog)
	{
#ifdef UNIX
		gtmsource_logstats = TRUE;
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Begin statistics logging\n");
#else
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Stats logging not supported on VMS\n");
#endif

	} else if (gtmsource_logstats && !gtmsource_local->statslog)
	{
		gtmsource_logstats = FALSE;
		repl_log(gtmsource_log_fp, TRUE, TRUE, "End statistics logging\n");
	}
	if ((gtmsource_filter & EXTERNAL_FILTER) && ('\0' == gtmsource_local->filter_cmd[0]))
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Stopping filter\n");
		repl_stop_filter();
		gtmsource_filter &= ~EXTERNAL_FILTER;
	}
	return (SS_NORMAL);
}
