/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#include <sys/mman.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include <errno.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "tp.h"
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
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_filter.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "is_proc_alive.h"
#include "gtmmsg.h"
#include "sgtm_putmsg.h"
#include "repl_comm.h"
#include "repl_instance.h"
#include "ftok_sems.h"
#include "ftok_sem_incrcnt.h"
#include "gt_timer.h"		/* for LONG_SLEEP macro (hiber_start function prototype) and add_safe_timer_handler */
#include "gtmsource_heartbeat.h" /* for gtmsource_heartbeat_timer */
#include "mutex.h"
#include "gtm_zlib.h"
#include "fork_init.h"
#include "gtmio.h"
#include "util.h"
#include "mu_outofband_setup.h"
#include "mupip_exit.h"
#include "io.h"
#include "getjobnum.h"		/* for SET_PROCESS_ID */
#ifdef GTM_TLS
#include "gtm_repl.h"
#endif
#ifdef DEBUG
#include "anticipatory_freeze.h"
#include "fake_enospc.h"
#endif

GBLDEF	boolean_t		gtmsource_logstats = FALSE, gtmsource_pool2file_transition = FALSE;
GBLDEF	int			gtmsource_filter = NO_FILTER;
GBLDEF	boolean_t		update_disable = FALSE;
GBLDEF	boolean_t		last_seen_freeze_flag = FALSE;

GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		first_syslog;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	uint4			process_id;
GBLREF	int			gtmsource_sock_fd;
GBLREF	int			gtmsource_log_fd;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	gd_addr			*gd_header;
GBLREF	void			(*call_on_signal)();
GBLREF	seq_num			gtmsource_save_read_jnl_seqno, seq_num_zero;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	int			gtmsource_msgbufsiz;
GBLREF	unsigned char		*gtmsource_tcombuff_start;
GBLREF	boolean_t		is_jnlpool_creator;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	int			gtmsource_srv_count;
GBLREF	gd_region		*ftok_sem_reg;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	IN_PARMS		*cli_lex_in_ptr;
GBLREF	uint4			mutex_per_process_init_pid;
GBLREF	bool			mu_ctrlc_occurred;
GBLREF	io_pair			io_std_device;

error_def(ERR_JNLPOOLSETUP);
error_def(ERR_MUPCLIERR);
error_def(ERR_REPLNORESP);
error_def(ERR_NOTALLDBOPN);
error_def(ERR_NULLCOLLDIFF);
error_def(ERR_REPL0BACKLOG);
error_def(ERR_REPLBACKLOG);
error_def(ERR_REPLCOMM);
error_def(ERR_REPLERR);
error_def(ERR_REPLINFO);
error_def(ERR_REPLINSTFREEZECOMMENT);
error_def(ERR_REPLINSTFROZEN);
error_def(ERR_REPLOFFJNLON);
error_def(ERR_TEXT);
error_def(ERR_SHUT2QUICK);
error_def(ERR_CTRLC);
error_def(ERR_MUNOFINISH);

