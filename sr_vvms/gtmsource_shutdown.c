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

#include <sys/mman.h>
#if !(defined(__MVS__)) && !(defined(VMS))
#include <sys/param.h>
#endif
#include "gtm_socket.h"
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */
#include "gtm_string.h"
#include <descrip.h> /* Required for gtmsource.h */

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
#include "gt_timer.h"
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "is_proc_alive.h"
#include "repl_comm.h"
#include "repl_log.h"

#define	GTMSOURCE_WAIT_FOR_SHUTDOWN	(1000 - 1) /* ms, almost 1 s */

GBLREF jnlpool_addrs 		jnlpool;
GBLREF jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF uint4			process_id;
GBLREF int			gtmsource_srv_count;
GBLREF gtmsource_options_t	gtmsource_options;
GBLREF int4			jnlpool_shmid;
GBLREF boolean_t		is_src_server;
GBLREF void                     (*call_on_signal)();

int gtmsource_shutdown(boolean_t auto_shutdown, int exit_status)
{
	uint4		savepid;
	int		status;

	/*
	 * Significance of shutdown field in gtmsource_local:
	 * This field is initially set to NO_SHUTDOWN. When a command to shut
	 * down the source server is issued, the process initiating the
	 * shutdown sets this field to SHUTDOWN. The Source Server on sensing
	 * that it has to shut down (reads SHUTDOWN in the shutdown field),
	 * flushes the database regions, writes (NORMAL_SHUTDOWN + its exit
	 * value) into this field and exits. On seeing a non SHUTDOWN value
	 * in this field, the process which initiated the shutdown removes the
	 * ipcs and exits with the exit value which is a combination of
	 * gtmsource_local->shutdown and its own exit value.
	 *
	 * Note : Exit values should be positive for error indication,
	 * zero for normal exit.
	 */
	repl_log(stdout, TRUE, TRUE, "Initiating shut down\n");
	call_on_signal = NULL;		/* Don't reenter on error */
	/* Grab the jnlpool access control lock and jnlpool option write lock */
	if (!auto_shutdown || gtmsource_srv_count)
	{
		status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM);
		if (0 == status)
			if (0 > (status = grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM)))
				rel_sem(SOURCE, JNL_POOL_ACCESS_SEM);
	} else /* else if autoshutdown, parent is still holding the control lock, and the option lock,
		* and the child is exiting at startup */
		status = 0;

	if (0 > status)
	{
		repl_log(stderr, TRUE, TRUE,
				"Error grabbing jnlpool access control/jnlpool option write lock : %s. Shutdown not complete\n",
				REPL_SEM_ERROR);
		return (ABNORMAL_SHUTDOWN);
	}

	if (!auto_shutdown)
	{
		/* Wait till shutdown time nears */
		if (0 < gtmsource_options.shutdown_time)
		{
			repl_log(stdout, FALSE, TRUE, "Waiting for %d seconds before signalling shutdown\n",
												gtmsource_options.shutdown_time);
			LONG_SLEEP(gtmsource_options.shutdown_time);
		} else
			repl_log(stdout, FALSE, TRUE, "Signalling shutdown immediate\n");

		jnlpool.gtmsource_local->shutdown = SHUTDOWN;

		/* Wait for source server(s) to die. But release jnlpool access control and src serv options semaphore before
		 * waiting as the concurrently running source server(s) might need these (e.g. if it is in the process of
		 * starting up and invoking the function "gtmsource_secnd_update").
		 */
		if (0 != (status = rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM)))
			repl_log(stderr, TRUE, TRUE, "Error releasing the Source Server Options semaphore : %s\n", REPL_SEM_ERROR);
		if (0 != (status = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM)))
			repl_log(stderr, TRUE, TRUE, "Error releasing the Journal Pool Access semaphore : %s\n", REPL_SEM_ERROR);

		while ((SHUTDOWN == jnlpool.gtmsource_local->shutdown)
				&& (0 < (savepid = jnlpool.gtmsource_local->gtmsource_pid))
				&& is_proc_alive(savepid, 0))
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_SHUTDOWN);

		if (0 != (status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM)))
		{
			repl_log(stderr, TRUE, TRUE, "Error re-grabbing the Journal Pool Access semaphore : %s\n", REPL_SEM_ERROR);
			return (ABNORMAL_SHUTDOWN);
		}
		if (0 != (status = grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM)))
		{
			rel_sem(SOURCE, JNL_POOL_ACCESS_SEM);
			repl_log(stderr, TRUE, TRUE, "Error re-grabbing the Source Server Options semaphore : %s\n",
				REPL_SEM_ERROR);
			return (ABNORMAL_SHUTDOWN);
		}
		exit_status = jnlpool.gtmsource_local->shutdown;
		if (SHUTDOWN == exit_status)
		{
			if (0 == savepid) /* No source server */
				exit_status = NORMAL_SHUTDOWN;
			else /* Source Server crashed */
			{
				repl_log(stderr, FALSE, TRUE,"Source Server exited abnormally. MUPIP RUNDOWN might be warranted\n");
				exit_status = ABNORMAL_SHUTDOWN;
			}
		}
	}

	/*
	 * gtmsource_ipc_cleanup will not be successful unless source server has completely exited.
	 * It relies on SRC_SERV_COUNT_SEM value.
	 */
	if (FALSE == gtmsource_ipc_cleanup(auto_shutdown, &exit_status))
	{
		/* Release rundown, count, and option semaphores */
		if (!auto_shutdown)
			rel_sem_immediate(SOURCE, SRC_SERV_COUNT_SEM);
		rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
		rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
	}
	return (exit_status);
}

static void gtmsource_stop(boolean_t exit)
{
	int	status;

	status = gtmsource_shutdown(TRUE, gtmsource_end1(TRUE)) - NORMAL_SHUTDOWN;
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

void gtmsource_autoshutdown(void)
{
	gtmsource_stop(TRUE);
	return;
}
