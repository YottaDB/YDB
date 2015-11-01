/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include "gtm_unistd.h"
#include <arpa/inet.h>
#include "gtm_string.h"
#ifdef UNIX
#include <sys/sem.h>
#include "repl_instance.h"
#elif defined(VMS)
#include <descrip.h> /* Required for gtmrecv.h */
#else
#error Unsupported platform
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "is_proc_alive.h"
#include "repl_log.h"
#include "gt_timer.h"
#ifdef UNIX
#include "ftok_sems.h"
#endif

#define GTMRECV_WAIT_FOR_SHUTDOWN	(1000 - 1) /* ms, almost 1s */

GBLREF uint4			process_id;
GBLREF recvpool_addrs		recvpool;
GBLREF int			recvpool_shmid;
GBLREF gtmrecv_options_t	gtmrecv_options;
GBLREF boolean_t		is_rcvr_server;
GBLREF int			gtmrecv_srv_count;
GBLREF void			(*call_on_signal)();

int gtmrecv_shutdown(boolean_t auto_shutdown, int exit_status)
{

	uint4           savepid;
	boolean_t       shut_upd_too;
	int             status;

	UNIX_ONLY(error_def(ERR_RECVPOOLSETUP);)
	repl_log(stdout, TRUE, TRUE, "Initiating shut down\n");
	call_on_signal = NULL;		/* So we don't reenter on error */
	UNIX_ONLY(
		if (!ftok_sem_lock(recvpool.recvpool_dummy_reg, FALSE, FALSE))
			rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	)
	/* Grab the receive pool access control and receive pool option write lock */
	status = grab_sem(RECV, RECV_POOL_ACCESS_SEM);
	if (0 == status && (!auto_shutdown || gtmrecv_srv_count))
	{
		if((status = grab_sem(RECV, RECV_SERV_OPTIONS_SEM)) < 0)
			rel_sem(RECV, RECV_POOL_ACCESS_SEM);
	} /* else if autoshutdown, parent still holds RECV_SERV_OPTIONS_SEM and the child is exiting at startup */
	if (0 > status)
	{
		repl_log(stderr, TRUE, TRUE,
			"Error grabbing receive pool control/recvpool option write lock : %s. Shutdown not complete\n",
			REPL_SEM_ERROR);
		return (ABNORMAL_SHUTDOWN);
	}

	shut_upd_too = FALSE;

	if (!auto_shutdown)
	{
		/* Wait till shutdown time nears */
		if (0 < gtmrecv_options.shutdown_time)
		{
			repl_log(stdout, FALSE, TRUE, "Waiting for %d seconds before signalling shutdown\n",
				gtmrecv_options.shutdown_time);
			LONG_SLEEP(gtmrecv_options.shutdown_time);
		} else
			repl_log(stdout, FALSE, TRUE, "Signalling immediate shutdown\n");
		recvpool.gtmrecv_local->shutdown = SHUTDOWN;

		/* Wait for receiver server to shut down */
		while(recvpool.gtmrecv_local->shutdown == SHUTDOWN &&
	      	      0 < (savepid = recvpool.gtmrecv_local->recv_serv_pid) &&
	      	       is_proc_alive(savepid, 0))
				SHORT_SLEEP(GTMRECV_WAIT_FOR_SHUTDOWN);

		exit_status = recvpool.gtmrecv_local->shutdown;
		if (SHUTDOWN == exit_status)
		{
			if (0 == savepid) /* No Receiver Process */
				exit_status = NORMAL_SHUTDOWN;
			else /* Receiver Server Crashed */
			{
				repl_log(stderr, FALSE, TRUE, "Receiver Server exited abnormally\n");
				exit_status = ABNORMAL_SHUTDOWN;
				shut_upd_too = TRUE;
			}
		}
	}

	if (shut_upd_too)
		gtmrecv_endupd();

	/*
	 * gtmrecv_ipc_cleanup will not be successful unless receiver server has completely exited.
	 * It relies on RECV_SERV_COUNT_SEM value.
	 */
	if (FALSE == gtmrecv_ipc_cleanup(auto_shutdown, &exit_status))
	{
		/* Release all semaphores */
		if (!auto_shutdown)
		{
			rel_sem_immediate( RECV, UPD_PROC_COUNT_SEM);
			rel_sem_immediate( RECV, RECV_SERV_COUNT_SEM);
		}
		rel_sem_immediate( RECV, RECV_SERV_OPTIONS_SEM);
		rel_sem_immediate( RECV, RECV_POOL_ACCESS_SEM);
	}
	UNIX_ONLY(
	else if (NORMAL_SHUTDOWN == exit_status)
		repl_inst_recvpool_reset();
	if (!ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, FALSE))
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	)
	return (exit_status);
}

static void gtmrecv_stop(boolean_t exit)
{
	int status;

	status = gtmrecv_shutdown(TRUE, gtmrecv_end1(TRUE)) - NORMAL_SHUTDOWN;
	if (exit)
		gtmrecv_exit(status);
	return;
}

void gtmrecv_sigstop(void)
{
	if (is_rcvr_server)
		gtmrecv_stop(FALSE);
	return;
}

void gtmrecv_autoshutdown(void)
{
	gtmrecv_stop(TRUE);
	return;
}
