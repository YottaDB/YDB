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

#include "mdef.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "error.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "cli.h"
#include "iosp.h"
#include "repl_log.h"
#include "repl_errno.h"
#include "gtm_event_log.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_filter.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "is_proc_alive.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "sgtm_putmsg.h"
#include "repl_comm.h"
#include "repl_instance.h"
#include "ftok_sems.h"

GBLDEF boolean_t	gtmsource_logstats = FALSE, gtmsource_pool2file_transition = FALSE;
GBLDEF int		gtmsource_filter = NO_FILTER;
GBLDEF boolean_t	update_disable = FALSE;

GBLREF gtmsource_options_t	gtmsource_options;
GBLREF gtmsource_state_t	gtmsource_state;
GBLREF boolean_t        is_src_server;
GBLREF jnlpool_addrs 	jnlpool;
GBLREF uint4		process_id;
GBLREF int		gtmsource_sock_fd;
GBLREF int		gtmsource_log_fd;
GBLREF FILE		*gtmsource_log_fp;
GBLREF int		gtmsource_statslog_fd;
GBLREF FILE		*gtmsource_statslog_fp;
GBLREF gd_addr		*gd_header;
GBLREF void             (*call_on_signal)();
GBLREF seq_num		gtmsource_save_read_jnl_seqno, seq_num_zero;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF repl_msg_ptr_t	gtmsource_msgp;
GBLREF int		gtmsource_msgbufsiz;
GBLREF unsigned char	*gtmsource_tcombuff_start;
GBLREF uchar_ptr_t	repl_filter_buff;
GBLREF int		repl_filter_bufsiz;
GBLREF int		gtmsource_srv_count;

int gtmsource()
{
	gd_region	*reg, *region_top;
	sgmnt_addrs	*csa;
	boolean_t	jnlpool_inited, all_files_open;
	int		semval;
	int		status, log_init_status, waitpid_res, save_errno;
	pid_t           pid, ppid, procgp, sempid;
	seq_num		resync_seqno;
	char		print_msg[1024];

	error_def(ERR_NOTALLDBOPN);
	error_def(ERR_JNLPOOLSETUP);
	error_def(ERR_REPLPARSE);
	error_def(ERR_REPLLOG);
	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);
	error_def(ERR_REPLINFO);

	memset((uchar_ptr_t)&jnlpool, 0, sizeof(jnlpool_addrs));
	call_on_signal = gtmsource_sigstop;
	ESTABLISH_RET(gtmsource_ch, SS_NORMAL);
	if (-1 == gtmsource_get_opt())
		rts_error(VARLSTCNT(1) ERR_REPLPARSE);
	jnlpool_init(GTMSOURCE, gtmsource_options.start, &jnlpool_inited);
	/* When gtmsource_options.start is TRUE,
	 *	jnlpool_inited == TRUE ==> fresh start, and
	 *	jnlpool_inited == FALSE ==> start after a crash
	 */
	if (!gtmsource_options.start)
	{
		if (gtmsource_options.shut_down)
			gtmsource_exit(gtmsource_shutdown(FALSE, NORMAL_SHUTDOWN) - NORMAL_SHUTDOWN);
		else if (gtmsource_options.activate)
			gtmsource_exit(gtmsource_mode_change(GTMSOURCE_MODE_ACTIVE) - NORMAL_SHUTDOWN);
		else if (gtmsource_options.deactivate)
			gtmsource_exit(gtmsource_mode_change(GTMSOURCE_MODE_PASSIVE) - NORMAL_SHUTDOWN);
		else if (gtmsource_options.checkhealth)
			gtmsource_exit(gtmsource_checkhealth() - NORMAL_SHUTDOWN);
		else if (gtmsource_options.changelog)
			 gtmsource_exit(gtmsource_changelog() - NORMAL_SHUTDOWN);
		else if (gtmsource_options.showbacklog)
			gtmsource_exit(gtmsource_showbacklog() - NORMAL_SHUTDOWN);
		else if (gtmsource_options.stopsourcefilter)
			gtmsource_exit(gtmsource_stopfilter() - NORMAL_SHUTDOWN);
		else if (gtmsource_options.update)
			gtmsource_exit(gtmsource_secnd_update(FALSE) - NORMAL_SHUTDOWN);
		else
			gtmsource_exit(gtmsource_statslog() - NORMAL_SHUTDOWN);
	}
	assert(gtmsource_options.start);
	strcpy(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
	jnlpool.gtmsource_local->mode = gtmsource_options.mode;
	if (GTMSOURCE_MODE_ACTIVE == gtmsource_options.mode)
	{
		jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT] =
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT];
		jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD] =
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD];
		jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD] =
			gtmsource_options.connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD];
		jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_ALERT_PERIOD] =
			gtmsource_options.connect_parms[GTMSOURCE_CONN_ALERT_PERIOD];
		jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD] =
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD];
		jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT] =
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT];
	}
	log_init_status = repl_log_init(REPL_GENERAL_LOG, &gtmsource_log_fd, NULL, gtmsource_options.log_file, NULL);
	assert(SS_NORMAL == log_init_status);
	repl_log_fd2fp(&gtmsource_log_fp, gtmsource_log_fd);
	/* If previous shutdown did not complete successfully and
	 * jnlpool was left lying around, do not proceed
	 */
	if (!jnlpool_inited && NO_SHUTDOWN != jnlpool.gtmsource_local->shutdown)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO : Previous source server did not complete shutdown."
				"Resetting shutdown related fields\n");
		jnlpool.gtmsource_local->shutdown = NO_SHUTDOWN;
	}