int gtmsource()
{
	int			status, log_init_status, waitpid_res, save_errno;
	char			print_msg[OUT_BUFF_SIZE - 1], tmpmsg[REPL_MSG_SIZE];
	gd_region		*reg, *region_top;
	sgmnt_addrs		*csa, *repl_csa;
	boolean_t		all_files_open, isalive, ftok_counter_halted;
	pid_t			pid;
	seq_num			chkbklogresult, read_jnl_seqno, jnl_seqno;
	unix_db_info		*udi;
	gtmsource_local_ptr_t	gtmsource_local;
	boolean_t		hrtbt_recvd = FALSE, iszerobacklog = FALSE, this_side_std_null_coll, wasshut2quick = FALSE;
	int			null_fd, rc;
	uint4			gtmsource_pid;
	int			shutdowntime = 0;
	int4			index;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	call_on_signal = gtmsource_sigstop;
	ESTABLISH_RET(gtmsource_ch, SS_NORMAL);
	if (-1 == gtmsource_get_opt())
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUPCLIERR);
	gvinit();
	assert(NULL != gd_header);
	if (gtmsource_options.shut_down)
	{
		mu_outofband_setup();
		shutdowntime = gtmsource_options.shutdown_time;
		if (gtmsource_options.zerobacklog)
		{
			if (0 < shutdowntime)
			{
				repl_log(stdout, TRUE, TRUE, "Initiating ZEROBACKLOG shutdown operation. "
						"Waiting for up to %d second(s) for backlog to clear\n", shutdowntime);
				jnlpool_init(GTMSOURCE, gtmsource_options.start, &is_jnlpool_creator, gd_header);
				if (NULL != jnlpool->gtmsource_local)
					gtmsource_local = jnlpool->gtmsource_local; /* if -INST is specified */
				else
					gtmsource_local = &jnlpool->gtmsource_local_array[0];/* if -INST is not specified */
				/* Loop across all Source Server(s) or the selected Source Server and set hrtbt_recvd to FALSE */
				for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsource_local++)
				{
					if ('\0' == gtmsource_local->secondary_instname[0])
					{
						assert(NULL == jnlpool->gtmsource_local);
						continue;
					}
					gtmsource_pid = gtmsource_local->gtmsource_pid;
					/* gtmsource_pid is 0 for Source Servers that were shut down */
					if ((NULL == jnlpool->gtmsource_local) && (0 == gtmsource_pid))
						continue;
					/* Set hrtbt_recvd to FALSE. If it turns TRUE later, we have a heartbeat confirmation */
					gtmsource_local->hrtbt_recvd = FALSE;
					if (NULL != jnlpool->gtmsource_local)
						break;
				}
				for (; shutdowntime; shutdowntime--)
				{
					LONG_SLEEP(1);
					if (mu_ctrlc_occurred)
					{
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
						mupip_exit(ERR_MUNOFINISH);
					}
					chkbklogresult = gtmsource_checkforbacklog();
					if (0 == chkbklogresult)
					{
						/* There is no backlog. Stop waiting and proceed with the shut down */
						iszerobacklog = TRUE;
						break;
					}
				}
				/* Check and report those Source Servers where there was no heartbeat detected */
				if (!iszerobacklog)
				{
					if (NULL != jnlpool->gtmsource_local)
						gtmsource_local = jnlpool->gtmsource_local;
					else
						gtmsource_local = &jnlpool->gtmsource_local_array[0];
					for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsource_local++)
					{
						if ('\0' == gtmsource_local->secondary_instname[0])
						{
							assert(NULL == jnlpool->gtmsource_local);
							continue;
						}
						gtmsource_pid = gtmsource_local->gtmsource_pid;
						if ((NULL == jnlpool->gtmsource_local) && (0 == gtmsource_pid))
							continue;
						if (!gtmsource_local->hrtbt_recvd)
						{
							sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1, VARLSTCNT(6) ERR_REPLNORESP,
									4, LEN_AND_STR(gtmsource_local->secondary_instname),
									gtmsource_options.shutdown_time);
							repl_log(stdout, TRUE, TRUE, print_msg);
							if (gtmsource_local->connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD] >
									gtmsource_options.shutdown_time)
							{
								sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1,
										VARLSTCNT(6) ERR_SHUT2QUICK, 4,
										gtmsource_options.shutdown_time,
										gtmsource_local->
										connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD],
										LEN_AND_STR(gtmsource_local->secondary_instname));
								repl_log(stdout, TRUE, TRUE, print_msg);
							}
						}
						/* If there is only one instance to process,
						   break for loop in the first iteration */
						if (NULL != jnlpool->gtmsource_local)
							break;
					}

					sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1, VARLSTCNT(1) ERR_REPLBACKLOG);
					repl_log(stdout, TRUE, TRUE, print_msg);
				}
				else
				{
					sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1, VARLSTCNT(1) ERR_REPL0BACKLOG);
					repl_log(stdout, TRUE, TRUE, print_msg);
				}
			}
		} else if (0 < gtmsource_options.shutdown_time)
		{
			repl_log(stdout, TRUE, TRUE, "Waiting for %d second(s) before forcing shutdown\n",
					gtmsource_options.shutdown_time);
			for (; shutdowntime; shutdowntime--)
				{
					LONG_SLEEP(1);
					if (mu_ctrlc_occurred)
					{
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
						mupip_exit(ERR_MUNOFINISH);
					}
				}
		} else
			repl_log(stdout, TRUE, TRUE, "Forcing immediate shutdown\n");
	} else if (gtmsource_options.start)
	{
		repl_log(stdout, TRUE, TRUE, "Initiating START of source server for secondary instance [%s]\n",
			gtmsource_options.secondary_instname);
	}
	if (gtmsource_options.activate && (ROOTPRIMARY_SPECIFIED == gtmsource_options.rootprimary))
	{	/* MUPIP REPLIC -SOURCE -ACTIVATE -UPDOK has been specified. We need to open the gld and db regions now
		 * in case this is a secondary -> primary transition. This is so we can later switch journal files in all
		 * journaled regions when the transition actually happens inside "gtmsource_rootprimary_init". But since
		 * we have not yet done a "jnlpool_init", we don't know if updates are disabled in it or not. Although we
		 * need to do the gld/db open only if updates are currently disabled in the jnlpool, we do this always
		 * because once we do a jnlpool_init, we will come back with the ftok on the jnlpool held and that has
		 * issues with later db open since we will try to hold the db ftok as part of db open and the ftok logic
		 * currently has assumptions that a process holds only one ftok at any point in time.
		 */
		all_files_open = region_init(FALSE);
		if (!all_files_open)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTALLDBOPN);
			gtmsource_exit(ABNORMAL_SHUTDOWN);
		}
	}
	if (!holds_sem[SOURCE][JNL_POOL_ACCESS_SEM])
	{
		assert((NULL == jnlpool) || !jnlpool->pool_init);
		jnlpool_init(GTMSOURCE, gtmsource_options.start, &is_jnlpool_creator, gd_header);
		/* is_jnlpool_creator == TRUE ==> this process created the journal pool
		 * is_jnlpool_creator == FALSE ==> journal pool already existed and this process simply attached to it.
		 */
	}
	assert(jnlpool && jnlpool->pool_init);
	if (gtmsource_options.shut_down)
		gtmsource_exit(gtmsource_shutdown(FALSE, NORMAL_SHUTDOWN) - NORMAL_SHUTDOWN);
	else if (gtmsource_options.activate)
		gtmsource_exit(gtmsource_mode_change(GTMSOURCE_MODE_ACTIVE_REQUESTED) - NORMAL_SHUTDOWN);
	else if (gtmsource_options.deactivate)
		gtmsource_exit(gtmsource_mode_change(GTMSOURCE_MODE_PASSIVE_REQUESTED) - NORMAL_SHUTDOWN);
	else if (gtmsource_options.checkhealth)
		gtmsource_exit(gtmsource_checkhealth() - NORMAL_SHUTDOWN);
	else if (gtmsource_options.changelog)
		 gtmsource_exit(gtmsource_changelog() - NORMAL_SHUTDOWN);
	else if (gtmsource_options.showbacklog)
		gtmsource_exit(gtmsource_showbacklog() - NORMAL_SHUTDOWN);
	else if (gtmsource_options.stopsourcefilter)
		gtmsource_exit(gtmsource_stopfilter() - NORMAL_SHUTDOWN);
	else if (gtmsource_options.jnlpool)
		gtmsource_exit(gtmsource_jnlpool() - NORMAL_SHUTDOWN);
	else if (gtmsource_options.losttncomplete)
		gtmsource_exit(gtmsource_losttncomplete() - NORMAL_SHUTDOWN);
	else if (gtmsource_options.needrestart)
		gtmsource_exit(gtmsource_needrestart() - NORMAL_SHUTDOWN);
	else if (gtmsource_options.showfreeze)
		gtmsource_exit(gtmsource_showfreeze() - NORMAL_SHUTDOWN);
	else if (gtmsource_options.setfreeze)
		gtmsource_exit(gtmsource_setfreeze() - NORMAL_SHUTDOWN);
	else if (!gtmsource_options.start)
	{
		assert(CLI_PRESENT == cli_present("STATSLOG"));
		gtmsource_exit(gtmsource_statslog() - NORMAL_SHUTDOWN);
	}
	assert(gtmsource_options.start);
