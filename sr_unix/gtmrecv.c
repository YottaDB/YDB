/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_inet.h"
#include "gtm_ipc.h"
#include <sys/wait.h>
#include "repl_instance.h"
#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "gtmrecv.h"
#include "repl_log.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "repl_errno.h"
#include "gtm_event_log.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "cli.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"
#include "tp.h"
#include "repl_filter.h"
#include "error.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "is_proc_alive.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmmsg.h"
#include "sgtm_putmsg.h"
#include "gt_timer.h"
#include "ftok_sems.h"
#include "init_secshr_addrs.h"
#include "mutex.h"
#include "fork_init.h"

GBLDEF	boolean_t		gtmrecv_fetchreysnc;
GBLDEF	boolean_t		gtmrecv_logstats = FALSE;
GBLDEF	int			gtmrecv_filter = NO_FILTER;

GBLREF	void			(*call_on_signal)();
GBLREF	uint4			process_id;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			recvpool_shmid;
GBLREF	gtmrecv_options_t	gtmrecv_options;
GBLREF	int			gtmrecv_log_fd;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	boolean_t		is_rcvr_server;
GBLREF	int			gtmrecv_srv_count;
GBLREF	uint4			log_interval;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	IN_PARMS		*cli_lex_in_ptr;
GBLREF	uint4			mutex_per_process_init_pid;

error_def(ERR_MUPCLIERR);
error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPLERR);
error_def(ERR_REPLINFO);
error_def(ERR_TEXT);

int gtmrecv(void)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	uint4			gtmrecv_pid;
	int			semval, status, save_upd_status, upd_start_status, upd_start_attempts;
	char			print_msg[1024], tmpmsg[1024];
	recvpool_user		pool_user = GTMRECV;
	pid_t			pid, procgp;
	int			exit_status, waitpid_res;
	int			log_init_status;

	call_on_signal = gtmrecv_sigstop;
	ESTABLISH_RET(gtmrecv_ch, SS_NORMAL);
	memset((uchar_ptr_t)&recvpool, 0, SIZEOF(recvpool));
	if (-1 == gtmrecv_get_opt())
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	if (gtmrecv_options.start || gtmrecv_options.shut_down)
		jnlpool_init(GTMRECEIVE, (boolean_t)FALSE, (boolean_t *)NULL);
	if (gtmrecv_options.shut_down)
	{	/* Wait till shutdown time nears even before going to "recvpool_init". This is because the latter will return
		 * with the ftok semaphore, access and options semaphore held and we do not want to be holding those locks (while
		 * waiting for the user specified timeout to expire) as that will affect new GTM processes and/or other
		 * MUPIP REPLIC commands that need these locks for their function.
		 */
		if (0 < gtmrecv_options.shutdown_time)
		{
			repl_log(stdout, FALSE, TRUE, "Waiting for %d seconds before signalling shutdown\n",
				gtmrecv_options.shutdown_time);
			LONG_SLEEP(gtmrecv_options.shutdown_time);
		} else
			repl_log(stdout, FALSE, TRUE, "Signalling immediate shutdown\n");
	}
	recvpool_init(pool_user, gtmrecv_options.start && 0 != gtmrecv_options.listen_port);
	/*
	 * When gtmrecv_options.start is TRUE, shm field recvpool.recvpool_ctl->fresh_start is updated in "recvpool_init"
	 *	recvpool.recvpool_ctl->fresh_start == TRUE ==> fresh start, and
	 *	recvpool.recvpool_ctl->fresh_start == FALSE ==> start after a crash
	 */
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	upd_helper_ctl = recvpool.upd_helper_ctl;
	if (GTMRECV == pool_user)
	{
		if (gtmrecv_options.start)
		{
			if (0 == gtmrecv_options.listen_port /* implies (updateonly || helpers only) */
				|| !recvpool_ctl->fresh_start)
			{
				if (SRV_ALIVE == (status = is_recv_srv_alive()) && 0 != gtmrecv_options.listen_port)
				{
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
						  RTS_ERROR_LITERAL("Receiver Server already exists"));
				} else if (SRV_DEAD == status && 0 == gtmrecv_options.listen_port)
				{
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
						  RTS_ERROR_LITERAL("Receiver server does not exist. Start it first"));
				} else if (SRV_ERR == status)
				{
					status = errno;
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
						  RTS_ERROR_LITERAL("Receiver server semaphore error"), status);
				}
				if (gtmrecv_options.updateonly)
				{
					status = gtmrecv_start_updonly() - UPDPROC_STARTED;
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					gtmrecv_exit(status);
				}
				if (gtmrecv_options.helpers && 0 == gtmrecv_options.listen_port)
				{ /* start helpers only */
					status = gtmrecv_start_helpers(gtmrecv_options.n_readers, gtmrecv_options.n_writers);
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					gtmrecv_exit(status - NORMAL_SHUTDOWN);
				}
			}
