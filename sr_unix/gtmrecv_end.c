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

#include "gtm_unistd.h"		/* for close */
#include "gtm_string.h"

#include "gtm_ipc.h"
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>
#include "gtm_inet.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "gtm_event_log.h"
#include "eintr_wrappers.h"
#include "jnl.h"
#include "repl_filter.h"
#include "repl_msg.h"
#include "repl_sem.h"
#include "repl_log.h"
#include "is_proc_alive.h"
#include "gtmsource.h"
#include "gtmio.h"
#include "have_crit.h"

GBLREF	uint4			process_id;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			gtmrecv_filter;
GBLREF	boolean_t		gtmrecv_logstats;
GBLREF	int			gtmrecv_listen_sock_fd;
GBLREF	int			gtmrecv_sock_fd;
GBLREF	int			gtmrecv_log_fd;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	qw_num			repl_recv_data_recvd;
GBLREF	qw_num			repl_recv_data_processed;
GBLREF	repl_msg_ptr_t		gtmrecv_msgp;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		pool_init;

int gtmrecv_endupd(void)
{
	pid_t 		savepid;
	int		exit_status, status, save_errno;
	pid_t		waitpid_res;

	repl_log(stdout, TRUE, TRUE, "Initiating shut down of Update Process\n");
	recvpool.upd_proc_local->upd_proc_shutdown = SHUTDOWN;
	/* Wait for update process to shut down */
	while((SHUTDOWN == recvpool.upd_proc_local->upd_proc_shutdown)
		&& (0 < (savepid = (pid_t)recvpool.upd_proc_local->upd_proc_pid)) && is_proc_alive(savepid, 0))
	{
		SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_SHUTDOWN);
		WAITPID(savepid, &exit_status, WNOHANG, waitpid_res); /* Release defunct update process if dead */
	}
	exit_status = recvpool.upd_proc_local->upd_proc_shutdown;
	if (SHUTDOWN == exit_status)
	{
		if (0 == savepid) /* No Update Process */
			exit_status = NORMAL_SHUTDOWN;
		else /* Update Process Crashed */
		{
			repl_log(stderr, TRUE, TRUE, "Update Process exited abnormally, INTEGRITY CHECK might be warranted\n");
			exit_status = ABNORMAL_SHUTDOWN;
		}
	}
	/* Wait for the Update Process to detach */
	if (0 == grab_sem(RECV, UPD_PROC_COUNT_SEM))
	{
		if (0 != (status = rel_sem(RECV, UPD_PROC_COUNT_SEM)))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Error releasing the Update Process Count semaphore : %s\n",
					STRERROR(save_errno));
		}
		repl_log(stdout, TRUE, TRUE, "Update Process exited\n");
	} else
	{
		save_errno = errno;
		repl_log(stderr, TRUE, TRUE, "Error in update proc count semaphore : %s\n", STRERROR(save_errno));
		exit_status = ABNORMAL_SHUTDOWN;
	}
	return (exit_status);
}

