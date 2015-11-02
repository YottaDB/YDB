/****************************************************************
 *								*
 *	Copyright 2006, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"		/* for close() */

#include "gtm_ipc.h"
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include "gtm_inet.h"
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
#include "mutex.h"
#include "repl_log.h"
#include "repl_comm.h"
#include "have_crit.h"
#include "anticipatory_freeze.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	uint4			process_id;
GBLREF	int			gtmsource_sock_fd;
GBLREF	int			gtmsource_log_fd;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	int			gtmsource_filter;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_statslog_fd;
GBLREF	FILE			*gtmsource_statslog_fp;
GBLREF	unsigned char		*gtmsource_tcombuff_start;
GBLREF	qw_num			repl_source_cmp_sent;
GBLREF	qw_num			repl_source_data_sent;
GBLREF	qw_num			repl_source_msg_sent;
GBLREF	seq_num			seq_num_zero;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	boolean_t		pool_init;

int gtmsource_end1(boolean_t auto_shutdown)
{
	int		exit_status, idx, status, save_errno;
	seq_num		read_jnl_seqno, jnlpool_seqno, diff_seqno, jnlpool_strm_seqno[MAX_SUPPL_STRMS];
	int		fclose_res;
	sgmnt_addrs	*repl_csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gtmsource_ctl_close();
	DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
	assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke and "rel_lock" unconditionally */
	rel_lock(jnlpool.jnlpool_dummy_reg);
	mutex_cleanup(jnlpool.jnlpool_dummy_reg);
	exit_status = NORMAL_SHUTDOWN;
	if (!auto_shutdown)
		jnlpool.gtmsource_local->shutdown = NORMAL_SHUTDOWN;
	read_jnl_seqno = jnlpool.gtmsource_local->read_jnl_seqno;
	jnlpool_seqno = jnlpool.jnlpool_ctl->jnl_seqno;
	for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
		jnlpool_strm_seqno[idx] = jnlpool.jnlpool_ctl->strm_seqno[idx];
	jnlpool.gtmsource_local->gtmsource_pid = 0;
	jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_DUMMY_STATE;
	if (!auto_shutdown && !ANTICIPATORY_FREEZE_AVAILABLE)
	{	/* Detach from journal pool */
		JNLPOOL_SHMDT(status, save_errno);
		if (0 > status)
			repl_log(gtmsource_log_fp, FALSE, TRUE, "Error detaching from journal pool : %s\n", STRERROR(save_errno));
		jnlpool.repl_inst_filehdr = NULL;
		jnlpool.gtmsrc_lcl_array = NULL;
		jnlpool.gtmsource_local_array = NULL;
		jnlpool.jnldata_base = NULL;
		pool_init = FALSE;
	}
	gtmsource_free_msgbuff();
	gtmsource_free_tcombuff();
	gtmsource_free_filter_buff();
	gtmsource_stop_heartbeat();
	repl_close(&gtmsource_sock_fd);
	if (jnlpool_seqno)
	{
		repl_log(gtmsource_log_fp, TRUE, FALSE, "REPL INFO - Current Jnlpool Seqno : %llu\n", jnlpool_seqno);
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
		{
			if (jnlpool_strm_seqno[idx])
				repl_log(gtmsource_log_fp, TRUE, FALSE, "REPL INFO - Stream # %d : Current Jnlpool Stream Seqno "
					": %llu\n", idx, jnlpool_strm_seqno[idx]);
		}
		jnlpool_seqno--;
	}
	if (read_jnl_seqno)
		read_jnl_seqno--;
	diff_seqno = jnlpool_seqno - read_jnl_seqno;
	repl_log(gtmsource_log_fp, TRUE, FALSE, "REPL INFO - Last Seqno written in jnlpool : %llu", jnlpool_seqno);
	repl_log(gtmsource_log_fp, FALSE, FALSE, "  Last Seqno sent : %llu", read_jnl_seqno);
	repl_log(gtmsource_log_fp, FALSE, TRUE, "  Number of unsent Seqno : %llu\n", 0 < diff_seqno ? diff_seqno : 0);
	repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO - Jnl Total : %llu  Msg Total : %llu  CmpMsg Total : %llu\n",
		 repl_source_data_sent, repl_source_msg_sent, repl_source_cmp_sent);
	if (gtmsource_filter & EXTERNAL_FILTER)
		repl_stop_filter();
	gtm_event_log_close();
	if (auto_shutdown)
		return (exit_status);
	else
		gtmsource_exit(exit_status - NORMAL_SHUTDOWN);

	return -1; /* This will never get executed, added to make compiler happy */
}

void gtmsource_end(void)
{
	gtmsource_end1(FALSE);
}
