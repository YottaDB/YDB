/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
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
#include <sys/sem.h>
#include "repl_instance.h"
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
#include "ftok_sems.h"
#include "gtmmsg.h"
#include "repl_msg.h"
#include "gtmsource.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#include "gtmio.h"
#endif

#define GTMRECV_WAIT_FOR_SHUTDOWN	(1000 - 1) /* ms, almost 1s */

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	uint4			process_id;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			recvpool_shmid;
GBLREF	gtmrecv_options_t	gtmrecv_options;
GBLREF	boolean_t		is_rcvr_server;
GBLREF	int			gtmrecv_srv_count;
GBLREF	void			(*call_on_signal)();
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];

error_def(ERR_RECVPOOLSETUP);
error_def(ERR_TEXT);

int gtmrecv_shutdown(boolean_t auto_shutdown, int exit_status)
{
	uint4           savepid;
	boolean_t       shut_upd_too = FALSE, was_crit;
	int             status, save_errno;
	unix_db_info	*udi;

	udi = (unix_db_info *)FILE_INFO(recvpool.recvpool_dummy_reg);
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
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Error grabbing receive pool control semaphore : %s. "
					"Shutdown not complete\n", STRERROR(save_errno));
			repl_inst_ftok_sem_release();
			return ABNORMAL_SHUTDOWN;
		}
	} else
	{	/* ftok semaphore and recvpool access semaphore should already be held from the previous call to "recvpool_init" */
		assert(udi->grabbed_ftok_sem);
		assert(holds_sem[RECV][RECV_POOL_ACCESS_SEM]);
		/* We do not want to hold the options semaphore to avoid deadlocks with receiver server startup (C9F12-002766) */
		assert(!holds_sem[RECV][RECV_SERV_OPTIONS_SEM]);
		recvpool.gtmrecv_local->shutdown = SHUTDOWN;
		/* Wait for receiver server to die. But before that release ftok semaphore and receive pool access control
		 * semaphore. This way, other processes (either in this environment or a different one) don't encounter startup
		 * issues. However, to ensure that a concurrent argument-less rundown doesn't remove these semaphores (in case they
		 * are orphaned), increment the counter semaphore.
		 */
		if (0 != (status = incr_sem(RECV, RECV_SERV_COUNT_SEM)))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Could not acquire Receive Pool counter semaphore : %s. "
							"Shutdown did not complete\n", STRERROR(save_errno));
			/* Even though we hold the FTOK and RECV_POOL_ACCESS_SEM before entering this function (as ensured by
			 * asserts above), it is safe to release them in case of a premature error (like this one). The caller
			 * doesn't rely on the semaphores being held and this function is designed to release these semaphores
			 * eventually anyways (after gtmrecv_ipc_cleanup())
			 */
			repl_inst_ftok_sem_release();
			status = rel_sem(RECV, RECV_POOL_ACCESS_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
		}
		if (0 != (status = rel_sem(RECV, RECV_POOL_ACCESS_SEM)))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Could not release Receive Pool access control semaphore : %s. "
							"Shutdown did not complete\n", STRERROR(save_errno));
			repl_inst_ftok_sem_release(); /* see comment above for why this is okay */
			status = decr_sem(RECV, RECV_SERV_COUNT_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
		}
		repl_inst_ftok_sem_release();
		while((SHUTDOWN == recvpool.gtmrecv_local->shutdown)
				&& (0 < (savepid = recvpool.gtmrecv_local->recv_serv_pid))
				&& is_proc_alive(savepid, 0))
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
		/* (Re)Grab the ftok semaphore and recvpool access control semaphore IN THAT ORDER (to avoid deadlocks) */
		repl_inst_ftok_sem_lock();
#		ifdef DEBUG
		/* Sleep for a few seconds to test for concurrent argument-less RUNDOWN to ensure that the latter doesn't remove
		 * the RECV_POOL_ACCESS_SEM under the assumption that it is orphaned.
		 */
		if (gtm_white_box_test_case_enabled && (WBTEST_LONGSLEEP_IN_REPL_SHUTDOWN == gtm_white_box_test_case_number))
		{
			DBGFPF((stderr, "GTMRECV_SHUTDOWN is about to start long sleep\n"));
			LONG_SLEEP(10);
		}
#		endif
		if (0 != (status = grab_sem(RECV, RECV_POOL_ACCESS_SEM)))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Could not acquire Receive Pool access control semaphore : %s. "
							"Shutdown did not complete\n", STRERROR(save_errno));
			repl_inst_ftok_sem_release();
			status = decr_sem(RECV, RECV_SERV_COUNT_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
		}
		/* Now that semaphores are acquired, decrement the counter semaphore */
		if (0 != (status = decr_sem(RECV, RECV_SERV_COUNT_SEM)))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Could not release Receive Pool counter semaphore : %s. "
							"Shutdown did not complete\n", STRERROR(save_errno));
			repl_inst_ftok_sem_release();
			status = rel_sem(RECV, RECV_POOL_ACCESS_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
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
			decr_sem(RECV, UPD_PROC_COUNT_SEM);
			decr_sem(RECV, RECV_SERV_COUNT_SEM);
		}
		rel_sem_immediate( RECV, RECV_POOL_ACCESS_SEM);
	} else
	{	/* Receive Pool and Access Control Semaphores removed. Invalidate corresponding fields in file header */
		assert(!udi->s_addrs.hold_onto_crit);
		was_crit = udi->s_addrs.now_crit;
		/* repl_inst_recvpool_reset inturn invokes repl_inst_flush_filehdr which expects the caller to grab journal pool
		 * lock if journal pool is available.
		*/
		if ((NULL != jnlpool.jnlpool_ctl) && !was_crit)
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		repl_inst_recvpool_reset();
		if ((NULL != jnlpool.jnlpool_ctl) && !was_crit)
			rel_lock(jnlpool.jnlpool_dummy_reg);
	}
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
