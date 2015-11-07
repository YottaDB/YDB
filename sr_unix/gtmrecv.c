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
#include "gtm_fcntl.h"
#include "gtm_inet.h"
#include "gtm_ipc.h"
#include <sys/wait.h>
#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "gtmrecv.h"
#include "repl_log.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "repl_errno.h"
#include "gtm_event_log.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "cli.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"
#include "tp.h"
#include "repl_filter.h"
#include "error.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "is_proc_alive.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "gtmmsg.h"
#include "sgtm_putmsg.h"
#include "gt_timer.h"
#include "ftok_sems.h"
#include "init_secshr_addrs.h"
#include "mutex.h"
#include "fork_init.h"
#include "heartbeat_timer.h"
#include "gtmio.h"

GBLDEF	boolean_t		gtmrecv_fetchreysnc;
GBLDEF	boolean_t		gtmrecv_logstats = FALSE;
GBLDEF	int			gtmrecv_filter = NO_FILTER;

GBLREF	void			(*call_on_signal)();
GBLREF	uint4			process_id;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			recvpool_shmid;
GBLREF	gtmrecv_options_t	gtmrecv_options;
GBLREF	int			gtmrecv_log_fd;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	boolean_t		is_rcvr_server;
GBLREF	int			gtmrecv_srv_count;
GBLREF	uint4			log_interval;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	IN_PARMS		*cli_lex_in_ptr;
GBLREF	uint4			mutex_per_process_init_pid;

error_def(ERR_INITORRESUME);
error_def(ERR_MUPCLIERR);
error_def(ERR_NORESYNCSUPPLONLY);
error_def(ERR_NORESYNCUPDATERONLY);
error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPLERR);
error_def(ERR_REPLINFO);
error_def(ERR_REPLINSTFMT);
error_def(ERR_REPLINSTOPEN);
error_def(ERR_RESUMESTRMNUM);
error_def(ERR_REUSEINSTNAME);
error_def(ERR_REUSESLOTZERO);
error_def(ERR_TEXT);
error_def(ERR_UPDSYNC2MTINS);
error_def(ERR_UPDSYNCINSTFILE);

