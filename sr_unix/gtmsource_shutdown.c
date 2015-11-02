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
#include "gtm_string.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_socket.h"

#include <sys/mman.h>
#if !(defined(__MVS__)) && !(defined(VMS))
#include <sys/param.h>
#endif
#include <sys/time.h>
#include <errno.h>
#ifdef UNIX
#include <sys/sem.h>
#include "repl_instance.h"
#endif
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

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
#ifdef UNIX
#include "ftok_sems.h"
#endif
#include "gtm_c_stack_trace.h"

#define	GTMSOURCE_WAIT_FOR_SHUTDOWN	(1000 - 1) /* ms, almost 1 s */

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
	boolean_t		all_dead, first_time, regrab_lock;
	uint4			savepid[NUM_GTMSRC_LCL];
	int			status, shutdown_status;
	int4			index, maxindex, lcnt, num_src_servers_running;
	unix_db_info		*udi;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;

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
		/* Wait for source server(s) to die. But release ftok semaphore and jnlpool access control semaphore before
		 * waiting as the concurrently running source server(s) might need these (e.g. if it is about to call the
		 * function "gtmsource_set_next_histinfo_seqno").
		 */
		if (0 != rel_sem(SOURCE, JNL_POOL_ACCESS_SEM))
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_LITERAL("Error in source server shutdown rel_sem"),
				REPL_SEM_ERRNO);
		repl_inst_ftok_sem_release();
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
			{
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_SHUTDOWN);
			} else
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
			assert (FALSE);
			return (ABNORMAL_SHUTDOWN);
		}
		/* At this point, the source server process is dead. The call to "gtmsource_ipc_cleanup" below relies on this. */
		regrab_lock = TRUE;
	} else
	{
		if (gtmsource_srv_count)
		{
			repl_log(stdout, TRUE, TRUE, "Initiating shut down\n");
			if (udi->grabbed_ftok_sem)
			{	/* Possible if we were holding the ftok semaphore and the journal pool lock "grab_lock" and
				 * got a SIG-15 before we did the "rel_lock" (while still holding the ftok semaphore). The
				 * "rel_lock" would have invoked "deferred_signal_handler" which would have in turn recognized
				 * the signal and triggered shutdown processing all the while holding the ftok semaphore.
				 * Release the ftok semaphore and grab it again.
				 */
				repl_inst_ftok_sem_release();
			}
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
	{	/* (Re)Grab the ftok semaphore and jnlpool access control semaphore IN THAT ORDER (to avoid deadlocks) */
		repl_inst_ftok_sem_lock();
		status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM);
		if (0 > status)
		{
			repl_log(stderr, TRUE, TRUE,
				"Error grabbing jnlpool access control semaphore : %s. Shutdown not complete\n", REPL_SEM_ERROR);
			return (ABNORMAL_SHUTDOWN);
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
	else if (NORMAL_SHUTDOWN == exit_status)
		repl_inst_jnlpool_reset();
	if (!ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, FALSE))
		rts_error(VARLSTCNT(1) ERR_JNLPOOLSETUP);
	assert(!num_src_servers_running || (ABNORMAL_SHUTDOWN == exit_status));
	return (((1 == maxindex) && num_src_servers_running) ? shutdown_status : exit_status);
}

void gtmsource_stop(boolean_t exit)
{
	int	status;

	assert(gtmsource_srv_count);	/* This is a source server process */
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