#	ifndef REPL_DEBUG_NOBACKGROUND
	/* Set "child_server_running" to FALSE before forking off child. Wait for it to be set to TRUE by the child. */
	gtmsource_local = jnlpool->gtmsource_local;
	gtmsource_local->child_server_running = FALSE;
	udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
	FORK(pid);
	if (0 > pid)
	{
		save_errno = errno;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("Could not fork source server"), save_errno);
	} else if (0 < pid)
	{	/* Parent. Wait until child sets "child_server_running" to FALSE. That is an indication that the child
		 * source server has completed its initialization phase and is all set so the parent command can return.
		 */
		while ((isalive = is_proc_alive(pid, 0)))	/* note : intended assignment */
		{
			if (gtmsource_local->child_server_running)
				break;
			/* To take care of reassignment of PIDs, the while condition should be && with the condition
			 * (PPID of pid == process_id)
			 */
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_SRV_START);
			WAITPID(pid, &status, WNOHANG, waitpid_res); /* Release defunct child if dead */
		}
		if (isalive)
		{	/* Child process is alive and started with no issues */
			if (0 != (save_errno = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM)))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Error in rel_sem"), save_errno);
			/* If the child source server process got a ftok counter overflow, it would have recorded that in
			 * jnlpool->jnlpool_ctl->ftok_counter_halted. Decrement the ftok counter only if neither we nor the
			 * child process got a counter overflow.
			 */
			ftok_sem_release(jnlpool->jnlpool_dummy_reg, udi->counter_ftok_incremented
									&& !jnlpool->jnlpool_ctl->ftok_counter_halted, TRUE);
		} else
		{	/* Child source server process errored out at startup and is no longer alive.
			 * If we were the one who created the journal pool, let us clean it up.
			 */
			repl_log(stdout, TRUE, TRUE, "Source server startup failed. See source server log file\n");
			if (is_jnlpool_creator)
				status = gtmsource_shutdown(TRUE, NORMAL_SHUTDOWN);
		}
		/* If the parent is killed (or crashes) between the fork and exit, checkhealth may not detect that startup
		 * is in progress - parent forks and dies, the system will release sem 0 and 1, checkhealth might test the
		 * value of sem 1 before the child grabs sem 1.
		 */
		gtmsource_exit(isalive ? SRV_ALIVE : SRV_ERR);
	}
	/* At this point, the parent source server startup command would have done an "io_init" and attached to
	 * fds = 0, 1 and 2. The child source server is going to close fd=0 here (i.e. point to /dev/null) and
	 * about to close 1 and 2 a little later in repl_log_init (i.e. point to the source server log file).
	 * Therefore any io initializations done in io_init (e.g. io_std_device etc.) are soon going to become
	 * invalid. Since io_std_device.out is checked in the "util_out_print_vaparm" function to decide whether
	 * to use M IO (write through the device opened by io_init) vs writing directly to stderr (fd=2), it is
	 * best to clear io_std_device.out for the child process. That way any errors in case they occur go to
	 * stderr (if available) instead of /dev/null.
	 */
	io_std_device.out = NULL;
	/* Point stdin to /dev/null */
	OPENFILE("/dev/null", O_RDONLY, null_fd);
	if (0 > null_fd)
		RTS_ERROR_CSA_ABT(NULL, ERR_REPLERR, RTS_ERROR_LITERAL("Failed to open /dev/null for read"), errno, 0);
	DUP2(null_fd, 0, rc);
	if (0 > rc)
		RTS_ERROR_CSA_ABT(NULL, ERR_REPLERR, RTS_ERROR_LITERAL("Failed to set stdin to /dev/null"), errno, 0);
	CLOSEFILE(null_fd, rc);
	if (0 > rc)
		RTS_ERROR_CSA_ABT(NULL, ERR_REPLERR, RTS_ERROR_LITERAL("Failed to close /dev/null"), errno, 0);
	/* The parent process (source server startup command) will be holding the ftok semaphore and jnlpool access semaphore
	 * at this point. The variables that indicate this would have been copied over to the child during the fork. This will
	 * make the child think it is actually holding them as well when actually it is not. Reset those variables in the child
	 * to ensure they do not misrepresent the holder of those semaphores.
	 */
	ftok_sem_reg = NULL;
	assert(udi->grabbed_ftok_sem);
	udi->grabbed_ftok_sem = FALSE;
	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] = FALSE;
	assert(!holds_sem[SOURCE][SRC_SERV_COUNT_SEM]);
	/* Start child source server initialization. The process might have sent a syslog message already, so set first_syslog
	 * here to force setting image type with the new value of is_src_server. */
	is_src_server = first_syslog = TRUE;
	TREF(error_on_jnl_file_lost) = JNL_FILE_LOST_ERRORS; /* source server should never switch journal files even on errors */
	OPERATOR_LOG_MSG;
	SET_PROCESS_ID;
	/* Initialize mutex socket, memory semaphore etc. before any "grab_lock" is done by this process on the journal pool.
	 * Note that the initialization would already have been done by the parent receiver startup command but we need to
	 * redo the initialization with the child process id.
	 */
	assert(mutex_per_process_init_pid && (mutex_per_process_init_pid != process_id));
	mutex_per_process_init();
	log_init_status = repl_log_init(REPL_GENERAL_LOG, &gtmsource_log_fd, gtmsource_options.log_file);
	assert(SS_NORMAL == log_init_status);
	PRO_ONLY(UNUSED(log_init_status));
	repl_log_fd2fp(&gtmsource_log_fp, gtmsource_log_fd);
	if (-1 == setsid())
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Source server error in setsid"), errno);
#	endif /* REPL_DEBUG_NOBACKGROUND */
	if (ZLIB_CMPLVL_NONE != ydb_zlib_cmp_level)
		gtm_zlib_init();	/* Open zlib shared library for compression/decompression */
	REPL_DPRINT1("Setting up regions\n");
	gvinit();
	/* We use the same code dse uses to open all regions but we must make sure they are all open before proceeding. */
	all_files_open = region_init(FALSE);
	if (!all_files_open)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTALLDBOPN);
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	/* Determine primary side null subscripts collation order */
	/* Also check whether all regions have same null collation order */
	this_side_std_null_coll = -1;
	for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
	{
		assert(reg->open);
		csa = &FILE_INFO(reg)->s_addrs;
		if (this_side_std_null_coll != csa->hdr->std_null_coll)
		{
			if (-1 == this_side_std_null_coll)
				this_side_std_null_coll = csa->hdr->std_null_coll;
			else
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NULLCOLLDIFF);
				gtmsource_exit(ABNORMAL_SHUTDOWN);
			}
		}
		if (!REPL_ALLOWED(csa) && JNL_ALLOWED(csa))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REPLOFFJNLON, 2, DB_LEN_STR(reg));
			gtmsource_exit(ABNORMAL_SHUTDOWN);
		}
		if (reg->read_only && REPL_ALLOWED(csa))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				   RTS_ERROR_LITERAL("Source Server does not have write permissions to one or "
					             "more database files that are replicated"));
			gtmsource_exit(ABNORMAL_SHUTDOWN);
		}
		if ((NULL == csa->jnlpool) && REPL_ALLOWED(csa) && jnlpool->pool_init)
			csa->jnlpool = jnlpool;
	}
	/* Initialize source server alive/dead state related fields in "gtmsource_local" before the ftok semaphore is released */
	gtmsource_local->gtmsource_pid = process_id;
	gtmsource_local->gtmsource_state = GTMSOURCE_START;
	if (is_jnlpool_creator)
	{
		assert(jnlpool && jnlpool->jnlpool_ctl);
		DEBUG_ONLY(jnlpool->jnlpool_ctl->jnlpool_creator_pid = process_id);
		gtmsource_seqno_init(this_side_std_null_coll);
		if (ROOTPRIMARY_SPECIFIED == gtmsource_options.rootprimary)
		{	/* Created the journal pool as a root primary. Append a history record to the replication instance file.
			 * Invoke the function "gtmsource_rootprimary_init" to do that.
			 */
			gtmsource_rootprimary_init(jnlpool->jnlpool_ctl->jnl_seqno);
		}
	}
	/* after this point we can no longer have the case where all the regions are unreplicated/non-journaled. */