int gtmrecv(void)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	repl_inst_hdr		updresync_inst_hdr;
	uint4			gtmrecv_pid;
	int			idx, semval, status, save_upd_status, upd_start_status, upd_start_attempts;
	char			print_msg[1024], tmpmsg[1024];
	pid_t			pid, procgp;
	int			exit_status, waitpid_res, save_errno;
	int			log_init_status;
	int			updresync_instfile_fd;	/* fd of the instance file name specified in -UPDATERESYNC= */
	boolean_t		cross_endian;
	int			null_fd, rc;

	call_on_signal = gtmrecv_sigstop;
	ESTABLISH_RET(gtmrecv_ch, SS_NORMAL);
	memset((uchar_ptr_t)&recvpool, 0, SIZEOF(recvpool));
	if (-1 == gtmrecv_get_opt())
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
	if (gtmrecv_options.start || gtmrecv_options.shut_down)
	{
		jnlpool_init(GTMRECEIVE, (boolean_t)FALSE, (boolean_t *)NULL);
		assert(NULL != jnlpool.repl_inst_filehdr);
		assert(NULL != jnlpool.jnlpool_ctl);
		/* If -UPDATERESYNC was specified without an empty instance file, error out. The only exception is if
		 * the receiver is a root primary (updates enabled) supplementary instance and the source is non-supplementary.
		 * In this case, since the non-supplementary stream is being ADDED to an existing instance file, there is no
		 * requirement that the current instance file be empty. In fact, in that case we expect it to be non-empty
		 * as one history record would have been written by the source server that brought up the root primary instance.
		 */
		if (gtmrecv_options.updateresync && jnlpool.repl_inst_filehdr->num_histinfo
				&& !(jnlpool.repl_inst_filehdr->is_supplementary && !jnlpool.jnlpool_ctl->upd_disabled))
			/* replication instance file is NOT empty. Issue error */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UPDSYNC2MTINS);
		if (gtmrecv_options.noresync)
		{	/* If -NORESYNC was specified on a non-supplementary receiver instance, issue an error */
			if (!jnlpool.repl_inst_filehdr->is_supplementary)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NORESYNCSUPPLONLY);
			/* If -NORESYNC was specified on a receiver instance where updates are disabled, issue an error */
			if (jnlpool.jnlpool_ctl->upd_disabled)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NORESYNCUPDATERONLY);
		}
	}
	if (gtmrecv_options.shut_down)
	{	/* Wait till shutdown time nears even before going to "recvpool_init". This is because the latter will return
		 * with the ftok semaphore, access and options semaphore held and we do not want to be holding those locks (while
		 * waiting for the user specified timeout to expire) as that will affect new GTM processes and/or other
		 * MUPIP REPLIC commands that need these locks for their function.
		 */
		if (0 < gtmrecv_options.shutdown_time)
		{
			repl_log(stdout, FALSE, TRUE, "Waiting for %d seconds before signalling shutdown\n",
				gtmrecv_options.shutdown_time);
			LONG_SLEEP(gtmrecv_options.shutdown_time);
		} else
			repl_log(stdout, FALSE, TRUE, "Signalling immediate shutdown\n");
	}
	recvpool_init(GTMRECV, gtmrecv_options.start && 0 != gtmrecv_options.listen_port);
	/*
	 * When gtmrecv_options.start is TRUE, shm field recvpool.recvpool_ctl->fresh_start is updated in "recvpool_init"
	 *	recvpool.recvpool_ctl->fresh_start == TRUE ==> fresh start, and
	 *	recvpool.recvpool_ctl->fresh_start == FALSE ==> start after a crash
	 */
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	upd_helper_ctl = recvpool.upd_helper_ctl;
	if (gtmrecv_options.start)
	{
		if (0 == gtmrecv_options.listen_port /* implies (updateonly || helpers only) */
			|| !recvpool_ctl->fresh_start)
		{
			if (SRV_ALIVE == (status = is_recv_srv_alive()) && 0 != gtmrecv_options.listen_port)
			{
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Receiver Server already exists"));
			} else if (SRV_DEAD == status && 0 == gtmrecv_options.listen_port)
			{
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Receiver server does not exist. Start it first"));
			} else if (SRV_ERR == status)
			{
				status = errno;
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Receiver server semaphore error"), status);
			}
			if (gtmrecv_options.updateonly)
			{
				status = gtmrecv_start_updonly() - UPDPROC_STARTED;
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				gtmrecv_exit(status);
			}
			if (gtmrecv_options.helpers && 0 == gtmrecv_options.listen_port)
			{ /* start helpers only */
				status = gtmrecv_start_helpers(gtmrecv_options.n_readers, gtmrecv_options.n_writers);
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				gtmrecv_exit(status - NORMAL_SHUTDOWN);
			}
		}
		if (gtmrecv_options.updateresync && ('\0' != gtmrecv_options.updresync_instfilename[0]))
		{	/* -UPDATERESYNC=<INSTANCE_FILENAME> was specified.
			 * Note: -UPDATERESYNC without a value is treated as -UPDATERESYNC="" hence the above check.
			 */
			OPENFILE(gtmrecv_options.updresync_instfilename, O_RDONLY, updresync_instfile_fd);
			if (FD_INVALID == updresync_instfile_fd)
			{
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLINSTOPEN, 2,
					LEN_AND_STR(gtmrecv_options.updresync_instfilename), errno);
			}
			LSEEKREAD(updresync_instfile_fd, 0, &updresync_inst_hdr, SIZEOF(updresync_inst_hdr), status);
			if (0 != status)
			{	/* Encountered an error reading the full file header */
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Input file does not even have a full instance file header"));
			}
			/* Check if it is the right version */
			if (memcmp(updresync_inst_hdr.label, GDS_REPL_INST_LABEL, GDS_REPL_INST_LABEL_SZ - 1))
			{
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_UPDSYNCINSTFILE, 0,
					ERR_REPLINSTFMT, 6, LEN_AND_STR(gtmrecv_options.updresync_instfilename),
					GDS_REPL_INST_LABEL_SZ - 1, GDS_REPL_INST_LABEL,
					GDS_REPL_INST_LABEL_SZ - 1, updresync_inst_hdr.label);
			}
			/* Check endianness match. If not, fields have to be endian converted before examining them. */
			cross_endian = (GTM_IS_LITTLE_ENDIAN != updresync_inst_hdr.is_little_endian);
			/* At the time of this writing, the only minor version supported is 1.
			 * Whenever this gets updated, we need to add code to do the online upgrade.
			 * Add an assert as a reminder to do this.
			 */
			assert(1 == SIZEOF(updresync_inst_hdr.replinst_minorver)); /* so no endian conversion needed */
			assert(1 == updresync_inst_hdr.replinst_minorver);
			/* Check if cleanly shutdown */
			if (cross_endian)
			{
				assert(4 == SIZEOF(updresync_inst_hdr.crash)); /* so need to use GTM_BYTESWAP_32 */
				updresync_inst_hdr.crash = GTM_BYTESWAP_32(updresync_inst_hdr.crash);
			}
			if (updresync_inst_hdr.crash)
			{	/* The instance file cannot be used for updateresync if it was not cleanly shutdown */
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Input instance file was not cleanly shutdown"));
			}
			if (cross_endian)
			{
				assert(4 == SIZEOF(updresync_inst_hdr.is_supplementary)); /* so need GTM_BYTESWAP_32 */
				updresync_inst_hdr.is_supplementary = GTM_BYTESWAP_32(updresync_inst_hdr.is_supplementary);
				assert(8 == SIZEOF(updresync_inst_hdr.jnl_seqno)); /* so need to use GTM_BYTESWAP_64 */
				updresync_inst_hdr.jnl_seqno = GTM_BYTESWAP_64(updresync_inst_hdr.jnl_seqno);
				assert(4 == SIZEOF(updresync_inst_hdr.num_histinfo)); /* so need to use GTM_BYTESWAP_32 */
				updresync_inst_hdr.num_histinfo = GTM_BYTESWAP_32(updresync_inst_hdr.num_histinfo);
			}
			if (jnlpool.repl_inst_filehdr->is_supplementary && !updresync_inst_hdr.is_supplementary)
			{	/* Do one check for non-supplementary -> supplementary connection using -updateresync.
				 * This is because this use of -updateresync is different than the other connection usages.
				 */
				if (!updresync_inst_hdr.jnl_seqno)
				{	/* The instance file cannot be used for updateresync if it has a ZERO seqno */
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
						  RTS_ERROR_LITERAL("Non-supplementary input instance file cannot be used"
						  " on supplementary instance when it is empty (has seqno of 0)"));
				}
			}
			/* else similar checks of the jnl_seqno and num_histinfo for other types of connections (i.e.
			 * non-supplementary -> non-supplementary, supplementary -> supplementary) will be done later in
			 * "repl_inst_get_updresync_histinfo" only when there is a need to scan the input instance file
			 * for any history record.
			 */
			updresync_inst_hdr.num_histinfo--;	/* needed to get at the last history record */
			if (cross_endian)
				ENDIAN_CONVERT_REPL_INST_UUID(&updresync_inst_hdr.lms_group_info);
			if (IS_REPL_INST_UUID_NULL(updresync_inst_hdr.lms_group_info))
			{	/* The input instance has a NULL LMS group. Cannot be used to fill in current instance */
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Input instance file has NULL LMS group"));
			}
			if (updresync_inst_hdr.is_supplementary)
			{	/* The input instance file is supplementary. Allow it only if the current instance is
				 * supplementary and is not a root primary.
				 */
				if (!jnlpool.repl_inst_filehdr->is_supplementary)
				{
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
						  LEN_AND_LIT("Input instance file must be non-supplementary"
							  	" to match current instance"));
					assert(FALSE);
				}
				if (!jnlpool.jnlpool_ctl->upd_disabled)
				{
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
						  LEN_AND_LIT("Input instance file must be non-supplementary"
							  	" as current instance is a supplementary root primary"));
				}
			} else if (jnlpool.repl_inst_filehdr->is_supplementary)
			{
				if (jnlpool.jnlpool_ctl->upd_disabled)
				{	/* The input instance file is non-supplementary. Allow it only if the current
					 * instance is non-supplementary or if it is a supplementary root primary.
					 */
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
						  LEN_AND_LIT("Input instance file must be supplementary"
								" to match current instance"));
				}
				if (!gtmrecv_options.resume_specified && !gtmrecv_options.initialize_specified)
				{
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INITORRESUME);
				}
			}
			if (!jnlpool.repl_inst_filehdr->is_supplementary || jnlpool.jnlpool_ctl->upd_disabled)
			{
				if (gtmrecv_options.resume_specified)
				{
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RESUMESTRMNUM, 0, ERR_TEXT, 2,
						LEN_AND_LIT("RESUME allowed only on root primary supplementary instance"));
				}
				if (gtmrecv_options.reuse_specified)
				{
					rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REUSEINSTNAME, 0, ERR_TEXT, 2,
						LEN_AND_LIT("REUSE allowed only on root primary supplementary instance"));
				}
			}
		}
