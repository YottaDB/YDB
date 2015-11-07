/****************************************************************
 *								*
 *	Copyright 2006, 2009 Fidelity Information Services, Inc	*
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
#ifdef UNIX
#include "gtm_ipc.h"
#include <sys/wait.h>
#include "repl_instance.h"
#elif defined(VMS)
#include <ssdef.h>
#include <descrip.h> /* Required for gtmrecv.h */
#endif
#include <errno.h>

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
#ifdef UNIX
#include "ftok_sems.h"
#endif

GBLDEF boolean_t		gtmrecv_fetchreysnc;
GBLDEF boolean_t		gtmrecv_logstats = FALSE;
GBLDEF int			gtmrecv_filter = NO_FILTER;
GBLDEF seq_num			gtmrecv_resync_seqno;
GBLREF void                      (*call_on_signal)();

GBLREF uint4			process_id;
GBLREF recvpool_addrs		recvpool;
GBLREF int			recvpool_shmid;
GBLREF gtmrecv_options_t	gtmrecv_options;
GBLREF int			gtmrecv_log_fd;
GBLREF FILE			*gtmrecv_log_fp;
GBLREF boolean_t        	is_rcvr_server;
GBLREF int			gtmrecv_srv_count;
GBLREF uint4			log_interval;

int gtmrecv(void)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	uint4			gtmrecv_pid, channel;
	int			semval, status, save_upd_status, upd_start_status, upd_start_attempts;
	char			print_msg[1024];
	recvpool_user		pool_user = GTMRECV;
#ifdef UNIX
	pid_t			pid, procgp;
	int			exit_status, waitpid_res;
	int			log_init_status;
#elif defined(VMS)
	uint4			pid;
	char			proc_name[PROC_NAME_MAXLEN + 1];
	$DESCRIPTOR(proc_name_desc, proc_name);
#endif

	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_MUPCLIERR);
	error_def(ERR_TEXT);
	error_def(ERR_REPLERR);

	call_on_signal = gtmrecv_sigstop;
	ESTABLISH_RET(gtmrecv_ch, SS_NORMAL);

#ifdef VMS
	pool_user = (CLI_PRESENT == cli_present("DUMMY_START")) ? GTMRECV_CHILD : GTMRECV;
#endif
	memset((uchar_ptr_t)&recvpool, 0, SIZEOF(recvpool));
	if (-1 == gtmrecv_get_opt())
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	recvpool_init(pool_user, gtmrecv_options.start && 0 != gtmrecv_options.listen_port, gtmrecv_options.start);
	/*
	 * When gtmrecv_options.start is TRUE, shm field recvpool.recvpool_ctl->fresh_start is updated in recvpool_init()
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
			if (SS_NORMAL == (status = repl_fork_rcvr_server(&pid, &channel)) && pid)
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
#ifdef UNIX
					WAITPID(pid, &exit_status, WNOHANG, waitpid_res); /* Release defunct child if dead */
#endif
				}
#ifdef VMS
				/* Deassign the cmd mailbox channel */
				if (SS_NORMAL != (status = sys$dassgn(channel)))
				{
					gtm_putmsg(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
							RTS_ERROR_LITERAL("Unable to close send-cmd mbox channel"), status);
					gtmrecv_exit(ABNORMAL_SHUTDOWN);
				}
#endif
				if (0 <= semval)
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				gtmrecv_exit(1 == semval ? SRV_ALIVE : SRV_DEAD);
			} else if (SS_NORMAL != status)
			{
#ifdef UNIX
				status = errno;
#endif
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
	is_rcvr_server = TRUE;
	process_id = getpid();
	strcpy(gtmrecv_local->log_file, gtmrecv_options.log_file);
	gtmrecv_local->log_interval = log_interval = gtmrecv_options.rcvr_log_interval;
	upd_proc_local->log_interval = gtmrecv_options.upd_log_interval;
	upd_helper_ctl->start_helpers = FALSE;
	upd_helper_ctl->start_n_readers = upd_helper_ctl->start_n_writers = 0;
#ifdef UNIX
	log_init_status = repl_log_init(REPL_GENERAL_LOG, &gtmrecv_log_fd, NULL, gtmrecv_options.log_file, NULL);
	assert(SS_NORMAL == log_init_status);
	repl_log_fd2fp(&gtmrecv_log_fp, gtmrecv_log_fd);
	if (-1 == (procgp = setsid()))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Receiver server error in setsid"), errno);
#elif defined(VMS)
	util_log_open(STR_AND_LEN(gtmrecv_options.log_file));
	/* Get a meaningful process name */
	proc_name_desc.dsc$w_length = get_proc_name(LIT_AND_LEN("GTMRCV"), process_id, proc_name);
	if (SS$_NORMAL != (status = sys$setprn(&proc_name_desc)))
	{
		gtm_putmsg(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to change receiver server process name"), status);
		gtmrecv_exit(ABNORMAL_SHUTDOWN);
	}
#else
#error Unsupported platform
#endif
	gtm_event_log_init();
	gtmrecv_local->recv_serv_pid = process_id;
	gtmrecv_local->listen_port = gtmrecv_options.listen_port;
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
			gtmrecv_autoshutdown();
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
		gtmrecv_autoshutdown();
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
#ifdef UNIX
	/*
	 * Child needs to increment receivpool lock couner semaphore.
	 * Since we can have ftok collisions with someone else, we cannot gaurantee that current count is one.
	 * So just increment semaphore number 1 after grabbing semaphore number 0. Then release semaphore 0.
	 */
	if (!ftok_sem_get(recvpool.recvpool_dummy_reg, TRUE, REPLPOOL_ID, FALSE))
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	if (!ftok_sem_release(recvpool.recvpool_dummy_reg, FALSE, FALSE))
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
#endif
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