#	ifndef REPL_DEBUG_NOBACKGROUND
	/* It is necessary for every process that is using the ftok semaphore to increment the counter by 1. This is used
	 * by the last process that shuts down to delete the ftok semaphore when it notices the counter to be 0.
	 * Note that the parent source server startup command would have done an increment of the ftok counter semaphore
	 * for the replication instance file. But the source server process (the child) that comes here would not have done
	 * that. Do that while the parent is still holding on to the ftok semaphore waiting for our okay.
	 */
	assert(jnlpool && jnlpool->pool_init);
	if (!ftok_sem_incrcnt(jnlpool->jnlpool_dummy_reg, FILE_TYPE_REPLINST, &ftok_counter_halted))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_JNLPOOLSETUP);
	/* Increment the source server count semaphore */
	status = incr_sem(SOURCE, SRC_SERV_COUNT_SEM);
	if (0 != status)
	{
		save_errno = errno;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Counter semaphore increment failure in child source server"), save_errno);
	}
#	else
	if (0 != (save_errno = rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM)))
	{
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error in rel_sem_immediate"), save_errno);
	}
#	endif /* REPL_DEBUG_NOBACKGROUND */

	gtmsource_srv_count++;
	gtmsource_local->child_server_running = TRUE;	/* At this point, the parent startup command will stop waiting for child */
	/* Log source server startup command line first */
	SNPRINTF(tmpmsg, REPL_MSG_SIZE, "%s %s\n", cli_lex_in_ptr->argv[0], cli_lex_in_ptr->in_str);
	repl_log(gtmsource_log_fp, TRUE, TRUE, tmpmsg);

	SNPRINTF(tmpmsg, REPL_MSG_SIZE, "GTM Replication Source Server with Pid [%d] started for Secondary Instance [%s]",
		process_id, gtmsource_local->secondary_instname);
	sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1, VARLSTCNT(4) ERR_REPLINFO, 2, LEN_AND_STR(tmpmsg));
	repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
	if (is_jnlpool_creator)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Created jnlpool with shmid = [%d] and semid = [%d]\n",
			jnlpool->repl_inst_filehdr->jnlpool_shmid, jnlpool->repl_inst_filehdr->jnlpool_semid);
	} else
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Attached to existing jnlpool with shmid = [%d] and semid = [%d]\n",
			jnlpool->repl_inst_filehdr->jnlpool_shmid, jnlpool->repl_inst_filehdr->jnlpool_semid);