#ifndef REPL_DEBUG_NOBACKGROUND
	if (0 > (pid = fork()))
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Could not fork source server"), errno);
	else if (0 != pid)
	{
		/* Parent */
		rel_sem(SOURCE, SRC_SERV_COUNT_SEM);
		while (0 == (semval = get_sem_info(SOURCE, SRC_SERV_COUNT_SEM, SEM_INFO_VAL)) && is_proc_alive(pid, 0))
		{
			/* To take care of reassignment of PIDs, the while
			 * condition should be && with the condition
			 * (PPID of pid == process_id)
			 */
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_SRV_START);
			WAITPID(pid, &status, WNOHANG, waitpid_res); /* Release defunct child if dead */
		}
		if (0 <= semval)
		{
			if (0 != (save_errno = rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM)))
				rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error in rel_sem"), save_errno);
			if (0 != (save_errno = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM)))
				rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error in rel_sem"), save_errno);
		}
		/*
		 * If the parent is killed (or crashes) between the fork
		 * and exit, checkhealth may not detect that startup
		 * is in progress - parent forks and dies, the system will
		 * release sem 0 and 1, checkhealth might test the value
		 * of sem 1 before the child grabs sem 1.
		 */
		gtmsource_exit(1 == semval ? SRV_ALIVE : SRV_ERR);
	}

	is_src_server = TRUE;
	process_id = getpid();
	ppid = getppid();
	jnlpool.gtmsource_local->gtmsource_pid = process_id;
	if (-1 == (procgp = setsid()))
		send_msg(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Source server error in setsid"), errno);

#endif /* REPL_DEBUG_NOBACKGROUND */

	REPL_DPRINT1("Setting up regions\n");
	gvinit();

#ifdef GTMSOURCE_OPEN_DB_RDONLY
	/* Open the regions in read-only mode */
	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
		reg->read_only = TRUE;
#endif
	/* We use the same code dse uses to open all regions but we must make sure
	 * they are all open before proceeding.
	 */
	all_files_open = region_init(FALSE);
	if (!all_files_open)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_NOTALLDBOPN);
		gtmsource_autoshutdown();
	}
	if (jnlpool_inited)
		gtmsource_seqno_init();

#ifndef REPL_DEBUG_NOBACKGROUND
	if (!ftok_sem_get(jnlpool.jnlpool_dummy_reg, TRUE, REPLPOOL_ID, FALSE))
		rts_error(VARLSTCNT(1) ERR_JNLPOOLSETUP);
	if (!ftok_sem_release(jnlpool.jnlpool_dummy_reg, FALSE, FALSE))
		rts_error(VARLSTCNT(1) ERR_JNLPOOLSETUP);
	/* Lock the source server count semaphore. Its value should be atmost 1. */
	do
	{
		if (0 == (status = grab_sem_immediate(SOURCE, SRC_SERV_COUNT_SEM)))
			break;
		save_errno = REPL_SEM_ERRNO;
		SHORT_SLEEP(GTMSOURCE_WAIT_FOR_SRV_START);
	} while (REPL_SEM_NOT_GRABBED1 && (sempid = get_sem_info(SOURCE, SRC_SERV_COUNT_SEM, SEM_INFO_PID)) == ppid);
	if (0 != status)
	{
		if (REPL_SEM_NOT_GRABBED1 && 0 < sempid)
		{
			REPL_DPRINT2("Process %d is attempting source server startup. Giving way to that process. "
				     "Exiting...\n", sempid);
			gtmsource_exit(ABNORMAL_SHUTDOWN);
		} else
		{
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL((REPL_SEM_NOT_GRABBED1) ?
					"Journal pool semop failure in child source server" :
					"Journal pool semctl (GETPID) failure in child source server"), REPL_SEM_ERRNO);
		}
	}
#else
	jnlpool.gtmsource_local->gtmsource_pid = process_id;
	if (0 != (save_errno = rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM)))
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error in rel_sem_immediate"), save_errno);
	if (0 != (save_errno = rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM)))
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error in rel_sem_immediate"), save_errno);
#endif /* REPL_DEBUG_NOBACKGROUND */

	gtmsource_srv_count++;
	gtmsource_secnd_update(TRUE);
#ifndef GTMSOURCE_OPEN_DB_RDONLY
	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		if (reg->read_only && REPL_ENABLED(FILE_INFO(reg)->s_addrs.hdr))
		{
			gtm_putmsg(VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				   RTS_ERROR_LITERAL("Source Server does not have write permissions to one or "
					             "more database files that are replicated"));
			gtmsource_autoshutdown();
		}
	}
