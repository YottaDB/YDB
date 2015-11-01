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

#ifdef UNIX
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#elif defined(VMS)
#include <ssdef.h>
#include <psldef.h>
#include <descrip.h> /* Required for gtmsource.h */
#else
#error Unsupported platform
#endif
#include <errno.h>
#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#include "gtm_string.h"

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
#include "gtm_stdio.h"
#include "gtm_event_log.h"
#include "repl_shutdcode.h"
#include "eintr_wrappers.h"
#include "jnl.h"
#include "repl_filter.h"
#include "repl_sem.h"
#ifdef VMS
#include "repl_shm.h"
#endif
#ifdef UNIX
#include "mutex.h"
#endif
#include "repl_log.h"

GBLREF jnlpool_addrs 	jnlpool;
GBLREF jnlpool_ctl_ptr_t jnlpool_ctl;
GBLREF uint4		process_id;
GBLREF int		gtmsource_sock_fd;
GBLREF int		gtmsource_log_fd;
GBLREF FILE		*gtmsource_log_fp;
GBLREF int		gtmsource_filter;
GBLREF boolean_t	gtmsource_logstats;
GBLREF int		gtmsource_statslog_fd;
GBLREF FILE		*gtmsource_statslog_fp;
GBLREF unsigned char	*gtmsource_tcombuff_start;
GBLREF long		repl_source_data_sent;
GBLREF long		repl_source_msg_sent;
GBLREF seq_num		seq_num_zero;
GBLREF repl_msg_ptr_t	gtmsource_msgp;
GBLREF uchar_ptr_t	repl_filter_buff;
GBLREF boolean_t	pool_init;

int gtmsource_end1(boolean_t auto_shutdown)
{
	int		exit_status;
	seq_num		log_seqno, log_seqno1, diff_seqno;
	int		fclose_res;
	unsigned char	seq_num_str[32], *seq_num_ptr;
#ifdef VMS
	int4		status;
#endif

	gtmsource_ctl_close();
	rel_lock(jnlpool.jnlpool_dummy_reg);
	UNIX_ONLY(mutex_cleanup(jnlpool.jnlpool_dummy_reg);)
	exit_status = NORMAL_SHUTDOWN;
	if (!auto_shutdown)
		jnlpool.gtmsource_local->shutdown = NORMAL_SHUTDOWN;
	QWASSIGN(log_seqno, jnlpool.gtmsource_local->read_jnl_seqno);
	QWASSIGN(log_seqno1, jnlpool.jnlpool_ctl->jnl_seqno);
	jnlpool.gtmsource_local->gtmsource_pid = 0;
	/* Detach from journal pool */
	UNIX_ONLY(
		if (jnlpool.jnlpool_ctl && 0 > SHMDT(jnlpool.jnlpool_ctl))
			repl_log(gtmsource_log_fp, FALSE, TRUE, "Error detaching from journal pool : %s\n", REPL_STR_ERROR);
	)
	VMS_ONLY(
		if (jnlpool.jnlpool_ctl)
		{
			if (SS$_NORMAL != (status = detach_shm(jnlpool.shm_range)))
				repl_log(stderr, TRUE, TRUE, "Error detaching from jnlpool : %s\n", REPL_STR_ERROR);
			jnlpool.jnlpool_ctl = NULL;
			if (!auto_shutdown && (SS$_NORMAL != (status = signoff_from_gsec(jnlpool.shm_lockid))))
				repl_log(stderr, TRUE, TRUE, "Error dequeueing lock on jnlpool global section : %s\n",
														REPL_STR_ERROR);
		}
	)
	jnlpool.jnlpool_ctl = jnlpool_ctl = NULL;
	pool_init = FALSE;
	if (gtmsource_msgp)
		free(gtmsource_msgp);
	if (gtmsource_tcombuff_start)
		free(gtmsource_tcombuff_start);
	if (repl_filter_buff)
		free(repl_filter_buff);
	gtmsource_stop_heartbeat();
	if (-1 != gtmsource_sock_fd)
		close(gtmsource_sock_fd); /* Close the conn with Receiver */
	if (QWNE(log_seqno, seq_num_zero))
		QWDECRBYDW(log_seqno, 1);
	if (QWNE(log_seqno1, seq_num_zero))
		QWDECRBYDW(log_seqno1, 1);
	QWSUB(diff_seqno, log_seqno1, log_seqno);
	repl_log(gtmsource_log_fp, TRUE, FALSE, "REPL INFO - Last written tr num into jnlpool : "INT8_FMT, INT8_PRINT(log_seqno1));
	repl_log(gtmsource_log_fp, FALSE, FALSE, "  Last sent tr num : "INT8_FMT, INT8_PRINT(log_seqno));
	repl_log(gtmsource_log_fp, FALSE, TRUE, "  Number of unsent tr : "INT8_FMT"\n", INT8_PRINT(diff_seqno));
	repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO - Tr Total : %ld  Msg Total : %ld\n",
		 repl_source_data_sent, repl_source_msg_sent);
	if (gtmsource_filter & EXTERNAL_FILTER)
		repl_stop_filter();
	gtm_event_log_close();
	if (auto_shutdown)
		return (exit_status);
	else
		gtmsource_exit(exit_status - NORMAL_SHUTDOWN);
}

void gtmsource_end(void)
{
	gtmsource_end1(FALSE);
}