#	ifdef GTM_TLS
	if (REPL_TLS_REQUESTED)
	{
		repl_do_tls_init(gtmsource_log_fp);
		assert(REPL_TLS_REQUESTED || PLAINTEXT_FALLBACK);
	}
#	endif
	if (jnlpool->jnlpool_ctl->freeze)
	{
		last_seen_freeze_flag = jnlpool->jnlpool_ctl->freeze;
		sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1, VARLSTCNT(3) ERR_REPLINSTFROZEN, 1,
				jnlpool->repl_inst_filehdr->inst_info.this_instname);
		repl_log(gtmsource_log_fp, TRUE, FALSE, print_msg);
		sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1, VARLSTCNT(3) ERR_REPLINSTFREEZECOMMENT, 1,
				jnlpool->jnlpool_ctl->freeze_comment);
		repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
	}
	add_safe_timer_handler(1, gtmsource_heartbeat_timer);
#	ifdef DEBUG
	if (is_jnlpool_creator)
	{
		if (TREF(ydb_test_fake_enospc) && CUSTOM_ERRORS_LOADED)
		{	/* Only the journal pool creator drives fake_enospc */
			srand(time(NULL));
			start_timer((TID)fake_enospc, ENOSPC_INIT_DURATION, fake_enospc, 0, NULL);
		}
	}
