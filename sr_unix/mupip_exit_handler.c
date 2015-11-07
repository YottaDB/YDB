/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* The mupip exit handler called on all exits from mupip */
#include "mdef.h"

#include "gtm_ipc.h"
#include <sys/shm.h>
#include <sys/sem.h>
#include "gtm_inet.h"
#include <signal.h>
#include <errno.h>

#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_time.h"
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
#include "gt_timer.h"
#include "util.h"
#include "mutex.h"
#include "gv_rundown.h"
#include "mu_term_setup.h"
#include "mupip_exit.h"
#include "print_exit_stats.h"
#include "ftok_sems.h"
#include "db_ipcs_reset.h"
#include "gtm_unistd.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gtmmsg.h"
#include "secshr_db_clnup.h"
#include "gtmio.h"
#include "repl_shutdcode.h"
#include "op.h"
#include "io.h"
#include "gtmsource_srv_latch.h"

GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	boolean_t		need_core;
GBLREF	boolean_t		created_core;
GBLREF	unsigned int		core_in_progress;
GBLREF	boolean_t		exit_handler_active;
GBLREF	recvpool_addrs		recvpool;
GBLREF	boolean_t		pool_init;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		is_rcvr_server;
GBLREF	boolean_t		is_updproc;
GBLREF	boolean_t		is_updhelper;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	FILE			*updproc_log_fp;
GBLREF	FILE			*updhelper_log_fp;
GBLREF	int			gtmsource_log_fd;
GBLREF	int			gtmrecv_log_fd;
GBLREF	int			updproc_log_fd;
GBLREF	int			updhelper_log_fd;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	upd_helper_entry_ptr_t	helper_entry;
GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			process_id;

void close_repl_logfiles(void);

void mupip_exit_handler(void)
{
	char		err_log[1024];
	FILE		*fp;
	boolean_t	files_closed = TRUE;

	if (exit_handler_active)	/* Don't recurse if exit handler exited */
		return;
	exit_handler_active = TRUE;
	SET_PROCESS_EXITING_TRUE;
	if (jgbl.mupip_journal)
	{
		files_closed = mur_close_files();
		mupip_jnl_recover = FALSE;
	}
	jgbl.dont_reset_gbl_jrec_time = jgbl.forw_phase_recovery = FALSE;
	cancel_timer(0);		/* Cancel all timers - No unpleasant surprises */
	secshr_db_clnup(NORMAL_TERMINATION);
	if (dollar_tlevel)
		OP_TROLLBACK(0);
	if (is_updhelper && NULL != helper_entry) /* haven't had a chance to cleanup, must be an abnormal exit */
	{
		helper_entry->helper_shutdown = ABNORMAL_SHUTDOWN;
		helper_entry->helper_pid = 0; /* vacate my slot */
		helper_entry = NULL;
	}
	if (recvpool.recvpool_ctl)
	{
		SHMDT(recvpool.recvpool_ctl);
		recvpool.recvpool_ctl = NULL;
	}
	gv_rundown(); /* also takes care of detaching from the journal pool */
	/* Log the exit of replication servers. In case they are exiting abnormally, their log file pointers
	 * might not be set up. In that case, use "stderr" for logging.
	 */
	if (is_src_server)
	{
		fp = (NULL != gtmsource_log_fp) ? gtmsource_log_fp : stderr;
		repl_log(fp, TRUE, TRUE, "Source server exiting...\n\n");
	} else if (is_rcvr_server)
	{
		fp = (NULL != gtmrecv_log_fp) ? gtmrecv_log_fp : stderr;
		repl_log(fp, TRUE, TRUE, "Receiver server exiting...\n\n");
	} else if (is_updproc)
	{
		fp = (NULL != updproc_log_fp) ? updproc_log_fp : stderr;
		repl_log(fp, TRUE, TRUE, "Update process exiting...\n\n");
	} else if (is_updhelper)
	{
		fp = (NULL != updhelper_log_fp) ? updhelper_log_fp : stderr;
		repl_log(fp, TRUE, TRUE, "Helper exiting...\n\n");
	} else
		mu_reset_term_characterstics(); /* the replication servers use files for output/error, not terminal */
	flush_pio();
	util_out_close();
	close_repl_logfiles();
	print_exit_stats();
	io_rundown(RUNDOWN_EXCEPT_STD);
	if (need_core && !created_core)
	{
		++core_in_progress;
		DUMP_CORE;	/* This will not return */
	}
	if (!files_closed)
		_exit(EXIT_FAILURE);
}

void close_repl_logfiles()
{
	int	rc;

	if (FD_INVALID != gtmsource_log_fd)
		CLOSEFILE_RESET(gtmsource_log_fd, rc);	/* resets "gtmsource_log_fd" to FD_INVALID */
	if (NULL != gtmsource_log_fp)
		FCLOSE(gtmsource_log_fp, rc);
	if (FD_INVALID != gtmrecv_log_fd)
		CLOSEFILE_RESET(gtmrecv_log_fd, rc);	/* resets "gtmrecv_log_fd" to FD_INVALID */
	if (NULL != gtmrecv_log_fp)
		FCLOSE(gtmrecv_log_fp, rc);
	if (FD_INVALID != updproc_log_fd)
	{
		assert(updproc_log_fd != updhelper_log_fd);
		CLOSEFILE_RESET(updproc_log_fd, rc);	/* resets "updproc_log_fd" to FD_INVALID */
	}
	if (NULL != updproc_log_fp)
		FCLOSE(updproc_log_fp, rc);
	if (FD_INVALID != updhelper_log_fd)
		CLOSEFILE_RESET(updhelper_log_fd, rc);	/* resets "updhelper_log_fd" to FD_INVALID */
	if (NULL != updhelper_log_fp)
		FCLOSE(updhelper_log_fp, rc);
}
