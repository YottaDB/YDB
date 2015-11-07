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

#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_socket.h"
#include <sys/mman.h>
#include <sys/param.h>
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
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "is_proc_alive.h"
#include "repl_comm.h"
#include "repl_log.h"
#include "ftok_sems.h"
#include "gtm_c_stack_trace.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#include "gtmio.h"
#include "anticipatory_freeze.h"
#include "gtm_threadgbl.h"
#endif

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	uint4			process_id;
GBLREF	int			gtmsource_srv_count;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	int4			jnlpool_shmid;
GBLREF	boolean_t		is_src_server;
GBLREF	void			(*call_on_signal)();
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	boolean_t		pool_init;
GBLREF	gd_addr			*gd_header;

error_def(ERR_JNLPOOLSETUP);
error_def(ERR_REPLFTOKSEM);
error_def(ERR_TEXT);

int gtmsource_shutdown(boolean_t auto_shutdown, int exit_status)
{
	boolean_t		all_dead, first_time, sem_incremented, regrab_lock;
	uint4			savepid[NUM_GTMSRC_LCL];
	int			status, shutdown_status, save_errno;
	int4			index, maxindex, lcnt, num_src_servers_running;
	unix_db_info		*udi;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;
#ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#endif
	/* Significance of shutdown field in gtmsource_local:
	 * This field is initially set to NO_SHUTDOWN. When a command to shut down the source server is issued,
	 * the process initiating the shutdown sets this field to SHUTDOWN. The Source Server on sensing
	 * that it has to shut down (reads SHUTDOWN in the shutdown field), flushes the database regions, writes
	 * (NORMAL_SHUTDOWN + its exit value) into this field and exits. On seeing a non SHUTDOWN value
	 * in this field, the process which initiated the shutdown removes the ipcs and exits with the exit value
	 * which is a combination of gtmsource_local->shutdown and its own exit value.
	 *
	 * Note : Exit values should be positive for error indication, zero for normal exit.
	 */
	call_on_signal = NULL;		/* Don't reenter on error */
	assert(pool_init);	/* should have attached to the journal pool before coming here */
	udi = (unix_db_info *)FILE_INFO(jnlpool.jnlpool_dummy_reg);
	if (!auto_shutdown)
	{	/* ftok semaphore and jnlpool access semaphore should already be held from the previous call to "jnlpool_init" */
		assert(udi->grabbed_ftok_sem);
		assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
		if (NULL != jnlpool.gtmsource_local)
		{	/* Shutdown source server for the secondary instance specified in the command line */
			savepid[0] = jnlpool.gtmsource_local->gtmsource_pid;
			/* Set flag to signal concurrently running source server to shutdown */
			jnlpool.gtmsource_local->shutdown = SHUTDOWN;
			repl_log(stdout, TRUE, TRUE, "Initiating SHUTDOWN operation on source server pid [%d] for secondary"
				" instance [%s]\n", savepid[0], jnlpool.gtmsource_local->secondary_instname);
			maxindex = 1;	/* Only one process id to check */
		} else
		{	/* Shutdown ALL source servers that are up and running */
			gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
			for (maxindex = 0, index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
			{
				savepid[index] = gtmsourcelocal_ptr->gtmsource_pid;
				if (0 < savepid[index])
				{
					gtmsourcelocal_ptr->shutdown = SHUTDOWN;
					repl_log(stdout, TRUE, TRUE, "Initiating SHUTDOWN operation on source server pid [%d] "
						"for secondary instance [%s]\n", savepid[index],
						gtmsourcelocal_ptr->secondary_instname);
					maxindex = index + 1;	/* Check at least until pid corresponding to "index" */
				}
			}
		}
		/* Wait for source server(s) to die. But before that release ftok semaphore and jnlpool access control semaphore.
		 * This way, other processes (either in this environment or a different one) don't encounter startup issues.
		 * However, to ensure that a concurrent argument-less rundown doesn't remove these semaphores (in case they
		 * are orphaned), increment the counter semaphore.
		 */
		if (0 != incr_sem(SOURCE, SRC_SERV_COUNT_SEM))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Could not increment Journal Pool counter semaphore : %s. "
							"Shutdown did not complete\n", STRERROR(save_errno));
			/* Even though we hold the FTOK and JNL_POOL_ACCESS_SEM before entering this function (as ensured by
			 * asserts above), it is safe to release them in case of a premature error (like this one). The caller
			 * doesn't rely on the semaphores being held and this function is designed to release these semaphores
			 * eventually anyways (after gtmsource_ipc_cleanup())
			 */
			repl_inst_ftok_sem_release();
			status = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
		}
		if (0 != rel_sem(SOURCE, JNL_POOL_ACCESS_SEM))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Could not release Journal Pool access control semaphore : %s. "
							"Shutdown did not complete\n", STRERROR(save_errno));
			repl_inst_ftok_sem_release(); /* see comment above for why this is okay */
			status = decr_sem(SOURCE, SRC_SERV_COUNT_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
		}
		repl_inst_ftok_sem_release();
		regrab_lock = sem_incremented = TRUE;
		gvinit();	/* Get the gd header*/
		/* Wait for ONE particular or ALL source servers to die */
		repl_log(stdout, TRUE, TRUE, "Waiting for upto [%d] seconds for the source server to shutdown\n",
			GTMSOURCE_MAX_SHUTDOWN_WAITLOOP);
		for (lcnt = 1; GTMSOURCE_MAX_SHUTDOWN_WAITLOOP >= lcnt; lcnt++)
		{
			all_dead = TRUE;
			for (index = 0; index < maxindex; index++)
			{
				if ((0 < savepid[index]) && is_proc_alive(savepid[index], 0))
				{
					all_dead = FALSE;
#					ifdef DEBUG
					if (!(lcnt % 60))
						GET_C_STACK_FROM_SCRIPT("ERR_SHUTDOWN_INFO", process_id, savepid[index], lcnt);
#					endif
				}
			}
			if (!all_dead)
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_SHUTDOWN)
			else
				break;
		}
		if (GTMSOURCE_MAX_SHUTDOWN_WAITLOOP < lcnt)
		{	/* Max timeout over, take stack trace of all the source server(s) which are still running.
			 * Display the list of pids that wont die along with the secondary instances they correspond to.
			 * Users need to kill these pids and reissue the shutdown command for the journal pool to be cleaned up.
			 */
			repl_log(stderr, TRUE, TRUE, "Error : Timed out waiting for following source server process(es) to die\n");
			for (lcnt = 0, index = 0; index < maxindex; index++)
			{
				if ((0 < savepid[index]) && is_proc_alive(savepid[index], 0))
				{
					lcnt++;
					GET_C_STACK_FROM_SCRIPT("ERR_SHUTDOWN", process_id, savepid[index], lcnt);
					if (NULL != jnlpool.gtmsource_local)
					{
						assert(0 == index);
						gtmsourcelocal_ptr = jnlpool.gtmsource_local;
					} else
						gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[index];
					repl_log(stderr, FALSE, FALSE,
						" ---> Source server pid [%d] for secondary instance [%s] is still alive\n",
						savepid[index], gtmsourcelocal_ptr->secondary_instname);
				}
			}
			repl_log(stderr, FALSE, TRUE, "Shutdown cannot proceed. Stop the above processes and reissue "
					"the shutdown command.\n");
			status = decr_sem(SOURCE, SRC_SERV_COUNT_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
		}
	} else
	{
		sem_incremented = FALSE;
		if (gtmsource_srv_count)
		{
			repl_log(stdout, TRUE, TRUE, "Initiating shut down\n");
			/* A non-zero gtmsource_srv_count indicates we are the spawned off child source server. That means we
			 * are not holding any semaphores. More importantly, none of the source server's mainline code holds
			 * the ftok or the access control semaphore anymore. So, even if we reach here due to an external signal
			 * we are guaranteed that we don't hold any semaphores. Assert that.
			 */
			assert(!udi->grabbed_ftok_sem);
			assert(!holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
			regrab_lock = TRUE;
		} else
		{
			assert(udi->grabbed_ftok_sem);
			assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
			/* Do not release lock as this is a case of the source server startup command coming here after the
			 * forked off child source server errored out at startup itself. Just in case the jnlpool has not
			 * yet been initialized (possible if this process created the journal pool) we do not want to
			 * release the lock and let someone else sneak in and see uninitialized data. Better to remove the
			 * journal pool before anyone can come in. Hence hold on to the lock.
			 */
			regrab_lock = FALSE;
		}
	}
	if (regrab_lock)
	{	/* Now that the source servers are shutdown, regrab the FTOK and access control semaphore (IN THAT ORDER to avoid
		 * deadlocks)
		 */
		repl_inst_ftok_sem_lock();
#		ifdef DEBUG
		/* Sleep for a few seconds to test for concurrent argument-less RUNDOWN to ensure that the latter doesn't remove
		 * the JNL_POOL_ACCESS_SEM under the assumption that it is orphaned.
		 */
		if (gtm_white_box_test_case_enabled && (WBTEST_LONGSLEEP_IN_REPL_SHUTDOWN == gtm_white_box_test_case_number))
		{
			DBGFPF((stderr, "GTMSOURCE_SHUTDOWN is about to start long sleep\n"));
			LONG_SLEEP(10);
		}
#		endif
		if (0 > (status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM)))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Could not acquire Journal Pool access control semaphore : %s. "
								"Shutdown not complete\n", STRERROR(save_errno));
			repl_inst_ftok_sem_release();
			status = decr_sem(SOURCE, SRC_SERV_COUNT_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
		}
		/* Now that the locks are re-acquired, decrease the counter sempahore */
		if (sem_incremented && (0 > (status = decr_sem(SOURCE, SRC_SERV_COUNT_SEM))))
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Could not decrement Journal Pool counter semaphore : %s."
								"Shutdown not complete\n", STRERROR(save_errno));
			repl_inst_ftok_sem_release();
			status = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM);
			assert(0 == status);
			return ABNORMAL_SHUTDOWN;
		}
	}
	if (!auto_shutdown)
	{
		first_time = TRUE;
		for (index = 0; index < maxindex; index++)
		{
			if (NULL != jnlpool.gtmsource_local)
			{
				assert(0 == index);
				gtmsourcelocal_ptr = jnlpool.gtmsource_local;
			} else
				gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[index];
			exit_status = gtmsourcelocal_ptr->shutdown;
			if (SHUTDOWN == exit_status)
			{
				if (0 == savepid[index]) /* No source server */
					exit_status = NORMAL_SHUTDOWN;
				else /* Source Server crashed */
				{
					repl_log(stderr, first_time, TRUE, "Source Server pid [%d] (secondary instance [%s])"
						" exited abnormally. MUPIP RUNDOWN might be warranted\n",
						savepid[index], gtmsourcelocal_ptr->secondary_instname);
					first_time = FALSE;
				}
			}
		}
		if (!first_time)	/* At least one source server did not exit normally. Reset "exit_status" */
			exit_status = ABNORMAL_SHUTDOWN;
	}
	shutdown_status = exit_status;
	/* gtmsource_ipc_cleanup will not be successful unless source server has completely exited.
	 * It relies on SRC_SERV_COUNT_SEM value. One thing to note here is that if shutdown of a specific source server
	 * is requested and that is successfully shutdown we should return NORMAL_SHUTDOWN if other source servers
	 * are running (currently returned as an ABNORMAL_SHUTDOWN "exit_status" in "gtmsource_ipc_cleanup". But if any
	 * other error occurs in that function causing it to return ABNORMAL_SHUTDOWN, then we should return ABNORMAL_SHUTDOWN
	 * from this function as well.
	 */
	if (FALSE == gtmsource_ipc_cleanup(auto_shutdown, &exit_status, &num_src_servers_running))
		rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
	else
	{	/* Journal Pool and Access Control Semaphores removed. Invalidate corresponding fields in file header */
		assert(NORMAL_SHUTDOWN == exit_status);
		repl_inst_jnlpool_reset();
	}
	if (!ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, FALSE))
		rts_error(VARLSTCNT(1) ERR_JNLPOOLSETUP);
	assert(!num_src_servers_running || (ABNORMAL_SHUTDOWN == exit_status));
	return (((1 == maxindex) && num_src_servers_running) ? shutdown_status : exit_status);
}

void gtmsource_stop(boolean_t exit)
{
	int	status;

	assert(gtmsource_srv_count || is_src_server);
	status = gtmsource_end1(TRUE);
	status = gtmsource_shutdown(TRUE, status) - NORMAL_SHUTDOWN;
	if (exit)
		gtmsource_exit(status);
	return;
}

void gtmsource_sigstop(void)
{
	if (is_src_server)
		gtmsource_stop(FALSE);
	return;
}

