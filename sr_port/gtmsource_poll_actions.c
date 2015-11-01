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

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_string.h"
#include "gtm_unistd.h"

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
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
#ifdef UNIX
#include "gtmio.h"
#endif
#include "sgtm_putmsg.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	int			gtmsource_sock_fd;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_log_fd;
GBLREF 	FILE			*gtmsource_log_fp;
GBLREF	int			gtmsource_statslog_fd;
GBLREF 	FILE			*gtmsource_statslog_fp;
GBLREF	int			gtmsource_filter;
GBLREF	volatile time_t		gtmsource_now;

int gtmsource_poll_actions(boolean_t poll_secondary)
{
	/* This function should be called only in active mode, but cannot assert for it */

	gtmsource_local_ptr_t	gtmsource_local;
	time_t			now;
	repl_heartbeat_msg_t	overdue_heartbeat;
	char			seq_num_str[32], *seq_num_ptr;
	char			*time_ptr;
	char			time_str[CTIME_BEFORE_NL + 1];
	char			print_msg[1024], msg_str[1024];
	int			status;
	error_def(ERR_REPLWARN);

	gtmsource_local = jnlpool.gtmsource_local;
	if (SHUTDOWN == gtmsource_local->shutdown)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Shutdown signalled\n");
		gtmsource_end(); /* Won't return */
	}
	if (GTMSOURCE_CHANGING_MODE !=  gtmsource_state && GTMSOURCE_MODE_PASSIVE == gtmsource_local->mode)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Changing mode from ACTIVE to PASSIVE\n");
		gtmsource_state = GTMSOURCE_CHANGING_MODE;
		return (SS_NORMAL);
	}

	if (poll_secondary && GTMSOURCE_CHANGING_MODE != gtmsource_state && GTMSOURCE_WAITING_FOR_CONNECTION != gtmsource_state)
	{
		now = gtmsource_now;
		if (gtmsource_is_heartbeat_overdue(&now, &overdue_heartbeat))
		{
			time_ptr = GTM_CTIME((time_t *)&overdue_heartbeat.ack_time[0]);
			memcpy(time_str, time_ptr, CTIME_BEFORE_NL);
			time_str[CTIME_BEFORE_NL] = '\0';
			SPRINTF(msg_str, "No response received for heartbeat sent at %s with SEQNO %llu in %00.f seconds. "
					"Closing connection\n", time_str, *(seq_num *)&overdue_heartbeat.ack_seqno[0],
				 	difftime(now, *(time_t *)&overdue_heartbeat.ack_time[0]));
			sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_STR(msg_str));
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
			repl_close(&gtmsource_sock_fd);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
			gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
			return (SS_NORMAL);
		}

		if (gtmsource_is_heartbeat_due(&now) && !gtmsource_is_heartbeat_stalled)
		{
			gtmsource_send_heartbeat(&now);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state ||
		    	    GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
		}
	}

	if (gtmsource_local->changelog)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Changing log file to %s\n", gtmsource_local->log_file);
#ifdef UNIX
		repl_log_init(REPL_GENERAL_LOG, &gtmsource_log_fd, NULL, gtmsource_local->log_file, NULL);
		repl_log_fd2fp(&gtmsource_log_fp, gtmsource_log_fd);
#elif defined(VMS)
		util_log_open(STR_AND_LEN(gtmsource_local->log_file));
#else
#error unsupported platform
#endif
		gtmsource_local->changelog = FALSE;
	}
	if (!gtmsource_logstats && gtmsource_local->statslog)
	{
#ifdef UNIX
		gtmsource_logstats = TRUE;
		repl_log_init(REPL_STATISTICS_LOG, &gtmsource_log_fd, &gtmsource_statslog_fd, gtmsource_local->log_file,
			      gtmsource_local->statslog_file);
		repl_log_fd2fp(&gtmsource_statslog_fp, gtmsource_statslog_fd);
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Starting stats log to %s\n", gtmsource_local->statslog_file);
		repl_log(gtmsource_statslog_fp, TRUE, TRUE, "Begin statistics logging\n");
#else
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Stats logging not supported on VMS\n");
#endif

	} else if (gtmsource_logstats && !gtmsource_local->statslog)
	{
		gtmsource_logstats = FALSE;
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Stopping stats log\n");
		/* Force all data out to the file before closing the file */
		repl_log(gtmsource_statslog_fp, TRUE, TRUE, "End statistics logging\n");
		UNIX_ONLY(CLOSEFILE(gtmsource_statslog_fd, status);) VMS_ONLY(close(gtmsource_statslog_fd);)
		gtmsource_statslog_fd = -1;
		/* We need to FCLOSE because a later open() in repl_log_init() might return the same file descriptor as the one
		 * that we just closed. In that case, FCLOSE done in repl_log_fd2fp() affects the newly opened file and
		 * FDOPEN will fail returning NULL for the file pointer. So, we close both the file descriptor and file pointer.
		 * Note the same problem does not occur with GENERAL LOG because the current log is kept open while opening
		 * the new log and hence the new file descriptor will be different (we keep the old log file open in case there
		 * are errors during DUPing. In such a case, we do not switch the log file, but keep the current one).
		 * We can FCLOSE the old file pointer later in repl_log_fd2fp() */
		FCLOSE(gtmsource_statslog_fp, status);
		gtmsource_statslog_fp = NULL;
	}
	if ((gtmsource_filter & EXTERNAL_FILTER) && ('\0' == gtmsource_local->filter_cmd[0]))
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Stopping filter\n");
		repl_stop_filter();
		gtmsource_filter &= ~EXTERNAL_FILTER;
	}
	return (SS_NORMAL);
}