#ifndef REPL_DEBUG_NOBACKGROUND
			DO_FORK(pid);
			if (0 < pid)
			{
				REPL_DPRINT2("Waiting for receiver child process %d to startup\n", pid);
				while (0 == (semval = get_sem_info(RECV, RECV_SERV_COUNT_SEM, SEM_INFO_VAL)) &&
				       is_proc_alive(pid, 0))
				{
					/* To take care of reassignment of PIDs, the while condition should be && with the
					 * condition (PPID of pid == process_id)
					 */
					REPL_DPRINT2("Waiting for receiver child process %d to startup\n", pid);
					SHORT_SLEEP(GTMRECV_WAIT_FOR_SRV_START);
					WAITPID(pid, &exit_status, WNOHANG, waitpid_res); /* Release defunct child if dead */
				}
				if (0 <= semval)
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				gtmrecv_exit(1 == semval ? SRV_ALIVE : SRV_DEAD);
			} else if (0 > pid)
			{
				status = errno;
				rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Unable to fork"), status);
			}
#endif
		} else if (gtmrecv_options.shut_down)
		{
			if (gtmrecv_options.updateonly)
				gtmrecv_exit(gtmrecv_endupd() - NORMAL_SHUTDOWN);
			if (gtmrecv_options.helpers)
				gtmrecv_exit(gtmrecv_end_helpers(FALSE) - NORMAL_SHUTDOWN);
			gtmrecv_exit(gtmrecv_shutdown(FALSE, NORMAL_SHUTDOWN) - NORMAL_SHUTDOWN);
		} else if (gtmrecv_options.changelog)
		{
			gtmrecv_exit(gtmrecv_changelog() - NORMAL_SHUTDOWN);
		} else if (gtmrecv_options.checkhealth)
		{
			gtmrecv_exit(gtmrecv_checkhealth() - NORMAL_SHUTDOWN);
		} else if (gtmrecv_options.showbacklog)
		{
			gtmrecv_exit(gtmrecv_showbacklog() - NORMAL_SHUTDOWN);
		} else
		{
			gtmrecv_exit(gtmrecv_statslog() - NORMAL_SHUTDOWN);
		}
	} /* (pool_user != GTMRECV) */
	assert(!holds_sem[RECV][RECV_POOL_ACCESS_SEM]);
	assert(holds_sem[RECV][RECV_SERV_OPTIONS_SEM]);
	is_rcvr_server = TRUE;
	process_id = getpid();
	/* Reinvoke secshr related initialization with the child's pid */
	INVOKE_INIT_SECSHR_ADDRS;
	/* Initialize mutex socket, memory semaphore etc. before any "grab_lock" is done by this process on the journal pool.
	 * Note that the initialization would already have been done by the parent receiver startup command but we need to
	 * redo the initialization with the child process id.
	 */
	assert(mutex_per_process_init_pid && mutex_per_process_init_pid != process_id);
	mutex_per_process_init();
	STRCPY(gtmrecv_local->log_file, gtmrecv_options.log_file);
	gtmrecv_local->log_interval = log_interval = gtmrecv_options.rcvr_log_interval;
	upd_proc_local->log_interval = gtmrecv_options.upd_log_interval;
	upd_helper_ctl->start_helpers = FALSE;
	upd_helper_ctl->start_n_readers = upd_helper_ctl->start_n_writers = 0;
	log_init_status = repl_log_init(REPL_GENERAL_LOG, &gtmrecv_log_fd, NULL, gtmrecv_options.log_file, NULL);
	assert(SS_NORMAL == log_init_status);
	repl_log_fd2fp(&gtmrecv_log_fp, gtmrecv_log_fd);
	if (-1 == (procgp = setsid()))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Receiver server error in setsid"), errno);
	gtm_event_log_init();
	gtmrecv_local->recv_serv_pid = process_id;
	assert(NULL != jnlpool.jnlpool_ctl);
	jnlpool.jnlpool_ctl->gtmrecv_pid = process_id;
	gtmrecv_local->listen_port = gtmrecv_options.listen_port;
	/* Log receiver server startup command line first */
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "%s %s\n", cli_lex_in_ptr->argv[0], cli_lex_in_ptr->in_str);

	assert(NULL != jnlpool.repl_inst_filehdr);
	SPRINTF(tmpmsg, "GTM Replication Receiver Server with Pid [%d] started on replication instance [%s]",
		process_id, jnlpool.repl_inst_filehdr->this_instname);
	sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2, LEN_AND_STR(tmpmsg));
	repl_log(gtmrecv_log_fp, TRUE, TRUE, print_msg);
	gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLINFO", print_msg);
	if (recvpool_ctl->fresh_start)
		QWASSIGNDW(recvpool_ctl->jnl_seqno, 0); /* Update process will initialize this to a non-zero value */
	else
	{	/* Coming up after a crash, reset Update process read.  This is done by setting gtmrecv_local->restart.
		 * This will trigger update process to reset recvpool_ctl->jnl_seqno too.
		 */
		gtmrecv_local->restart = GTMRECV_RCVR_RESTARTED;
	}
	save_upd_status = upd_proc_local->upd_proc_shutdown;
	for (upd_start_attempts = 0;
	     UPDPROC_START_ERR == (upd_start_status = gtmrecv_upd_proc_init(recvpool_ctl->fresh_start)) &&
	     GTMRECV_MAX_UPDSTART_ATTEMPTS > upd_start_attempts;
	     upd_start_attempts++)
	{
		if (EREPL_UPDSTART_SEMCTL == repl_errno || EREPL_UPDSTART_BADPATH == repl_errno)
		{
			gtmrecv_exit(ABNORMAL_SHUTDOWN);
		} else if (EREPL_UPDSTART_FORK == repl_errno)
		{
			/* Couldn't start up update now, can try later */
			LONG_SLEEP(GTMRECV_WAIT_FOR_PROC_SLOTS);
			continue;
		} else if (EREPL_UPDSTART_EXEC == repl_errno)
		{
			/* In forked child, could not exec, should exit */
			upd_proc_local->upd_proc_shutdown = save_upd_status;
			gtmrecv_exit(ABNORMAL_SHUTDOWN);
		}
	}
	if ((UPDPROC_EXISTS == upd_start_status && recvpool_ctl->fresh_start) ||
	    (UPDPROC_START_ERR == upd_start_status && GTMRECV_MAX_UPDSTART_ATTEMPTS <= upd_start_attempts))
	{
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLERR, RTS_ERROR_LITERAL((UPDPROC_EXISTS == upd_start_status) ?
			    "Runaway Update Process. Aborting..." :
			    "Too many failed attempts to fork Update Process. Aborting..."));
		repl_log(gtmrecv_log_fp, TRUE, TRUE, print_msg);
		gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLERR", print_msg);
		gtmrecv_exit(ABNORMAL_SHUTDOWN);
	}
	upd_proc_local->start_upd = UPDPROC_STARTED;
	if (!recvpool_ctl->fresh_start)
	{
		while ((GTMRECV_RCVR_RESTARTED == gtmrecv_local->restart) && (SRV_ALIVE == is_updproc_alive()))
		{
			REPL_DPRINT1("Rcvr waiting for update to restart\n");
			SHORT_SLEEP(GTMRECV_WAIT_FOR_SRV_START);
		}
		upd_proc_local->bad_trans = FALSE;
		recvpool_ctl->write_wrap = recvpool_ctl->recvpool_size;
		recvpool_ctl->write = 0;
		recvpool_ctl->wrapped = FALSE;
		upd_proc_local->changelog = TRUE;
		gtmrecv_local->restart = GTMRECV_NO_RESTART; /* release the update process wait */
	}
	if (gtmrecv_options.helpers)
		gtmrecv_helpers_init(gtmrecv_options.n_readers, gtmrecv_options.n_writers);
	/* It is necessary for every process that is using the ftok semaphore to increment the counter by 1. This is used
	 * by the last process that shuts down to delete the ftok semaphore when it notices the counter to be 0.
	 * Note that the parent receiver server startup command would have done an increment of the ftok counter semaphore
	 * for the replication instance file. But the receiver server process (the child) that comes here would not have done
	 * that. Do that while the parent is still waiting for our okay.
	 */
	if (!ftok_sem_incrcnt(recvpool.recvpool_dummy_reg))
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	/* Lock the receiver server count semaphore. Its value should be atmost 1. */
	if (0 > grab_sem_immediate(RECV, RECV_SERV_COUNT_SEM))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Receive pool semop failure"),
			  REPL_SEM_ERRNO);
#ifdef REPL_DEBUG_NOBACKGROUND
	rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
#endif
	gtmrecv_srv_count++;
	gtmrecv_filter = NO_FILTER;
	if ('\0' != gtmrecv_local->filter_cmd[0])
	{
		if (SS_NORMAL == (status = repl_filter_init(gtmrecv_local->filter_cmd)))
			gtmrecv_filter |= EXTERNAL_FILTER;
		else
		{
			if (EREPL_FILTERSTART_EXEC == repl_errno)
				gtmrecv_exit(ABNORMAL_SHUTDOWN);
		}
	}
	gtmrecv_process(!recvpool_ctl->fresh_start);
	return (SS_NORMAL);
}