int gtmrecv_end1(boolean_t auto_shutdown)
{
	int4		strm_idx;
	int		exit_status, idx, status, save_errno;
	int		fclose_res, rc;
	seq_num		log_seqno, log_seqno1, jnlpool_seqno, jnlpool_strm_seqno[MAX_SUPPL_STRMS];
	uint4		savepid;

	exit_status = gtmrecv_end_helpers(TRUE);
	exit_status = gtmrecv_endupd();
	log_seqno = recvpool.recvpool_ctl->jnl_seqno;
	log_seqno1 = recvpool.upd_proc_local->read_jnl_seqno;
	strm_idx = recvpool.gtmrecv_local->strm_index;
	/* Detach from receive pool */
	recvpool.gtmrecv_local->shutdown = exit_status;
	recvpool.gtmrecv_local->recv_serv_pid = 0;
	if (0 > SHMDT(recvpool.recvpool_ctl))
	{
		save_errno = errno;
		repl_log(stderr, TRUE, TRUE, "Error detaching from Receive Pool : %s\n", STRERROR(save_errno));
	}
	recvpool.recvpool_ctl = NULL;
	assert((NULL != jnlpool_ctl) && (jnlpool_ctl == jnlpool.jnlpool_ctl));
	if (NULL != jnlpool.jnlpool_ctl)
	{	/* Reset fields that might have been initialized by the receiver server after connecting to the primary.
		 * It is ok not to hold the journal pool lock while updating jnlpool_ctl fields since this will be the
		 * only process updating those fields.
		 */
		jnlpool.jnlpool_ctl->primary_instname[0] = '\0';
		jnlpool.jnlpool_ctl->gtmrecv_pid = 0;
		jnlpool_seqno = jnlpool.jnlpool_ctl->jnl_seqno;
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			jnlpool_strm_seqno[idx] = jnlpool.jnlpool_ctl->strm_seqno[idx];
		/* Also take this opportunity to detach from the journal pool except in the auto_shutdown case. This is because
		 * the fields "jnlpool_ctl->repl_inst_filehdr->recvpool_semid" and "jnlpool_ctl->repl_inst_filehdr->recvpool_shmid"
		 * need to be reset by "gtmrecv_jnlpool_reset" (called from "gtmrecv_shutdown") which is invoked a little later.
		 */
		if (!auto_shutdown)
		{
			JNLPOOL_SHMDT(status, save_errno);
			if (0 > status)
				repl_log(stderr, TRUE, TRUE, "Error detaching from Journal Pool : %s\n", STRERROR(save_errno));
			jnlpool.jnlpool_ctl = jnlpool_ctl = NULL;
			jnlpool.repl_inst_filehdr = NULL;
			jnlpool.gtmsrc_lcl_array = NULL;
			jnlpool.gtmsource_local_array = NULL;
			jnlpool.jnldata_base = NULL;
			pool_init = FALSE;
		}
	} else
		jnlpool_seqno = 0;
	gtmrecv_free_msgbuff();
	gtmrecv_free_filter_buff();
	recvpool.recvpool_ctl = NULL;
	/* Close the connection with the Receiver */
	if (FD_INVALID != gtmrecv_listen_sock_fd)
		CLOSEFILE_RESET(gtmrecv_listen_sock_fd, rc);	/* resets "gtmrecv_listen_sock_fd" to FD_INVALID */
	if (FD_INVALID != gtmrecv_sock_fd)
		CLOSEFILE_RESET(gtmrecv_sock_fd, rc);	/* resets "gtmrecv_sock_fd" to FD_INVALID */
	repl_log(gtmrecv_log_fp, TRUE, FALSE, "REPL INFO - Current Jnlpool Seqno : %llu\n", jnlpool_seqno);
	for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
	{
		if (jnlpool_strm_seqno[idx])
			repl_log(gtmrecv_log_fp, TRUE, FALSE, "REPL INFO - Stream # %d : Current Jnlpool Stream Seqno : %llu\n",
				idx, jnlpool_strm_seqno[idx]);
	}
	if (0 < strm_idx)
		repl_log(gtmrecv_log_fp, TRUE, FALSE, "REPL INFO - Receiver server has Stream # %d\n", strm_idx);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Current Update process Read Seqno : %llu\n", log_seqno1);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Current Receive Pool Seqno : %llu\n", log_seqno);
	/* If log_seqno/log_seqno1 is 0, then do not decrement it as that will be interpreted as a huge positive seqno. Keep it 0 */
	if (log_seqno)
		log_seqno--;
	if (log_seqno1)
		log_seqno1--;
	repl_log(gtmrecv_log_fp, TRUE, FALSE, "REPL INFO - Last Recvd Seqno : %llu  Jnl Total : %llu  Msg Total : %llu\n",
			log_seqno, repl_recv_data_processed, repl_recv_data_recvd);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Last Seqno processed by update process : %llu\n", log_seqno1);
	gtm_event_log_close();
	if (gtmrecv_filter & EXTERNAL_FILTER)
		repl_stop_filter();
	if (auto_shutdown)
		return (exit_status);
	else
		gtmrecv_exit(exit_status - NORMAL_SHUTDOWN);

	return -1; /* This will never get executed, added to make compiler happy */
}

void gtmrecv_end(void)
{
	gtmrecv_end1(FALSE);
}
