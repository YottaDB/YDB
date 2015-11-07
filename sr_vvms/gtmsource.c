/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <efndef.h>
#include "gtm_inet.h"
#include <sys/mman.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "gtm_string.h"

#include <ssdef.h>
#include <prtdef.h>
#include <prcdef.h>
#include <secdef.h>
#include <psldef.h>
#include <descrip.h>
#include <iodef.h>
#include <prvdef.h>
#include <lnmdef.h>

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
#include "cli.h"
#include "iosp.h"
#include "repl_log.h"
#include "repl_errno.h"
#include "gtm_event_log.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "efn.h"
#include "vmsdtype.h"
#include "gtm_logicals.h"
#include "repl_filter.h"
#include "util.h"
#include "io.h"
#include "is_proc_alive.h"
#include "is_file_identical.h"
#include "dfntmpmbx.h"
#include "gtmmsg.h"
#include "sgtm_putmsg.h"
#include "trans_log_name.h"
#include "repl_comm.h"

GBLDEF boolean_t	gtmsource_logstats = FALSE, gtmsource_pool2file_transition = FALSE;
GBLDEF int		gtmsource_filter = NO_FILTER;
GBLDEF boolean_t	update_disable = FALSE;

GBLREF gtmsource_options_t	gtmsource_options;
GBLREF gtmsource_state_t	gtmsource_state;
GBLREF jnlpool_addrs 	jnlpool;
GBLREF uint4		process_id;
GBLREF int		gtmsource_sock_fd;
GBLREF int		gtmsource_log_fd;
GBLREF FILE		*gtmsource_log_fp;
GBLREF gd_addr		*gd_header;
GBLREF void             (*call_on_signal)();
GBLREF boolean_t        is_src_server;
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
GBLREF boolean_t	primary_side_std_null_coll;
GBLREF boolean_t	primary_side_trigger_support;
GBLREF uint4		log_interval;
GBLREF unsigned char	jnl_ver;

error_def(ERR_JNLPOOLSETUP);
error_def(ERR_MUPCLIERR);
error_def(ERR_NOTALLDBOPN);
error_def(ERR_NULLCOLLDIFF);
error_def(ERR_REPLCOMM);
error_def(ERR_REPLINFO);
error_def(ERR_REPLOFFJNLON);
error_def(ERR_TEXT);

