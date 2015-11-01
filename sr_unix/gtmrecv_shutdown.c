/****************************************************************
 *								*
 *	Copyright 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_string.h"

#include <sys/time.h>
#include <errno.h>
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
#include "gtmmsg.h"

#define GTMRECV_WAIT_FOR_SHUTDOWN	(1000 - 1) /* ms, almost 1s */

GBLREF	uint4			process_id;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			recvpool_shmid;
GBLREF	gtmrecv_options_t	gtmrecv_options;
GBLREF	boolean_t		is_rcvr_server;
GBLREF	int			gtmrecv_srv_count;
GBLREF	void			(*call_on_signal)();
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];

int gtmrecv_shutdown(boolean_t auto_shutdown, int exit_status)
{
	uint4           savepid;
	boolean_t       shut_upd_too = FALSE;
	int             status;
	unix_db_info	*udi;

	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_TEXT);

	repl_log(stdout, TRUE, TRUE, "Initiating shut down\n");
	call_on_signal = NULL;		/* So we don't reenter on error */
	/* assert that auto shutdown should be invoked only if the current process is a receiver server */
	assert(!auto_shutdown || gtmrecv_srv_count);
	if (auto_shutdown)
	{	/* grab the ftok semaphore and recvpool access control lock IN THAT ORDER (to avoid deadlocks) */
		repl_inst_ftok_sem_lock();
		status = grab_sem(RECV, RECV_POOL_ACCESS_SEM);
		if (0 > status)
		{
			repl_log(stderr, TRUE, TRUE,
				"Error grabbing receive pool control semaphore : %s. Shutdown not complete\n", REPL_SEM_ERROR);
			return (ABNORMAL_SHUTDOWN);
		}
	} else
	{	/* ftok semaphore and recvpool access semaphore should already be held from the previous call to "recvpool_init" */
		DEBUG_ONLY(udi = (unix_db_info *)FILE_INFO(recvpool.recvpool_dummy_reg);)
		assert(udi->grabbed_ftok_sem);
		assert(holds_sem[RECV][RECV_POOL_ACCESS_SEM]);
		/* We do not want to hold the options semaphore to avoid deadlocks with receiver server startup (C9F12-002766) */
		assert(!holds_sem[RECV][RECV_SERV_OPTIONS_SEM]);
		recvpool.gtmrecv_local->shutdown = SHUTDOWN;
		/* Wait for receiver server to die. But release ftok semaphore and recvpool access control semaphore before
		 * waiting as the concurrently running receiver server might need these (e.g. if it is about to call the
		 * function "repl_inst_was_rootprimary").
		 */
		if (0 != rel_sem(RECV, RECV_POOL_ACCESS_SEM))
			gtm_putmsg(VARLSTCNT(7) ERR_TEXT, 2, RTS_ERROR_LITERAL("Error in receiver server shutdown rel_sem"),
				REPL_SEM_ERRNO);
		repl_inst_ftok_sem_release();
		/* Wait for receiver server to shut down */
		while((SHUTDOWN == recvpool.gtmrecv_local->shutdown)
				&& (0 < (savepid = recvpool.gtmrecv_local->recv_serv_pid))
				&& is_proc_alive(savepid, 0))
			SHORT_SLEEP(GTMRECV_WAIT_FOR_SHUTDOWN);
		/* (Re)Grab the ftok semaphore and recvpool access control semaphore IN THAT ORDER (to avoid deadlocks) */
		repl_inst_ftok_sem_lock();
		status = grab_sem(RECV, RECV_POOL_ACCESS_SEM);
		if (0 > status)
		{
			repl_log(stderr, TRUE, TRUE,
				"Error regrabbing receive pool control semaphore : %s. Shutdown not complete\n", REPL_SEM_ERROR);
			return (ABNORMAL_SHUTDOWN);
		}
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
	/* gtmrecv_ipc_cleanup will not be successful unless receiver server has completely exited.
	 * It relies on RECV_SERV_COUNT_SEM value.
	 */
	if (FALSE == gtmrecv_ipc_cleanup(auto_shutdown, &exit_status))
	{	/* Release all semaphores */
		if (!auto_shutdown)
		{
			rel_sem_immediate( RECV, UPD_PROC_COUNT_SEM);
			rel_sem_immediate( RECV, RECV_SERV_COUNT_SEM);
		}
		rel_sem_immediate( RECV, RECV_POOL_ACCESS_SEM);
	} else if (NORMAL_SHUTDOWN == exit_status)
		repl_inst_recvpool_reset();
	if (!ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, FALSE))
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
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