#	endif
	gtmsource_local->jnlfileonly = gtmsource_options.jnlfileonly;
	do
	{ 	/* If mode is passive, go to sleep. Wakeup every now and then and check to see if I have to become active. */
		gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_START;
		if ((gtmsource_local->mode == GTMSOURCE_MODE_PASSIVE) && (gtmsource_local->shutdown == NO_SHUTDOWN))
		{
			gtmsource_poll_actions(FALSE);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_MODE_CHANGE);
			continue;
		}
		if (GTMSOURCE_MODE_PASSIVE == gtmsource_local->mode)
		{	/* Shutdown initiated */
			assert(gtmsource_local->shutdown == SHUTDOWN);
			sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1, VARLSTCNT(4) ERR_REPLINFO, 2,
				    RTS_ERROR_LITERAL("GTM Replication Source Server Shutdown signalled"));
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
			break;
		}
		gtmsource_poll_actions(FALSE);
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
			continue;
		if (GTMSOURCE_MODE_ACTIVE_REQUESTED == gtmsource_local->mode)
			gtmsource_local->mode = GTMSOURCE_MODE_ACTIVE;
		SNPRINTF(tmpmsg, REPL_MSG_SIZE, "GTM Replication Source Server now in ACTIVE mode using port %d",
			gtmsource_local->secondary_port);
		sgtm_putmsg(print_msg, OUT_BUFF_SIZE - 1, VARLSTCNT(4) ERR_REPLINFO, 2, LEN_AND_STR(tmpmsg));
		repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
		DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs;)
		assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke "grab_lock" and "rel_lock" unconditionally */
		grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
		if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
		{
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Starting afresh due to ONLINE ROLLBACK\n");
			repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO - Current Jnlpool Seqno : %llu\n",
					jnlpool->jnlpool_ctl->jnl_seqno);
			continue;
		}
		GTMSOURCE_SET_READ_ADDR(gtmsource_local, jnlpool);
		gtmsource_local->read_state = gtmsource_local->jnlfileonly ? READ_FILE : READ_POOL;
		read_jnl_seqno = gtmsource_local->read_jnl_seqno;
		assert(read_jnl_seqno <= jnlpool->jnlpool_ctl->jnl_seqno);
		if (read_jnl_seqno < jnlpool->jnlpool_ctl->jnl_seqno)
		{
			gtmsource_local->read_state = READ_FILE;
			QWASSIGN(gtmsource_save_read_jnl_seqno, jnlpool->jnlpool_ctl->jnl_seqno);
			gtmsource_pool2file_transition = TRUE; /* so that we read the latest gener jnl files */
		}
		rel_lock(jnlpool->jnlpool_dummy_reg);
		if (SS_NORMAL != (status = gtmsource_alloc_tcombuff()))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error allocating initial tcom buffer space. Malloc error"), status);
		gtmsource_filter = NO_FILTER;
		if ('\0' != gtmsource_local->filter_cmd[0])
		{
			if (SS_NORMAL == (status = repl_filter_init(gtmsource_local->filter_cmd)))
				gtmsource_filter |= EXTERNAL_FILTER;
			else
				gtmsource_exit(ABNORMAL_SHUTDOWN);
		}
		gtmsource_process();
		/* gtmsource_process returns only when mode needs to be changed to PASSIVE */
		assert(gtmsource_state == GTMSOURCE_CHANGING_MODE);
		gtmsource_ctl_close();
		gtmsource_free_msgbuff();
		gtmsource_free_tcombuff();
		gtmsource_free_filter_buff();
		gtmsource_stop_heartbeat();
		if (FD_INVALID != gtmsource_sock_fd)
			repl_close(&gtmsource_sock_fd);
		STOP_EXTERNAL_FILTER_IF_NEEDED(gtmsource_filter, gtmsource_log_fp, "GTMSOURCE_CHANGING_MODE");
	} while (TRUE);
	gtmsource_end();
	return(SS_NORMAL);
}
