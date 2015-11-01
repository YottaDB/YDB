/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* The mupip exit handler called on all exits from mupip */
#include "mdef.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtmsiginfo.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "repl_sem.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "error.h"
#include "gtmimagename.h"
#include "eintr_wrappers.h"
#include "repl_log.h"
#include "io.h"			/* for gtmsecshr.h */
#include "gtmsecshr.h"
#include "gt_timer.h"
#include "util.h"
#include "mutex.h"
#include "gv_rundown.h"
#include "mu_term_setup.h"
#include "mupip_exit.h"
#include "print_exit_stats.h"
#include "ftok_sems.h"
#include "gtm_unistd.h"

GBLREF int			process_exiting;
GBLREF boolean_t	        mupip_jnl_recover;
GBLREF boolean_t                need_core;
GBLREF boolean_t                created_core;
GBLREF boolean_t	        core_in_progress;
GBLREF boolean_t	        exit_handler_active;
GBLREF recvpool_addrs 	        recvpool;
GBLREF jnlpool_addrs 	        jnlpool;
GBLREF boolean_t	        pool_init;
GBLREF jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF boolean_t        	is_src_server;
GBLREF boolean_t        	is_rcvr_server;
GBLREF boolean_t		is_updproc;
GBLREF FILE			*gtmsource_log_fp;
GBLREF FILE			*gtmrecv_log_fp;
GBLREF FILE			*updproc_log_fp;
GBLREF int			gtmsource_log_fd;
GBLREF int			gtmsource_statslog_fd;
GBLREF FILE			*gtmsource_statslog_fp;
GBLREF int			gtmrecv_log_fd;
GBLREF int			gtmrecv_statslog_fd;
GBLREF FILE			*gtmrecv_statslog_fp;
GBLREF int			updproc_log_fd;
GBLREF int			gtmsource_srv_count;
GBLREF int			gtmrecv_srv_count;
GBLREF gd_region		*standalone_reg;
GBLREF gd_region		*gv_cur_region;
GBLREF gd_region		*ftok_sem_reg;
GBLREF boolean_t		forw_phase_recovery;

void close_repl_logfiles(void);

void mupip_exit_handler(void)
{
	char	err_log[1024];
        unix_db_info    	*udi;

	if (exit_handler_active)	/* Don't recurse if exit handler exited */
		return;
	exit_handler_active = TRUE;
	process_exiting = TRUE;
	mupip_jnl_recover = FALSE;
	forw_phase_recovery = FALSE;
	cancel_timer(0);		/* Cancel all timers - No unpleasant surprises */
	secshr_db_clnup(NORMAL_TERMINATION);
        if (jnlpool.jnlpool_ctl)
	{
		rel_lock(jnlpool.jnlpool_dummy_reg);
		mutex_cleanup(jnlpool.jnlpool_dummy_reg);
                SHMDT(jnlpool.jnlpool_ctl);
		jnlpool.jnlpool_ctl = jnlpool_ctl = NULL;
		pool_init = FALSE;
	}
	gv_rundown();
	if (standalone_reg)
		db_ipcs_reset(standalone_reg, TRUE);
        if (recvpool.recvpool_ctl)
	{
                SHMDT(recvpool.recvpool_ctl);
		recvpool.recvpool_ctl = NULL;
	}
	/*
	 * Note:
	 * 	In older versions we used to release replication semaphores here.
	 * 	But it does not really help. We do not want to release them until this process exits.
	 *	We use SEM_UNDO flag for semaphore creation. So when this process will exit,
	 *	OS will automatically release the semaphore value by 1. That is do nothing about them.
	 */
	if (ftok_sem_reg)
	{
		/* This segment of code will be executed by utilities
		 * like mupip integ file/mupip restore etc., which operates on one single region.
		 * In case of an error or, for any other code path, if the ftok semaphore is
		 * grabbed but not released, ftok_sem_reg will have non-null value
		 * and grabbed_ftok_sem will be TRUE.
		 * (We cannot rely on gv_cur_region which is used in so many places in so many ways.)
		 * In case a processed released ftok semaphore lock but did not decrement
		 * the counter, ftok_sem_reg will be NULL. In that case we rely on OS to decrement
		 * the counter when the process exits completely.
		 */
		DEBUG_ONLY(udi = FILE_INFO(ftok_sem_reg);)
		assert(udi->grabbed_ftok_sem);
		ftok_sem_release(ftok_sem_reg, TRUE, TRUE);
	} else
	{
		/* This segment is for replication ftok semaphore cleanup in case of error. */
		if (NULL != jnlpool.jnlpool_dummy_reg)
		{
			udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
			if (udi->grabbed_ftok_sem)
				ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
		}
		if (NULL != recvpool.recvpool_dummy_reg)
		{
			udi = FILE_INFO(recvpool.recvpool_dummy_reg);
			if (udi->grabbed_ftok_sem)
				ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
		}
	}
	mu_reset_term_characterstics();
	/* log the exit of replication servers */
	if (is_src_server)
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server exiting...\n");
	if (is_rcvr_server)
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Receiver server exiting...\n");
	if (is_updproc)
		repl_log(updproc_log_fp, TRUE, TRUE, "Update process exiting...\n");
	util_out_close();
	close_repl_logfiles();
	print_exit_stats();
	if (need_core && !created_core)
	{
		core_in_progress = TRUE;
		DUMP_CORE;	/* This will not return */
	}
}

void close_repl_logfiles()
{
	int		fclose_res;

	if (gtmsource_statslog_fd != -1)
		close(gtmsource_statslog_fd);
	if (gtmsource_statslog_fp != NULL)
		FCLOSE(gtmsource_statslog_fp, fclose_res);
	if (gtmsource_log_fd != -1)
		close(gtmsource_log_fd);
	if (gtmsource_log_fp != NULL)
		FCLOSE(gtmsource_log_fp, fclose_res);
	if (gtmrecv_statslog_fd != -1)
		close(gtmrecv_statslog_fd);
	if (gtmrecv_statslog_fp != NULL)
		FCLOSE(gtmrecv_statslog_fp, fclose_res);
	if (gtmrecv_log_fd != -1)
		close(gtmrecv_log_fd);
	if (gtmrecv_log_fp != NULL)
		FCLOSE(gtmrecv_log_fp, fclose_res);
	if (updproc_log_fd != -1)
		close(updproc_log_fd);
	if (updproc_log_fp != NULL)
		FCLOSE(updproc_log_fp, fclose_res);
}