#endif
	gtm_event_log_init();
	if (jnlpool_inited)
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2,
			    RTS_ERROR_LITERAL("GTM Replication Source Server : Fresh Start"));
	else
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2,
			    RTS_ERROR_LITERAL("GTM Replication Source Server : Crash Restart"));
	repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
	gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLINFO", print_msg);
	jnl_setver();
	do
	{	/* If mode is passive, go to sleep. Wakeup every now and
		 * then and check to see if I have to become active */
		gtmsource_state = GTMSOURCE_START;
		while (jnlpool.gtmsource_local->mode == GTMSOURCE_MODE_PASSIVE
		       		&& jnlpool.gtmsource_local->shutdown == NO_SHUTDOWN)
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_MODE_CHANGE)
			;
		if (GTMSOURCE_MODE_PASSIVE == jnlpool.gtmsource_local->mode)
		{
			/* Shutdown initiated */
			assert(jnlpool.gtmsource_local->shutdown == SHUTDOWN);
			sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2,
				    RTS_ERROR_LITERAL("GTM Replication Source Server Shutdown signalled"));
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
			gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLINFO", print_msg);
			break;
		}
		gtmsource_poll_actions(FALSE);
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
			continue;
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2,
			    RTS_ERROR_LITERAL("GTM Replication Source Server now in ACTIVE mode"));
		repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
		gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLINFO", print_msg);
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
		{
			TP_CHANGE_REG(reg);
			wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
		}
		gtmsource_ctl_init();
		if (SS_NORMAL != (status = gtmsource_alloc_tcombuff()))
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				  RTS_ERROR_LITERAL("Error allocating initial tcom buffer space. Malloc error"), status);
		gtmsource_filter = NO_FILTER;
		if ('\0' != jnlpool.gtmsource_local->filter_cmd[0])
		{
			if (SS_NORMAL == (status = repl_filter_init(jnlpool.gtmsource_local->filter_cmd)))
				gtmsource_filter |= EXTERNAL_FILTER;
			else
			{
				if (EREPL_FILTERSTART_EXEC == repl_errno)
					gtmsource_exit(ABNORMAL_SHUTDOWN);
			}
		}
		grab_lock(jnlpool.jnlpool_dummy_reg);
		QWASSIGN(resync_seqno, seq_num_zero);
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
		{
			assert(reg->open);
			csa = &FILE_INFO(reg)->s_addrs;
			if (REPL_ENABLED(csa->hdr))
			{
				if (QWLT(resync_seqno, csa->hdr->resync_seqno))
					QWASSIGN(resync_seqno, csa->hdr->resync_seqno);
			}
		}
		if (QWGT(jnlpool.gtmsource_local->read_jnl_seqno, resync_seqno))         /* source server terminated before it */
			QWASSIGN(resync_seqno, jnlpool.gtmsource_local->read_jnl_seqno); /* set resync_seqno in the file headers */
		QWASSIGN(jnlpool.gtmsource_local->read_jnl_seqno, resync_seqno);
		QWASSIGN(jnlpool.gtmsource_local->read_addr, jnlpool.jnlpool_ctl->write_addr);
		jnlpool.gtmsource_local->read = jnlpool.jnlpool_ctl->write;
		jnlpool.gtmsource_local->read_state = READ_POOL;
		if (QWLT(jnlpool.gtmsource_local->read_jnl_seqno, jnlpool.jnlpool_ctl->jnl_seqno))
		{
			jnlpool.gtmsource_local->read_state = READ_FILE;
			QWASSIGN(gtmsource_save_read_jnl_seqno, jnlpool.jnlpool_ctl->jnl_seqno);
			gtmsource_pool2file_transition = TRUE; /* so that we read the latest gener jnl files */
		}
		rel_lock(jnlpool.jnlpool_dummy_reg);

		gtmsource_process();

		/* gtmsource_process returns only when mode needs to be changed to PASSIVE */
		assert(gtmsource_state == GTMSOURCE_CHANGING_MODE);
		gtmsource_ctl_close();
		if (gtmsource_msgp)
		{
			free(gtmsource_msgp);
			gtmsource_msgp = NULL;
			gtmsource_msgbufsiz = 0;
		}
		if (gtmsource_tcombuff_start)
		{
			free(gtmsource_tcombuff_start);
			gtmsource_tcombuff_start = NULL;
		}
		if (repl_filter_buff)
		{
			free(repl_filter_buff);
			repl_filter_buff = NULL;
			repl_filter_bufsiz = 0;
		}
		gtmsource_stop_heartbeat();
		if (gtmsource_sock_fd != -1)
			repl_close(&gtmsource_sock_fd);
		if (gtmsource_filter & EXTERNAL_FILTER)
			repl_stop_filter();
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
		{
			TP_CHANGE_REG(reg);
			wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
		}
	} while (TRUE);
	gtmsource_end();
	return(SS_NORMAL);
}