#		ifndef REPL_DEBUG_NOBACKGROUND
		FORK_CLEAN(pid);
		if (0 < pid)
		{
			REPL_DPRINT2("Waiting for receiver child process %d to startup\n", pid);
			while (0 == (semval = get_sem_info(RECV, RECV_SERV_COUNT_SEM, SEM_INFO_VAL)) &&
			       is_proc_alive(pid, 0))
			{
				/* To take care of reassignment of PIDs, the while condition should be && with the
				 * condition (PPID of pid == process_id)
				 */
				REPL_DPRINT2("Waiting for receiver child process %d to startup\n", pid);
				SHORT_SLEEP(GTMRECV_WAIT_FOR_SRV_START);
				WAITPID(pid, &exit_status, WNOHANG, waitpid_res); /* Release defunct child if dead */
			}
			if (0 <= semval)
				rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
			gtmrecv_exit(1 == semval ? SRV_ALIVE : SRV_DEAD);
		} else if (0 > pid)
		{
			status = errno;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				  RTS_ERROR_LITERAL("Unable to fork"), status);
		}
#		endif
	} else if (gtmrecv_options.shut_down)
	{
		if (gtmrecv_options.updateonly)
			gtmrecv_exit(gtmrecv_endupd() - NORMAL_SHUTDOWN);
		if (gtmrecv_options.helpers)
			gtmrecv_exit(gtmrecv_end_helpers(FALSE) - NORMAL_SHUTDOWN);
		gtmrecv_exit(gtmrecv_shutdown(FALSE, NORMAL_SHUTDOWN) - NORMAL_SHUTDOWN);
	} else if (gtmrecv_options.changelog)
		gtmrecv_exit(gtmrecv_changelog() - NORMAL_SHUTDOWN);
	else if (gtmrecv_options.checkhealth)
		gtmrecv_exit(gtmrecv_checkhealth() - NORMAL_SHUTDOWN);
	else if (gtmrecv_options.showbacklog)
		gtmrecv_exit(gtmrecv_showbacklog() - NORMAL_SHUTDOWN);
	else
		gtmrecv_exit(gtmrecv_statslog() - NORMAL_SHUTDOWN);
	/* Point stdin to /dev/null */
	OPENFILE("/dev/null", O_RDONLY, null_fd);
	if (0 > null_fd)
		rts_error_csa(CSA_ARG(NULL) ERR_REPLERR, RTS_ERROR_LITERAL("Failed to open /dev/null for read"), errno, 0);
	FCNTL3(null_fd, F_DUPFD, 0, rc);
	if (0 > rc)
		rts_error_csa(CSA_ARG(NULL) ERR_REPLERR, RTS_ERROR_LITERAL("Failed to set stdin to /dev/null"), errno, 0);
	CLOSEFILE(null_fd, rc);
	if (0 > rc)
		rts_error_csa(CSA_ARG(NULL) ERR_REPLERR, RTS_ERROR_LITERAL("Failed to close /dev/null"), errno, 0);
	assert(!holds_sem[RECV][RECV_POOL_ACCESS_SEM]);
	assert(holds_sem[RECV][RECV_SERV_OPTIONS_SEM]);
	is_rcvr_server = TRUE;
	process_id = getpid();
	OPERATOR_LOG_MSG;
	/* Reinvoke secshr related initialization with the child's pid */
	INVOKE_INIT_SECSHR_ADDRS;
	/* Initialize mutex socket, memory semaphore etc. before any "grab_lock" is done by this process on the journal pool.
	 * Note that the initialization would already have been done by the parent receiver startup command but we need to
	 * redo the initialization with the child process id.
	 */
	assert(mutex_per_process_init_pid && mutex_per_process_init_pid != process_id);
	mutex_per_process_init();
	START_HEARTBEAT_IF_NEEDED;
	/* Take a copy of the fact whether an -UPDATERESYNC was specified or not. Note this down in shared memory.
	 * We will clear the shared memory copy as soon as the first history record gets written to the instance file
	 * after the first time this receiver connects to a source. Future connects of this same receiver with the same
	 * or different source servers should NOT use the -UPDATERESYNC but instead treat it as a non-updateresync connect.
	 */
	gtmrecv_local->updateresync = gtmrecv_options.updateresync;
	/* Now that this receiver server has started up fine without issues, one can safely move the instance file descriptor
	 * (and other instance file header information) from private memory to gtmrecv_local (shared memory).
	 */
	if (gtmrecv_options.updateresync && ('\0' != gtmrecv_options.updresync_instfilename[0]))
	{
		gtmrecv_local->updresync_instfile_fd = updresync_instfile_fd;
		assert(cross_endian == (GTM_IS_LITTLE_ENDIAN != updresync_inst_hdr.is_little_endian));
		gtmrecv_local->updresync_cross_endian = cross_endian;
		gtmrecv_local->updresync_num_histinfo = updresync_inst_hdr.num_histinfo;	/* already endian converted */
		/* In case of a supplementary input instance file, we also want the last history record number
		 * for each available non-supplementary stream. Endian convert it if needed.
		 */
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
		{
			assert(4 == SIZEOF(updresync_inst_hdr.last_histinfo_num[idx])); /* so need to use GTM_BYTESWAP_32 */
			gtmrecv_local->updresync_num_histinfo_strm[idx]
				= cross_endian ? GTM_BYTESWAP_32(updresync_inst_hdr.last_histinfo_num[idx])
					       : updresync_inst_hdr.last_histinfo_num[idx];
			assert(gtmrecv_local->updresync_num_histinfo >= gtmrecv_local->updresync_num_histinfo_strm[idx]);
			assert((0 <= gtmrecv_local->updresync_num_histinfo_strm[idx])
				|| (INVALID_HISTINFO_NUM == gtmrecv_local->updresync_num_histinfo_strm[idx]));
		}
		gtmrecv_local->updresync_lms_group = updresync_inst_hdr.lms_group_info;
		gtmrecv_local->updresync_jnl_seqno = updresync_inst_hdr.jnl_seqno;
	} else
	{
		gtmrecv_local->updresync_instfile_fd = FD_INVALID;
		/* No need to initialize the below since they will be used only if updresync_instfile_fd is not FD_INVALID
		 *	gtmrecv_local->updresync_cross_endian = cross_endian;
		 *	gtmrecv_local->updresync_num_histinfo = updresync_inst_hdr.num_histinfo;
		 *	gtmrecv_local->updresync_num_histinfo_strm[] = updresync_inst_hdr.last_histinfo_num[];
		 *	gtmrecv_local->updresync_lms_group = updresync_inst_hdr.lms_group_info;
		 *	gtmrecv_local->updresync_jnl_seqno = updresync_inst_hdr.jnl_seqno;
		 */
	}
	/* Take a copy of the fact whether a -NORESYNC was specified or not. Note this down in shared memory.
	 * We will clear the shared memory copy as soon as the first history record gets written to the instance file
	 * after the first time this receiver connects to a source. Future connects of this same receiver with the same
	 * or different source servers should NOT use the -NORESYNC but instead treat it as a regular (no -noresync) connect.
	 */
	gtmrecv_local->noresync = gtmrecv_options.noresync;
	STRCPY(gtmrecv_local->log_file, gtmrecv_options.log_file);
	gtmrecv_local->log_interval = log_interval = gtmrecv_options.rcvr_log_interval;
	upd_proc_local->log_interval = gtmrecv_options.upd_log_interval;
	upd_helper_ctl->start_helpers = FALSE;
	upd_helper_ctl->start_n_readers = upd_helper_ctl->start_n_writers = 0;
	log_init_status = repl_log_init(REPL_GENERAL_LOG, &gtmrecv_log_fd, gtmrecv_options.log_file);
	assert(SS_NORMAL == log_init_status);
	repl_log_fd2fp(&gtmrecv_log_fp, gtmrecv_log_fd);
	if (-1 == (procgp = setsid()))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Receiver server error in setsid"), errno);
	gtm_event_log_init();
	gtmrecv_local->recv_serv_pid = process_id;
	assert(NULL != jnlpool.jnlpool_ctl);
	jnlpool.jnlpool_ctl->gtmrecv_pid = process_id;
	gtmrecv_local->listen_port = gtmrecv_options.listen_port;
	/* Log receiver server startup command line first */
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "%s %s\n", cli_lex_in_ptr->argv[0], cli_lex_in_ptr->in_str);

	assert(NULL != jnlpool.repl_inst_filehdr);
	SPRINTF(tmpmsg, "GTM Replication Receiver Server with Pid [%d] started on replication instance [%s]",
		process_id, jnlpool.repl_inst_filehdr->inst_info.this_instname);
	sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2, LEN_AND_STR(tmpmsg));
	repl_log(gtmrecv_log_fp, TRUE, TRUE, print_msg);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Attached to existing jnlpool with shmid = [%d] and semid = [%d]\n",
			jnlpool.repl_inst_filehdr->jnlpool_shmid, jnlpool.repl_inst_filehdr->jnlpool_semid);
	gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLINFO", print_msg);
	if (recvpool_ctl->fresh_start)
	{
		QWASSIGNDW(recvpool_ctl->jnl_seqno, 0); /* Update process will initialize this to a non-zero value */
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Created recvpool with shmid = [%d] and semid = [%d]\n",
				jnlpool.repl_inst_filehdr->recvpool_shmid, jnlpool.repl_inst_filehdr->recvpool_semid);
	} else
	{	/* Coming up after a crash, reset Update process read.  This is done by setting gtmrecv_local->restart.
		 * This will trigger update process to reset recvpool_ctl->jnl_seqno too.
		 */
		gtmrecv_local->restart = GTMRECV_RCVR_RESTARTED;
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Attached to existing recvpool with shmid = [%d] and semid = [%d]\n",
				jnlpool.repl_inst_filehdr->recvpool_shmid, jnlpool.repl_inst_filehdr->recvpool_semid);
	}
	save_upd_status = upd_proc_local->upd_proc_shutdown;
	for (upd_start_attempts = 0;
	     UPDPROC_START_ERR == (upd_start_status = gtmrecv_upd_proc_init(recvpool_ctl->fresh_start)) &&
	     GTMRECV_MAX_UPDSTART_ATTEMPTS > upd_start_attempts;
	     upd_start_attempts++)
	{
		if (EREPL_UPDSTART_SEMCTL == repl_errno || EREPL_UPDSTART_BADPATH == repl_errno)
		{
			gtmrecv_exit(ABNORMAL_SHUTDOWN);
		} else if (EREPL_UPDSTART_FORK == repl_errno)
		{
			/* Couldn't start up update now, can try later */
			LONG_SLEEP(GTMRECV_WAIT_FOR_PROC_SLOTS);
			continue;
		} else if (EREPL_UPDSTART_EXEC == repl_errno)
		{
			/* In forked child, could not exec, should exit */
			upd_proc_local->upd_proc_shutdown = save_upd_status;
			gtmrecv_exit(ABNORMAL_SHUTDOWN);
		}
	}
	if ((UPDPROC_EXISTS == upd_start_status && recvpool_ctl->fresh_start) ||
	    (UPDPROC_START_ERR == upd_start_status && GTMRECV_MAX_UPDSTART_ATTEMPTS <= upd_start_attempts))
	{
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLERR, RTS_ERROR_LITERAL((UPDPROC_EXISTS == upd_start_status) ?
			    "Runaway Update Process. Aborting..." :
			    "Too many failed attempts to fork Update Process. Aborting..."));
		repl_log(gtmrecv_log_fp, TRUE, TRUE, print_msg);
		gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLERR", print_msg);
		gtmrecv_exit(ABNORMAL_SHUTDOWN);
	}
	upd_proc_local->start_upd = UPDPROC_STARTED;
	if (!recvpool_ctl->fresh_start)
	{
		while ((GTMRECV_RCVR_RESTARTED == gtmrecv_local->restart) && (SRV_ALIVE == is_updproc_alive()))
		{
			REPL_DPRINT1("Rcvr waiting for update to restart\n");
			SHORT_SLEEP(GTMRECV_WAIT_FOR_SRV_START);
		}
		upd_proc_local->bad_trans = FALSE;
		recvpool_ctl->write_wrap = recvpool_ctl->recvpool_size;
		recvpool_ctl->write = 0;
		recvpool_ctl->wrapped = FALSE;
		upd_proc_local->changelog = TRUE;
		gtmrecv_local->restart = GTMRECV_NO_RESTART; /* release the update process wait */
	}
	if (gtmrecv_options.helpers)
		gtmrecv_helpers_init(gtmrecv_options.n_readers, gtmrecv_options.n_writers);
	/* It is necessary for every process that is using the ftok semaphore to increment the counter by 1. This is used
	 * by the last process that shuts down to delete the ftok semaphore when it notices the counter to be 0.
	 * Note that the parent receiver server startup command would have done an increment of the ftok counter semaphore
	 * for the replication instance file. But the receiver server process (the child) that comes here would not have done
	 * that. Do that while the parent is still waiting for our okay.
	 */
	if (!ftok_sem_incrcnt(recvpool.recvpool_dummy_reg))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RECVPOOLSETUP);
	/* Lock the receiver server count semaphore. Its value should be atmost 1. */
	if (0 > grab_sem_immediate(RECV, RECV_SERV_COUNT_SEM))
	{
		save_errno = errno;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Receive pool semop failure"), save_errno);
	}
#	ifdef REPL_DEBUG_NOBACKGROUND
	rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
#	endif
	gtmrecv_srv_count++;
	gtmrecv_filter = NO_FILTER;
	if ('\0' != gtmrecv_local->filter_cmd[0])
	{
		if (SS_NORMAL == (status = repl_filter_init(gtmrecv_local->filter_cmd)))
			gtmrecv_filter |= EXTERNAL_FILTER;
		else
		{
			if (EREPL_FILTERSTART_EXEC == repl_errno)
				gtmrecv_exit(ABNORMAL_SHUTDOWN);
		}
	}
	gtmrecv_process(!recvpool_ctl->fresh_start);
	return (SS_NORMAL);
}