int gtmsource()
{
	gd_region	        *reg, *region_top;
	sgmnt_addrs		*csa;
	boolean_t	        jnlpool_inited, srv_alive, all_files_open;
	uint4		        gtmsource_pid;
	int		        status, ef_status, retval=0, len;
	char		        print_msg[1024];
	uint4		        prvadr[2], prvprv[2];
	mstr			log_nam, trans_log_nam;
	char			proc_name[PROC_NAME_MAXLEN + 1];
	char			cmd_str[MAX_COMMAND_LINE_LENGTH], sigmbx_name[MAX_NAME_LEN + 1], trans_buff[MAX_FN_LEN];
	uint4			signal_channel, cmd_channel;
	unsigned short		mbsb[4], cmd_len;
	uint4			buff, server_pid, clus_flags_stat;
	gds_file_id		file_id;
	seq_num			resync_seqno;
	char			exp_log_file_name[MAX_FN_LEN], log_file_name[MAX_FN_LEN];
	unsigned short		log_file_len;
	int			exp_log_file_name_len, connect_parms_index;
	char			*ptr, qwstring[100];

	$DESCRIPTOR(proc_name_desc, proc_name);
	$DESCRIPTOR(d_signal_mbox, sigmbx_name);
	$DESCRIPTOR(cmd_desc, cmd_str);

	memset((uchar_ptr_t)&jnlpool, 0, SIZEOF(jnlpool));
	call_on_signal = gtmsource_sigstop;
	ESTABLISH_RET(gtmsource_ch, SS_NORMAL);
	if (-1 == gtmsource_get_opt())
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);

	if (CLI_PRESENT == cli_present("START"))
	{	/* Generate a unique name for the signal_mbx using global_name(gbldir) and "GTMX" as prefix */
		log_nam.addr = GTM_GBLDIR;
		log_nam.len = STR_LIT_LEN(GTM_GBLDIR);
		if (SS_NORMAL != (status = trans_log_name(&log_nam, &trans_log_nam, trans_buff)))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("gtm$gbldir not defined"), status);
			gtmsource_exit(ABNORMAL_SHUTDOWN);
		}
		set_gdid_from_file((gd_id_ptr_t)&file_id, trans_buff, trans_log_nam.len);
		global_name("GTMX", &file_id, sigmbx_name);
		STR_OF_DSC(d_signal_mbox)++;
		LEN_OF_DSC(d_signal_mbox) = sigmbx_name[0];
		if (CLI_PRESENT != cli_present("DUMMY_START"))
		{	/* Get the cmd line */
			if (0 == ((status = lib$get_foreign(&cmd_desc, 0, &cmd_len, 0)) & 1))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Unable to read the cmd-line into a string"), status);
				gtmsource_exit(ABNORMAL_SHUTDOWN);
			}
			if (0 == cmd_len)
			{ /* Command issued on the MUPIP command line, we have to build the argument string to pass to child */
				MEMCPY_LIT(&cmd_str[cmd_len], SOURCE_PROMPT_START_QUAL);
				cmd_len += STR_LIT_LEN(SOURCE_PROMPT_START_QUAL);
				if (CLI_PRESENT == cli_present("BUFFSIZE"))
				{
					MEMCPY_LIT(&cmd_str[cmd_len], BUFF_QUAL);
					cmd_len += STR_LIT_LEN(BUFF_QUAL);
					ptr = i2asc(qwstring, gtmsource_options.buffsize);
					memcpy(&cmd_str[cmd_len], qwstring, ptr - qwstring);
					cmd_len += ptr - qwstring;
				}
				if (CLI_PRESENT == cli_present("CONNECTPARAMS"))
				{
					MEMCPY_LIT(&cmd_str[cmd_len], CONNECT_QUAL);
					cmd_len += STR_LIT_LEN(CONNECT_QUAL);
					cmd_str[cmd_len++] = '"'; /* begin quote */
					for (connect_parms_index = GTMSOURCE_CONN_HARD_TRIES_COUNT;
					     connect_parms_index < GTMSOURCE_CONN_PARMS_COUNT; connect_parms_index++)
					{
						ptr = i2asc(qwstring, gtmsource_options.connect_parms[connect_parms_index]);
						memcpy(&cmd_str[cmd_len], qwstring, ptr - qwstring);
						cmd_len += ptr - qwstring;
						cmd_str[cmd_len++] = ','; /* delimiter */
					}
					cmd_str[cmd_len - 1] = '"'; /* end quote, overwrite last comma */
				}
				if (CLI_PRESENT == cli_present("FILTER"))
				{
					MEMCPY_LIT(&cmd_str[cmd_len], FILTER_QUAL);
					cmd_len += STR_LIT_LEN(FILTER_QUAL);
					len = strlen(gtmsource_options.filter_cmd);
					memcpy(&cmd_str[cmd_len], gtmsource_options.filter_cmd, len);
					cmd_len += len;
				}
				if (CLI_PRESENT == cli_present("LOG"))
				{
					MEMCPY_LIT(&cmd_str[cmd_len], LOG_QUAL);
					cmd_len += STR_LIT_LEN(LOG_QUAL);
					len = strlen(gtmsource_options.log_file);
					memcpy(&cmd_str[cmd_len], gtmsource_options.log_file, len);
					cmd_len += len;
				}
				if (CLI_PRESENT == cli_present("LOG_INTERVAL"))
				{
					MEMCPY_LIT(&cmd_str[cmd_len], LOGINTERVAL_QUAL);
					cmd_len += STR_LIT_LEN(LOGINTERVAL_QUAL);
					ptr = i2asc(qwstring, gtmsource_options.src_log_interval);
					memcpy(&cmd_str[cmd_len], qwstring, ptr - qwstring);
					cmd_len += ptr - qwstring;
				}
				if (CLI_PRESENT == cli_present("PASSIVE"))
				{
					MEMCPY_LIT(&cmd_str[cmd_len], PASSIVE_QUAL);
					cmd_len += STR_LIT_LEN(PASSIVE_QUAL);
				}
				if (CLI_PRESENT == cli_present("SECONDARY"))
				{
					MEMCPY_LIT(&cmd_str[cmd_len], SECONDARY_QUAL);
					cmd_len += STR_LIT_LEN(SECONDARY_QUAL);
					len = strlen(gtmsource_options.secondary_host);
					memcpy(&cmd_str[cmd_len], gtmsource_options.secondary_host, len);
					cmd_len += len;
					MEMCPY_LIT(&cmd_str[cmd_len], ":");
					cmd_len += 1;
					ptr = i2asc(qwstring, gtmsource_options.secondary_port);
					memcpy(&cmd_str[cmd_len], qwstring, ptr - qwstring);
					cmd_len += ptr - qwstring;
				}
			}
			/* Append a dummy qualifier */
			MEMCPY_LIT(&cmd_str[cmd_len], DUMMY_START_QUAL);
			cmd_desc.dsc$w_length = cmd_len + STR_LIT_LEN(DUMMY_START_QUAL);
			/* Create a mailbox to wait for the source server, tobe spawned off, to get initialized */
			status = dfntmpmbx(LEN_AND_LIT("LNM$GROUP"));
			if (SS$_NORMAL != status && SS$_SUPERSEDE != status)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Unable to redefine LNM$TEMPORARY_MAILBOX"), status);
				gtmsource_exit(ABNORMAL_SHUTDOWN);
			}
			SET_PRIV((PRV$M_GRPNAM), status);
			if (SS$_NORMAL != status)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Unable to get GRPNAM privilege"), status);
				gtmsource_exit(ABNORMAL_SHUTDOWN);
			}
			status = sys$crembx(0, &signal_channel, 4, 4, 0, PSL$C_USER, &d_signal_mbox);
			REL_PRIV;
			if (SS$_NORMAL != status)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Unable to create source server init-wait mailbox"), status);
				gtmsource_exit(ABNORMAL_SHUTDOWN);
			}
			status = dfntmpmbx(LEN_AND_LIT("LNM$JOB"));
			if (SS$_NORMAL != status && SS$_SUPERSEDE != status)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Unable to redefine LNM$TEMPORARY_MAILBOX"), status);
				gtmsource_exit(ABNORMAL_SHUTDOWN);
			}
			/* Create detached server and write startup commands to it */
			if (SS$_NORMAL != repl_create_server(&cmd_desc, "GTMS", "", &cmd_channel, &server_pid, ERR_JNLPOOLSETUP))
				gtmsource_exit(ABNORMAL_SHUTDOWN);
			/* Async. read on the mailbox, just borrowing the event flag efn_op_job */
			status = sys$qio(efn_op_job, signal_channel, IO$_READVBLK, &mbsb[0], 0, 0,
					&buff, SIZEOF(buff), 0, 0, 0, 0);
			if (SS$_NORMAL != status)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
							ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to read from signal-mbx"), status);
				retval = ABNORMAL_SHUTDOWN;
			}
			/* wait until (server process is alive AND qio-event-flag not set) */
			while (!retval && (srv_alive = is_proc_alive(server_pid, 0)) &&
			      (SS$_WASCLR == (ef_status = sys$readef(efn_op_job, &clus_flags_stat))))
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_SRV_START);
			if (!retval && !srv_alive)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Source server died while startup"));
				retval = ABNORMAL_SHUTDOWN;
			}
			if (!retval && (SS$_WASSET != ef_status))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Source server startup completion signalling problem"), ef_status);
				retval = ABNORMAL_SHUTDOWN;
			}
			/* Verify the content read from mailbox */
			if (!retval && (SERVER_UP != buff))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Invalid message read from signalling mbox"));
				retval = ABNORMAL_SHUTDOWN;
			}
			/* Deassign the send-cmd mailbox channel */
			if (SS$_NORMAL != (status = sys$dassgn(cmd_channel)))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Unable to close send-cmd mbox channel"), status);
				retval = ABNORMAL_SHUTDOWN;
			}
			/* Deassign the signalling mailbox channel */
			if (SS$_NORMAL != (status = sys$dassgn(signal_channel)))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Unable to close source server startup mbox channel"), status);
				retval = ABNORMAL_SHUTDOWN;
			}
			gtmsource_exit(retval);
		}
		process_id = getpid(); /* pid of the source server */
		/* Get a meaningful process name */
		proc_name_desc.dsc$w_length = get_proc_name(LIT_AND_LEN("GTMSRC"), process_id, proc_name);
		if (SS$_NORMAL != (status = sys$setprn(&proc_name_desc)))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Unable to change source server process name"), status);
			gtmsource_exit(ABNORMAL_SHUTDOWN);
		}
	}
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
		else if (gtmsource_options.update)
			gtmsource_exit(gtmsource_secnd_update(FALSE) - NORMAL_SHUTDOWN);
		else
			gtmsource_exit(gtmsource_statslog() - NORMAL_SHUTDOWN);
	}
	assert(gtmsource_options.start);
	is_src_server = TRUE;
	strcpy(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
	jnlpool.gtmsource_local->log_interval = log_interval = gtmsource_options.src_log_interval;
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
	util_log_open(STR_AND_LEN(gtmsource_options.log_file));
	/* If previous shutdown did not complete successfully and jnlpool was left lying around, do not proceed */
	if (!jnlpool_inited && NO_SHUTDOWN != jnlpool.gtmsource_local->shutdown)
	{
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2,
			RTS_ERROR_LITERAL("Previous source server did not complete shutdown. Resetting shutdown related fields\n"));
		repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
		jnlpool.gtmsource_local->shutdown = NO_SHUTDOWN;
	}
	REPL_DPRINT1("Setting up regions\n");
	gvinit();

	/* We use the same code dse uses to open all regions but we must make sure
	 * they are all open before proceeding.
	 */
	all_files_open = region_init(FALSE);
	if (!all_files_open)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTALLDBOPN);
		gtmsource_autoshutdown();
	}
	/* Determine primary side null subscripts collation order */
	/* Also check whether all regions have same null collation order */
	primary_side_std_null_coll = -1;
	for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
	{
		csa = &FILE_INFO(reg)->s_addrs;
		if (primary_side_std_null_coll != csa->hdr->std_null_coll)
		{
			if (-1 == primary_side_std_null_coll)
				primary_side_std_null_coll = csa->hdr->std_null_coll;
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NULLCOLLDIFF);
		}
	}
	primary_side_trigger_support = FALSE;
	EXIT_IF_REPLOFF_JNLON(gd_header);
	if (jnlpool_inited)
		gtmsource_seqno_init();
	jnlpool.gtmsource_local->gtmsource_pid = process_id;
	rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
	rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
	/* Indicate the start up completion to the parent mupip process through a mail box */
	status = dfntmpmbx(LEN_AND_LIT("LNM$GROUP"));
	if (SS$_NORMAL != status && SS$_SUPERSEDE != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to redefine LNM$TEMPORARY_MAILBOX"), status);
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	SET_PRIV((PRV$M_GRPNAM), status);
	status = sys$crembx(0, &signal_channel, 4, 4, 0, PSL$C_USER, &d_signal_mbox);
	REL_PRIV;
	if (SS$_NORMAL != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to create source server init-wait mailbox"), status);
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	status = dfntmpmbx(LEN_AND_LIT("LNM$JOB"));
	if (SS$_NORMAL != status && SS$_SUPERSEDE != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to redefine LNM$TEMPORARY_MAILBOX"), status);
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	buff = SERVER_UP;
	status = sys$qiow(EFN$C_ENF, signal_channel, IO$_WRITEVBLK | IO$M_NOW, &mbsb[0], 0, 0, &buff, SIZEOF(buff), 0, 0, 0, 0);
	if (SS$_NORMAL != status)
	{
		if (SS$_NORMAL != (status = sys$dassgn(signal_channel)))
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to close source server startup mbox channel (child)"), status);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to write start-up completion in the source server startup mbox (child)"),
				status);
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	if (SS$_NORMAL != (status = sys$dassgn(signal_channel)))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to close source server startup mbox channel (child)"), status);
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	gtmsource_srv_count++;
	gtmsource_secnd_update(TRUE);
	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		if (reg->read_only && REPL_ALLOWED(&FILE_INFO(reg)->s_addrs))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				   RTS_ERROR_LITERAL("Source Server does not have write permissions to one or "
					   	     "more database files that are replicated"));
			gtmsource_autoshutdown();
		}
	}
	gtm_event_log_init();
	if (jnlpool_inited)
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2,
			    RTS_ERROR_LITERAL("GTM Replication Source Server : Fresh Start"));
	else
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2,
			    RTS_ERROR_LITERAL("GTM Replication Source Server : Crash Restart"));
	repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
	gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLINFO", print_msg);
	jnl_ver = JNL_VER_THIS;
	do
	{	/* If mode is passive, go to sleep. Wakeup every now and
		 * then and check to see if I have to become active */
		gtmsource_state = GTMSOURCE_START;
		while (jnlpool.gtmsource_local->mode == GTMSOURCE_MODE_PASSIVE
			&& jnlpool.gtmsource_local->shutdown == NO_SHUTDOWN)
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_MODE_CHANGE)
			;
		if (GTMSOURCE_MODE_PASSIVE == jnlpool.gtmsource_local->mode)
		{	/* Shutdown initiated */
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
		if (GTMSOURCE_MODE_ACTIVE_REQUESTED == jnlpool.gtmsource_local->mode)
			jnlpool.gtmsource_local->mode = GTMSOURCE_MODE_ACTIVE;
		sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLINFO, 2,
			    RTS_ERROR_LITERAL("GTM Replication Source Server now in ACTIVE mode"));
		repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
		gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLINFO", print_msg);
		if (SS_NORMAL != (status = gtmsource_alloc_tcombuff()))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
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
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		QWASSIGN(resync_seqno, seq_num_zero);
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
		{
			assert(reg->open);
			csa = &FILE_INFO(reg)->s_addrs;
			if (REPL_ALLOWED(csa->hdr))
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
		gtmsource_free_msgbuff();
		gtmsource_free_tcombuff();
		gtmsource_free_filter_buff();
		gtmsource_stop_heartbeat();
		if (FD_INVALID != gtmsource_sock_fd)
			repl_close(&gtmsource_sock_fd);
		if (gtmsource_filter & EXTERNAL_FILTER)
			repl_stop_filter();
	} while (TRUE);
	gtmsource_end();
	return(SS_NORMAL);
}
