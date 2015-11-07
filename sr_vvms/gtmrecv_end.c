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
#include "gtm_string.h"

#ifdef UNIX
#include "gtm_ipc.h"
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#elif defined(VMS)
#include <ssdef.h>
#include <psldef.h>
#include <descrip.h> /* Required for gtmrecv.h */
#else
#error Unsupported platform
#endif
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
#ifdef VMS
#include "repl_shm.h"
#endif
#include "repl_log.h"
#include "is_proc_alive.h"

GBLREF uint4			process_id;
GBLREF recvpool_addrs		recvpool;
GBLREF int			gtmrecv_filter;
GBLREF boolean_t		gtmrecv_logstats;
GBLREF int			gtmrecv_listen_sock_fd;
GBLREF int			gtmrecv_sock_fd;
GBLREF int			gtmrecv_log_fd;
GBLREF FILE			*gtmrecv_log_fp;
GBLREF qw_num                  	repl_recv_data_recvd;
GBLREF qw_num                  	repl_recv_data_processed;
GBLREF repl_msg_ptr_t		gtmrecv_msgp;
GBLREF uchar_ptr_t		repl_filter_buff;

int gtmrecv_endupd(void)
{
	VMS_ONLY(uint4	savepid;) UNIX_ONLY(pid_t savepid;)
	int		exit_status;
	UNIX_ONLY(pid_t	waitpid_res;)

	repl_log(stdout, TRUE, TRUE, "Initiating shut down of Update Process\n");
	recvpool.upd_proc_local->upd_proc_shutdown = SHUTDOWN;
	/* Wait for update process to shut down */
	while(recvpool.upd_proc_local->upd_proc_shutdown == SHUTDOWN &&
	      (savepid = UNIX_ONLY((pid_t))recvpool.upd_proc_local->upd_proc_pid) > 0 &&
	      is_proc_alive(savepid, 0))
	{
		SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_SHUTDOWN);
		UNIX_ONLY(WAITPID(savepid, &exit_status, WNOHANG, waitpid_res);) /* Release defunct update process if dead */
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
		if(0 != (errno = rel_sem(RECV, UPD_PROC_COUNT_SEM)))
			repl_log(stderr, TRUE, TRUE, "Error releasing the Update Process Count semaphore : %s\n", REPL_SEM_ERROR);
		repl_log(stdout, TRUE, TRUE, "Update Process exited\n");
	} else
	{
		repl_log(stderr, TRUE, TRUE, "Error in update proc count semaphore : %s\n", REPL_SEM_ERROR);
		exit_status = ABNORMAL_SHUTDOWN;
	}
	return (exit_status);
}

int gtmrecv_end1(boolean_t auto_shutdown)
{
	uint4		savepid;
	int		exit_status;
	seq_num		log_seqno, log_seqno1;
	int		fclose_res;
#ifdef VMS
	int4		status;
#endif

	exit_status = gtmrecv_end_helpers(TRUE);
	exit_status = gtmrecv_endupd();
	QWASSIGN(log_seqno, recvpool.recvpool_ctl->jnl_seqno);
	QWASSIGN(log_seqno1, recvpool.upd_proc_local->read_jnl_seqno);
	/* Detach from receive pool */
	recvpool.gtmrecv_local->shutdown = exit_status;
	recvpool.gtmrecv_local->recv_serv_pid = 0;
	UNIX_ONLY(
		if (recvpool.recvpool_ctl && 0 > SHMDT(recvpool.recvpool_ctl))
			repl_log(stderr, TRUE, TRUE, "Error detaching from Receive Pool : %s\n", REPL_STR_ERROR);
	)
	VMS_ONLY(
		if (recvpool.recvpool_ctl)
		{
			if (SS$_NORMAL != (status = detach_shm(recvpool.shm_range)))
				repl_log(stderr, TRUE, TRUE, "Error detaching from recvpool : %s\n", REPL_STR_ERROR);
			recvpool.recvpool_ctl = NULL;
			if (!auto_shutdown && (SS$_NORMAL != (status = signoff_from_gsec(recvpool.shm_lockid))))
				repl_log(stderr, TRUE, TRUE, "Error dequeueing lock on recvpool global section : %s\n",
														REPL_STR_ERROR);
		}
	)
	gtmrecv_free_msgbuff();
	gtmrecv_free_filter_buff();
	recvpool.recvpool_ctl = NULL;
	/* Close the connection with the Receiver */
	if (FD_INVALID != gtmrecv_listen_sock_fd)
		close(gtmrecv_listen_sock_fd);
	if (FD_INVALID != gtmrecv_sock_fd)
		close(gtmrecv_sock_fd);
	QWDECRBYDW(log_seqno, 1);
	QWDECRBYDW(log_seqno1, 1);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Last Recvd Seqno : %llu  Jnl Total : %llu  Msg Total : %llu\n",
			log_seqno, repl_recv_data_processed, repl_recv_data_recvd);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Last Seqno processed by update process : %llu\n", log_seqno1);
	gtm_event_log_close();
	if (gtmrecv_filter & EXTERNAL_FILTER)
		repl_stop_filter();
	if (auto_shutdown)
		return (exit_status);
	else
		gtmrecv_exit(exit_status - NORMAL_SHUTDOWN);
}

void gtmrecv_end(void)
{
	gtmrecv_end1(FALSE);
}
