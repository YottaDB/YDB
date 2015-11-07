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
#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"
#include "gtm_time.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_inet.h"

#include <sys/mman.h>
#include <errno.h>
#ifdef VMS
#include <ssdef.h>
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "cdb_sc.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for muprec.h and tp.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "tp.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "cli.h"
#include "error.h"
#include "repl_dbg.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_shutdcode.h"
#include "repl_sp.h"
#include "jnl_write.h"
#ifdef UNIX
#include "repl_instance.h"
#include "gtmio.h"
#include "repl_inst_dump.h"		/* for "repl_dump_histinfo" prototype */
#endif
#ifdef GTM_TRIGGER
#include <rtnhdr.h>			/* for rtn_tabent in gv_trigger.h */
#include "gv_trigger.h"
#include "targ_alloc.h"
#endif
#ifdef VMS
#include <fab.h>
#include <rms.h>
#include <iodef.h>
#include <secdef.h>
#include <psldef.h>
#include <lckdef.h>
#include <syidef.h>
#include <xab.h>
#include <prtdef.h>
#endif
#include "op.h"
#include "svnames.h"		/* for SV_ZTWORMHOLE */
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "read_db_files_from_gld.h"
#include "updproc.h"
#include "tp_change_reg.h"
#include "wcs_flu.h"
#include "repl_log.h"
#include "tp_restart.h"
#include "gtmmsg.h"	/* for gtm_putmsg() prototype */
#include "mu_gv_stack_init.h"
#include "jnl_typedef.h"
#include "memcoherency.h"
#include "aswp.h"
#include "jnl_get_checksum.h"
#include "updproc_get_gblname.h"
#include "wcs_recover.h"
#include "have_crit.h"
#include "wbox_test_init.h"
#include "format_targ_key.h"
#include "op_tcommit.h"
#include "error_trap.h"
#include "tp_frame.h"
#include "gvcst_jrt_null.h"	/* for gvcst_jrt_null prototype */
#include "preemptive_db_clnup.h"

#define	UPDPROC_WAIT_FOR_READJNLSEQNO	100	/* ms */
#define UPDPROC_WAIT_FOR_STARTJNLSEQNO	100	/* ms */

GBLREF	uint4			dollar_tlevel;
#ifdef DEBUG
GBLREF	uint4			dollar_trestart;
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif
GBLREF	gv_key			*gv_currkey;
GBLREF  gd_region               *gv_cur_region;
GBLREF  sgmnt_addrs             *cs_addrs;
GBLREF 	sgmnt_data_ptr_t 	cs_data;
GBLREF	recvpool_addrs		recvpool;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		is_updproc;
GBLREF	seq_num			seq_num_zero, seq_num_one;
GBLREF  gd_addr                 *gd_header;
GBLREF	FILE	 		*updproc_log_fp;
GBLREF	void			(*call_on_signal)();
GBLREF	sgm_info		*first_sgm_info;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF	gv_namehead		*reset_gv_target;
#ifdef VMS
GBLREF	struct chf$signal_array	*tp_restart_fail_sig;
GBLREF	boolean_t		tp_restart_fail_sig_used;
GBLREF	boolean_t		secondary_side_std_null_coll;
#endif
GBLREF	boolean_t		is_replicator;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		disk_blk_read;
GBLREF	seq_num			lastlog_seqno;
GBLREF	uint4			log_interval;
#ifdef GTM_TRIGGER
DEBUG_ONLY(GBLREF ch_ret_type	(*ch_at_trigger_init)();)
GBLREF	int			tprestart_state;        /* When triggers restart, multiple states possible. See tp_restart.h */
GBLREF	dollar_ecode_type	dollar_ecode;		/* structure containing $ECODE related information */
GBLREF	mval			dollar_ztwormhole;
#endif
GBLREF	boolean_t		skip_dbtriggers;
GBLREF	gv_namehead		*gv_target;
GBLREF	boolean_t		gv_play_duplicate_kills;
#ifdef UNIX
GBLREF		int4			strm_index;
STATICDEF	boolean_t		set_onln_rlbk_flg;
#endif

LITREF	mval			literal_hasht;
static	boolean_t		updproc_continue = TRUE;

error_def(ERR_GBLOFLOW);
error_def(ERR_GVIS);
error_def(ERR_REC2BIG);
error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPEATERROR);
error_def(ERR_REPLONLNRLBK);
error_def(ERR_SECONDAHEAD);
error_def(ERR_STRMSEQMISMTCH);
error_def(ERR_TEXT);
error_def(ERR_TPRETRY);
error_def(ERR_TRIGDEFNOSYNC);
error_def(ERR_UPDREPLSTATEOFF);

/* The below logic does "jnl_ensure_open" and other pre-requisites. This code is very similar to t_end.c */
#define	DO_JNL_ENSURE_OPEN(CSA, JPC)										\
{														\
	jnl_buffer_ptr_t	jbp;										\
	uint4			jnl_status;									\
														\
	assert(CSA = &FILE_INFO(gv_cur_region)->s_addrs);	/* so we can use gv_cur_region below */		\
	assert(JPC == CSA->jnl);										\
	SET_GBL_JREC_TIME;	/* needed for jnl_put_jrt_pini() */						\
	jbp = JPC->jnl_buff;											\
	/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order		\
	 * of jnl records. This needs to be done BEFORE the jnl_ensure_open as that could write			\
	 * journal records (if it decides to switch to a new journal file).					\
	 */													\
	ADJUST_GBL_JREC_TIME(jgbl, jbp);									\
	/* Make sure timestamp of this seqno is >= timestamp of previous seqno. Note: The below			\
	 * macro invocation should be done AFTER the ADJUST_GBL_JREC_TIME call as the below resets		\
	 * jpl->prev_jnlseqno_time. Doing it the other way around would mean the reset will happen		\
	 * with a potentially lower value than the final adjusted time written in the jnl record.		\
	 */													\
	ADJUST_GBL_JREC_TIME_JNLPOOL(jgbl, jnlpool_ctl);							\
	if (JNL_ENABLED(CSA))											\
	{													\
		jnl_status = jnl_ensure_open();									\
		if (0 == jnl_status)										\
		{												\
			if (0 == JPC->pini_addr)								\
				jnl_put_jrt_pini(CSA);								\
		} else												\
		{												\
			if (SS_NORMAL != JPC->status)								\
				rts_error_csa(CSA_ARG(CSA) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(CSA->hdr),	\
					DB_LEN_STR(gv_cur_region), JPC->status);				\
			else											\
				rts_error_csa(CSA_ARG(CSA) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(CSA->hdr),	\
					DB_LEN_STR(gv_cur_region));						\
		}												\
	}													\
}

#ifdef UNIX
# define UPDPROC_ONLN_RLBK_CLNUP(REG)						\
{										\
	sgmnt_addrs		*csa;						\
										\
	assert(0 != have_crit(CRIT_HAVE_ANY_REG));				\
	csa = &FILE_INFO(REG)->s_addrs;						\
	assert(csa->now_crit);							\
	SYNC_ONLN_RLBK_CYCLES;							\
	if (REG == jnlpool.jnlpool_dummy_reg)					\
		rel_lock(REG);							\
	else									\
		rel_crit(REG);							\
	RESET_ALL_GVT_CLUES;							\
	/* transfers control back to updproc_ch */				\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REPLONLNRLBK); 		\
}

/* Receiver eventually sees the upd_proc_local->onln_rlbk_flag being set, drains the replication pipe, closes the connection and
 * restarts. But, before that it also resets recvpool_ctl->jnl_seqno to 0. So, wait until recvpool_ctl->jnl_seqno is reset to 0
 * to be sure that receiver server did acknowledge the upd_proc_local->onln_rlbk_flag.
 */
#define WAIT_FOR_ZERO_RECVPOOL_JNL_SEQNO											\
{																\
	repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Waiting for receiver server to reset recvpool_ctl->jnl_seqno\n");	\
	while (recvpool_ctl->jnl_seqno)												\
	{															\
		SHORT_SLEEP(UPDPROC_WAIT_FOR_STARTJNLSEQNO);									\
		if (SHUTDOWN == upd_proc_local->upd_proc_shutdown)								\
		{														\
			updproc_end();												\
			return SS_NORMAL;											\
		}														\
	}															\
}

#define LOG_ONLINE_ROLLBACK_EVENT												\
{																\
	repl_log(updproc_log_fp, TRUE, TRUE, "Starting afresh due to ONLINE ROLLBACK\n");					\
	repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Current Jnlpool Seqno : %llu\n", jnlpool.jnlpool_ctl->jnl_seqno);	\
	repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Current Update process Read Seqno : %llu\n",				\
			upd_proc_local->read_jnl_seqno);									\
	assert(recvpool_ctl->jnl_seqno);											\
	repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Current Receive Pool Seqno : %llu\n",					\
			recvpool_ctl->jnl_seqno);										\
}
#endif

CONDITION_HANDLER(updproc_ch)
{
	int		rc;
	unsigned char	seq_num_str[32], *seq_num_ptr;
	unsigned char	seq_num_strx[32], *seq_num_ptrx;

	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
#		if defined(DEBUG) && defined(DEBUG_UPDPROC_TPRETRY)
		assert(FALSE);
#		endif
		repl_log(updproc_log_fp, TRUE, TRUE, " ----> TPRETRY for sequence number "INT8_FMT" "INT8_FMTX"\n",
			INT8_PRINT(recvpool.upd_proc_local->read_jnl_seqno),
			INT8_PRINTX(recvpool.upd_proc_local->read_jnl_seqno));
		/* This is a kludge. We can come here from 2 places.
		 *	( i) From a call to t_retry which does a rts_error(ERR_TPRETRY)
		 *	(ii) From updproc_actions() where immediately after op_tcommit we detect that dollar_tlevel is non-zero.
		 * In the first case, we need to do a tp_restart. In the second, op_tcommit would have already done it for us.
		 * The way we detect the second case is from the value of first_sgm_info since it is NULLified in tp_restart.
		 */
		if (first_sgm_info GTMTRIG_ONLY( || (TPRESTART_STATE_NORMAL != tprestart_state)))
		{
			VMS_ONLY(assert(FALSE == tp_restart_fail_sig_used);)
			rc = tp_restart(1, TP_RESTART_HANDLES_ERRORS); /* any nested errors will set SIGNAL accordingly */
			assert(0 == rc);			/* No partials restarts can happen at this final level */
			GTMTRIG_ONLY(assert(TPRESTART_STATE_NORMAL == tprestart_state));
			NON_GTMTRIG_ONLY(assert(INVALID_GV_TARGET == reset_gv_target);)
			reset_gv_target = INVALID_GV_TARGET; /* see "trigger_item_tpwrap_ch" similar code for why this is needed */
#			ifdef UNIX
			if (ERR_REPLONLNRLBK == SIGNAL) /* tp_restart did rts_error(ERR_REPLONLNRLBK) */
				set_onln_rlbk_flg = TRUE;
			if ((ERR_TPRETRY == SIGNAL) || (ERR_REPLONLNRLBK == SIGNAL))
#			elif defined VMS
			if (!tp_restart_fail_sig_used)		/* If tp_restart ran clean */
#			else
#			  error unsupported platform
#			endif
			{
				UNWIND(NULL, NULL);
			}
#			ifdef VMS
			else
			{	/* Otherwise tp_restart had a signal that we must now deal with.
				 * replace the TPRETRY information with that saved from tp_restart.
				 * first assert that we have room for these arguments and proper setup
				 */
				assert(TPRESTART_ARG_CNT >= tp_restart_fail_sig->chf$is_sig_args);
				memcpy(sig, tp_restart_fail_sig, (tp_restart_fail_sig->chf$l_sig_args + 1) * SIZEOF(int));
				tp_restart_fail_sig_used = FALSE;
			}
#			endif
		} else
		{
			UNWIND(NULL, NULL);
		}
	}
#	ifdef GTM_TRIGGER
	else if (ERR_REPEATERROR == SIGNAL)
		SIGNAL = dollar_ecode.error_last_ecode;	/* Error rethrown from a trigger */
#	endif
#	ifdef UNIX
	else if (ERR_REPLONLNRLBK == SIGNAL)
	{
		preemptive_db_clnup(SEVERITY);
		assert(INVALID_GV_TARGET == reset_gv_target);
		set_onln_rlbk_flg = TRUE;
		UNWIND(NULL, NULL);
	}
#	endif
	NEXTCH;
}

/* updproc performs its main processing in a function call invoked within a loop.
 * Unless there is a TPRESTART, the processing remains in the updproc_actions loop,
 *   but when a resource conflict triggers a TPRESTART, the condition handler drops
 *   back to the outer loop in updproc, which reissues the call.
 */
int updproc(void)
{
	seq_num			jnl_seqno; /* the current jnl_seq no of the Update process */
	seq_num			start_jnl_seqno;
	uint4			status;
	gld_dbname_list 	*gld_db_files, *curr;
	recvpool_ctl_ptr_t	recvpool_ctl;
#	ifdef VMS
	char			proc_name[PROC_NAME_MAXLEN + 1];
	struct dsc$descriptor_s proc_name_desc;
#	endif
	upd_proc_local_ptr_t	upd_proc_local;
	sgmnt_addrs		*repl_csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	call_on_signal = updproc_sigstop;
	GTMTRIG_DBG_ONLY(ch_at_trigger_init = &updproc_ch);
#	ifdef VMS
	/* Get a meaningful process name */
	proc_name_desc.dsc$w_length = get_proc_name(LIT_AND_LEN("GTMUPD"), getpid(), proc_name);
	proc_name_desc.dsc$a_pointer = proc_name;
	proc_name_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	proc_name_desc.dsc$b_class = DSC$K_CLASS_S;
	if (SS$_NORMAL != (status = sys$setprn(&proc_name_desc)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to change update process name"), status);
#	else
	/* In the update process, we want every replicated update from an originating instances to end up in a replicated region
	 * on this (the receiving) instance. If not, we will issue a UPDREPLSTATEOFF error. But it is possible that replication
	 * was turned ON at that time (since we dont hold crit at the time we do the UPDREPLSTATEOFF check) but later got turned
	 * OFF (for example due to no disk space for journal files etc.) just before the actual application of the update.
	 * In that case, the UPDREPLSTATEOFF error would not have been issued but there is still an issue in that an update from
	 * the primary got applied to a non-replicated database on the secondary. We prevent this out-of-sync situation from
	 * happening in the first place, by ensuring "jnl_file_lost" (the function that is invoked to turn journaling OFF) issues
	 * a runtime error and does not turn journaling off.
	 */
	TREF(error_on_jnl_file_lost) = JNL_FILE_LOST_ERRORS;
#	endif
	is_updproc = TRUE;
	is_replicator = TRUE;	/* as update process goes through t_end() and can write jnl recs to the jnlpool for replicated db */
	gv_play_duplicate_kills = TRUE;	/* needed to ensure seqnos are kept in sync between source and receiver instances */
	NON_GTMTRIG_ONLY(skip_dbtriggers = TRUE;)
	memset((uchar_ptr_t)&recvpool, 0, SIZEOF(recvpool)); /* For util_base_ch and mupip_exit */
	if (updproc_init(&gld_db_files, &start_jnl_seqno) == UPDPROC_EXISTS) /* we got the global directory header already */
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RECVPOOLSETUP, 0,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("Update Process already exists"));
	}
	OPERATOR_LOG_MSG;
	/* Initialization of all the relevant global datastructures and allocation for TP */
	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
	mu_gv_stack_init();
	upd_proc_local = recvpool.upd_proc_local;
	recvpool_ctl = recvpool.recvpool_ctl;
	while (TRUE)
	{
		upd_proc_local->read = 0;
		jnl_seqno = start_jnl_seqno;
		upd_proc_local->read_jnl_seqno = 0;
		SHM_WRITE_MEMORY_BARRIER;	/* Ensure upd_proc_local->read_jnl_seqno update is visible to the receiver server
						 * BEFORE recvpool_ctl->jnl_seqno update. This way we ensure whenever it gets to the
						 * stage where it sets upd_proc_local->read_jnl_seqno to a non-zero value (possible
						 * in the case of a non-supplementary -> supplementary replication connection),
						 * its update will happen AFTER of the update process update to "read_jnl_seqno"
						 * and not the other way around.
						 */
		recvpool_ctl->jnl_seqno = jnl_seqno;
#		ifdef UNIX
		/* At this point, the receiver server will see the non-zero jnl_seqno and start communicating with a source server.
		 * But if this is a supplementary root primary instance, the current instance jnl_seqno is not what the receiver
		 * will ask the source to start sending transactions from. It has to first figure out what non-supplementary
		 * stream# (strm_index) the connecting source maps to and find out the current strm_seqno[strm_index] in the
		 * instance file header. All this processing involves the receiver server. Until it finishes this processing, the
		 * update process cannot proceed. Wait for this to finish and update local copy of seqnos accordingly. The receiver
		 * server will set upd_proc_local->read_jnl_seqno to a non-zero value to signal completion of this step.
		 */
		if (jnlpool.repl_inst_filehdr->is_supplementary && !jnlpool.jnlpool_ctl->upd_disabled)
		{
			repl_log(updproc_log_fp, TRUE, TRUE, "Waiting for Receiver Server to write read_jnl_seqno\n");
			while (!upd_proc_local->read_jnl_seqno)
			{
				SHORT_SLEEP(UPDPROC_WAIT_FOR_READJNLSEQNO);
				if (SHUTDOWN == upd_proc_local->upd_proc_shutdown)
				{
					updproc_end();
					return(SS_NORMAL);
				}
				if (GTMRECV_NO_RESTART != recvpool.gtmrecv_local->restart)
				{	/* wait for restart to become GTMRECV_NO_RESTART (set by the Receiver Server) */
					if (GTMRECV_RCVR_RESTARTED == recvpool.gtmrecv_local->restart)
					{
						recvpool_ctl->jnl_seqno = jnl_seqno;
						assert(0 == upd_proc_local->read);
						recvpool.gtmrecv_local->restart = GTMRECV_UPD_RESTARTED;
						recvpool.upd_helper_ctl->first_done = FALSE;
						recvpool.upd_helper_ctl->pre_read_offset = 0;
					}
				}
				if (repl_csa->onln_rlbk_cycle != jnlpool_ctl->onln_rlbk_cycle)
				{	/* A concurrent online rollback. Handle it */
					LOG_ONLINE_ROLLBACK_EVENT;
					assert(!repl_csa->now_crit && !set_onln_rlbk_flg);
					grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
					SYNC_ONLN_RLBK_CYCLES;
					rel_lock(jnlpool.jnlpool_dummy_reg);
					upd_proc_local->onln_rlbk_flg = TRUE; /* let receiver know about the online rollback */
					WAIT_FOR_ZERO_RECVPOOL_JNL_SEQNO;
					upd_proc_local->read = 0;
					recvpool_ctl->jnl_seqno = jnl_seqno;
				}
			}
			/* Ensure all updates done by the receiver server BEFORE setting
			 * upd_proc_local->read_jnl_seqno are visible once we see the updated read_jnl_seqno.
			 */
			SHM_READ_MEMORY_BARRIER;
			repl_log(updproc_log_fp, TRUE, TRUE, "Receiver Server wrote upd_proc_local->read_jnl_seqno = "
				"%llu [0x%llx]\n", upd_proc_local->read_jnl_seqno, upd_proc_local->read_jnl_seqno);
			repl_log(updproc_log_fp, TRUE, TRUE, "Receiver Server wrote stream # = %d\n",
					recvpool.gtmrecv_local->strm_index);
			jnl_seqno = upd_proc_local->read_jnl_seqno;
			assert(0 == GET_STRM_INDEX(jnl_seqno));
			start_jnl_seqno = jnl_seqno;
			strm_index = recvpool.gtmrecv_local->strm_index;
		}
#		endif
		upd_proc_local->read_jnl_seqno = jnl_seqno;
		/* Check if the secondary is ahead of the primary */
#		ifdef VMS
		if ((jnlpool_ctl->jnl_seqno > start_jnl_seqno) && jnlpool_ctl->upd_disabled)
		{
			repl_log(updproc_log_fp, TRUE, TRUE,
				"JNLSEQNO last updated by  update process = "INT8_FMT" "INT8_FMTX"\n",
				INT8_PRINT(start_jnl_seqno), INT8_PRINTX(start_jnl_seqno));
			repl_log(updproc_log_fp, TRUE, TRUE,
				"JNLSEQNO of last transaction written to journal pool = "INT8_FMT" "INT8_FMTX"\n",
				INT8_PRINT(jnlpool_ctl->jnl_seqno), INT8_PRINTX(jnlpool_ctl->jnl_seqno));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_SECONDAHEAD);
		}
#		else
		/* The SECONDAHEAD check is performed in the receiver server after it has connected with the source
		 * server. This is because the check is relevant only if the source server is dualsite. That is
		 * not known now but instead at connection time. Hence the deferred check.
		 * Note: In the case of supplementary root primary instances, the jnl_seqno of the jnlpool is the
		 * unified seqno across all the non-supplementary streams as well as the local stream of updates.
		 * But the receive pool jnl_seqno is the seqno of the specific non-supplementary stream. So those
		 * cannot be compared at all. But they are guaranteed to be equal in all other cases. Assert that.
		 */
		assert((jnlpool_ctl->jnl_seqno == start_jnl_seqno)
			|| (jnlpool.repl_inst_filehdr->is_supplementary && !jnlpool.jnlpool_ctl->upd_disabled));
#		endif
		UNIX_ONLY(assert(updproc_continue && !set_onln_rlbk_flg));
		while (updproc_continue UNIX_ONLY(&& !set_onln_rlbk_flg))
			updproc_actions(gld_db_files);
#		ifdef UNIX
		if (set_onln_rlbk_flg)
		{	/* A concurrent online rollback happened which drove the updproc_ch and called us. Need to let the receiver
			 * server know about it and set up the sequence numbers
			 */
			LOG_ONLINE_ROLLBACK_EVENT;
			upd_proc_local->onln_rlbk_flg = TRUE; /* let receiver know about the online rollback */
			WAIT_FOR_ZERO_RECVPOOL_JNL_SEQNO;
			set_onln_rlbk_flg = FALSE;
			/* Since we are going to start afresh, do a OP_TROLLBACK if we are in TP. This brings the global variables
			 * dollar_tlevel, dollar_trestart all to a known state thereby erasing any history of lingering TP artifacts
			 */
			if (dollar_tlevel)
			{
				OP_TROLLBACK(0);
				assert(!dollar_tlevel);
				assert(!dollar_trestart);
			}
			start_jnl_seqno = jnlpool.jnlpool_ctl->jnl_seqno; /* needed when we go back to the beginning of the loop */
		} else
#		endif
		if (!updproc_continue)
			break;
	}
	updproc_end();
	return(SS_NORMAL);
}

void updproc_actions(gld_dbname_list *gld_db_files)
{
	mval			ts_mv, val_mv;
	jnl_record		*rec;
	uint4			temp_write, temp_read;
	enum jnl_record_type	rectype;
	int4			upd_rec_seqno = 0; /* the total no of journal records excluding TCOM records */
	int4			tupd_num; /* the number of tset/tkill/tzkill records encountered */
	int4			tcom_num; /* the number of tcom records encountered */
	seq_num			jnl_seqno, tmpseqno; /* the current jnl_seq no of the Update process */
	seq_num			last_errored_seqno = 0;
	int			key_len, rec_len, backptr;
	char			fn[MAX_FN_LEN];
	sm_uc_ptr_t		readaddrs;	/* start of current rec in pool */
	boolean_t		incr_seqno;
	seq_num			jnlpool_ctl_seqno, rec_strm_seqno, strm_seqno;
	char			*val_ptr;
	jnl_string		*keystr;
	mstr			mname;
	char			*key, *keytop;
	gv_key			*gv_failed_key = NULL, *gv_failed_key_ptr;
	unsigned char		*endBuff, fmtBuff[MAX_ZWR_KEY_SZ];
	enum upd_bad_trans_type	bad_trans_type;
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	sgmnt_addrs		*csa, *repl_csa, *tmpcsa = NULL;
	sgmnt_data_ptr_t	csd;
	char	           	gv_mname[MAX_KEY_SZ];
	unsigned char		buff[MAX_ZWR_KEY_SZ], *end, scan_char, next_char;
	boolean_t		log_switched = FALSE;
#	ifdef UNIX
	boolean_t		suppl_root_primary, suppl_propagate_primary;
	repl_histinfo		histinfo;
	repl_old_triple_jnl_t	*input_old_triple;
	repl_histrec_jnl_ptr_t	input_histjrec;
	uint4			expected_rec_len;
#	endif
	jnl_private_control	*jpc;
	gld_dbname_list		*curr;
	gd_region		*save_reg;
	uint4			write_wrap, cntr, last_nullsubs, last_subs, keyend;
#	ifdef GTM_TRIGGER
	uint4			nodeflags;
	boolean_t		primary_has_trigdef, secondary_has_trigdef;
	const char		*trigdef_inst = NULL, *no_trigdef_inst = NULL;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef UNIX
	assert((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open);
	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
	DEBUG_ONLY(
		assert(!repl_csa->hold_onto_crit); /* so we can do unconditional grab_lock/rel_lock below */
		ASSERT_VALID_JNLPOOL(repl_csa);
	)
#	endif
	ESTABLISH(updproc_ch);
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	upd_helper_ctl = recvpool.upd_helper_ctl;
	temp_read = upd_proc_local->read;
	temp_write = recvpool_ctl->write;
	readaddrs = recvpool.recvdata_base + temp_read;
	upd_rec_seqno = tupd_num = tcom_num = 0;
	jnl_seqno = upd_proc_local->read_jnl_seqno;
	log_interval = upd_proc_local->log_interval;
	lastlog_seqno = jnl_seqno - log_interval; /* to ensure we log the first transaction */
#	ifdef UNIX
	if (jnlpool.repl_inst_filehdr->is_supplementary)
	{
		if (!jnlpool.jnlpool_ctl->upd_disabled)
		{
			suppl_root_primary = TRUE;
			suppl_propagate_primary = FALSE;
		} else
		{
			suppl_root_primary = FALSE;
			suppl_propagate_primary = TRUE;
		}
	} else
	{
		suppl_root_primary = FALSE;
		suppl_propagate_primary = FALSE;
	}
#	endif
	while (TRUE)
	{
		incr_seqno = FALSE;
		UNIX_ONLY(assert(0 == GET_STRM_INDEX(jnl_seqno));)
		if (SHUTDOWN == upd_proc_local->upd_proc_shutdown)
			break;
		if (GTMRECV_NO_RESTART != gtmrecv_local->restart)
		{	/* wait for restart to become GTMRECV_NO_RESTART (set by the Receiver Server) */
			if (GTMRECV_RCVR_RESTARTED == gtmrecv_local->restart)
			{
				recvpool.recvpool_ctl->jnl_seqno = jnl_seqno;
				readaddrs = recvpool.recvdata_base;
				upd_proc_local->read = 0;
				gtmrecv_local->restart = GTMRECV_UPD_RESTARTED;
				upd_helper_ctl->first_done = FALSE;
				upd_helper_ctl->pre_read_offset = 0;
				temp_read = 0;
				temp_write = 0;
				if (0 < tupd_num)
				{
					assert(donot_INVOKE_MUMTSTART);
					assert(dollar_tlevel);
					OP_TROLLBACK(0);
					tupd_num = 0;
				}
			}
			SHORT_SLEEP(10);
			continue;
		}
		if (upd_proc_local->changelog)
		{
			if (upd_proc_local->changelog & REPLIC_CHANGE_UPD_LOGINTERVAL)
			{
				repl_log(updproc_log_fp, TRUE, TRUE, "Changing log interval from %u to %u\n",
						log_interval, upd_proc_local->log_interval);
				log_interval = upd_proc_local->log_interval;
				lastlog_seqno = jnl_seqno - log_interval; /* force logging of the first transaction after the
									   * change in log interval */
			}
			if (upd_proc_local->changelog & REPLIC_CHANGE_LOGFILE)
			{
				log_switched = TRUE;
				upd_log_init(UPDPROC);
			}
			if ( log_switched == TRUE )
				repl_log(updproc_log_fp, TRUE, TRUE, "Change log to %s successful\n",
                                                     recvpool.upd_proc_local->log_file);
			upd_proc_local->changelog = 0;
		}
		/* Assert that dollar_tlevel & tupd_num are in sync with each other. The only exception is if dollar_tlevel
		 * is non-zero, it is possible tupd_num is 0 in case we come here after a TP restart.
		 */
		assert((dollar_tlevel && (tupd_num || dollar_trestart)) || !dollar_tlevel && !tupd_num);
		if (upd_proc_local->bad_trans UNIX_ONLY(|| upd_proc_local->onln_rlbk_flg)
			|| (!dollar_tlevel && (FALSE == recvpool.recvpool_ctl->wrapped)
				&& (temp_write = recvpool.recvpool_ctl->write) == upd_proc_local->read))
		{	/* bad-trans OR online rollback OR nothing to process. In case of bad_trans or online rollback, wait for
			 * the receiver to reset the corresponding fields. In case there is nothing to process, wait until data
			 * is available in the receive pool.
			 * Note that we check two shared memory fields "recvpool_ctl->wrapped" and "recvpool_ctl->write" in
			 * that order. The receiver server updates them in the opposite order when it sets "wrapped" to TRUE.
			 * So it is possible we read an uptodate value of "write" but an outofdate value of "wrapped". This
			 * means we could incorrectly conclude the pool is empty when actually it is full. But we expect these
			 * situations to be rare enough that it is ok to do an idle buffer flush in this case. We will eventually
			 * get to see the uptodate value of "wrapped" at which point we will move on to process the transactions.
			 */
			assert((0 == recvpool.recvpool_ctl->jnl_seqno) || (jnl_seqno <= recvpool.recvpool_ctl->jnl_seqno));
				/* the 0 == check takes care of the startup case where jnl_seqno is 0 in the recvpool_ctl */
			SHORT_SLEEP(10);
#			ifdef UNIX
			if (!upd_proc_local->onln_rlbk_flg && (repl_csa->onln_rlbk_cycle != jnlpool.jnlpool_ctl->onln_rlbk_cycle))
			{	/* A concurrent online rollback happened. Start afresh */
				grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
				UPDPROC_ONLN_RLBK_CLNUP(jnlpool.jnlpool_dummy_reg); /* No return */
			} /* else onln_rlbk_flag is already set and the receiver should take the next appropriate action */
#			endif
			continue;
		}
		/* The update process reads "recvpool_ctl->write" first and assumes that all data in the receive pool
		 * that it then reads (upto the "write" offset) is valid. In order for this assumption to hold good, the
		 * receiver server needs to do a write memory barrier after updating the receive pool data but before
		 * updating "write". The update process does a read memory barrier after reading "write" but before reading
		 * the receive pool data. Not enforcing this read order would mean we would have seen an updated "write"
		 * but not yet see the updated receive pool data which would mean whatever data we read from the receive pool
		 * will be stale (which if successfully processed could cause primary-secondary dbs to get out of sync).
		 */
		SHM_READ_MEMORY_BARRIER;
		/* To take the wrapping of buffer in case of over flow ------------ */
		/*     assume receiver will update wrapped even for exact overflows */
		write_wrap = recvpool.recvpool_ctl->write_wrap;
		if (temp_read >= write_wrap)
		{
			assert(temp_read == write_wrap);
			if (0 < tupd_num)	/* receive pool cannot wrap in the middle of TP */
				GTMASSERT;	/* see process_tr_buff in gtmrecv_process for why */
			if (FALSE == recvpool.recvpool_ctl->wrapped)
			{ 	/* Update process in keeping up with receiver server, notices that there was a wrap
				 * (thru write and write_wrap). It has to wait till receiver sets wrapped.
				 */
				SHORT_SLEEP(1);
				continue;
			}
			DEBUG_ONLY(
				repl_log(updproc_log_fp, TRUE, FALSE,
				       "-- Wrapping -- read = %ld :: write_wrap = %ld :: upd_jnl_seqno = "INT8_FMT" "INT8_FMTX"\n",
					temp_read, write_wrap, INT8_PRINT(jnl_seqno),INT8_PRINTX(jnl_seqno));
				repl_log(updproc_log_fp, TRUE, TRUE,
					"-------------> wrapped = %ld :: write = %ld :: recv_jnl_seqno = "INT8_FMT" "INT8_FMTX"\n",
					recvpool.recvpool_ctl->wrapped, recvpool.recvpool_ctl->write,
					INT8_PRINT(recvpool.recvpool_ctl->jnl_seqno),
					INT8_PRINTX(recvpool.recvpool_ctl->jnl_seqno));
			)
			/* The update process reads (a) "recvpool_ctl->wrapped" first and then reads (b) "recvpool_ctl->write".
			 * If "wrapped" is TRUE, it assumes that "write" will never hold a stale value that corresponds to a
			 * previous state of "wrapped" For this assumption to hold good, the receiver server needs to do a write
			 * memory barrier after updating "write" but before updating "wrapped". The update process will do a read
			 * memory barrier after reading "wrapped" but before reading "write". This way we are guaranteed the
			 * update process will never see a value of "write" that is at the tail end of the receive pool once it
			 * sees "wrapped" as TRUE. Not having this guarantee means we would have wrapped to read from the
			 * beginning of the receive pool but will continue to see "write" at the tail of the receive pool and will
			 * therefore treat almost the entire receive pool as containing unprocessed data when it is not the case.
			 */
			SHM_READ_MEMORY_BARRIER;
			temp_read = 0;
			upd_proc_local->read = 0;
			recvpool.recvpool_ctl->wrapped = FALSE;
			readaddrs = recvpool.recvdata_base;
			temp_write = recvpool.recvpool_ctl->write;
			if (0 == temp_write)
				continue; /* Receiver server wrapped but hasn't yet written anything into the pool */
		}
		rec = (jnl_record *)readaddrs;
		rectype = (enum jnl_record_type)rec->prefix.jrec_type;
		rec_len = rec->prefix.forwptr;
		assert(IS_REPLICATED(rectype));
#		ifdef UNIX
		if ((JRT_TRIPLE == rectype) || (JRT_HISTREC == rectype))
		{	/* Source server has sent a REPL_TRIPLE or REPL_HISTREC message in the middle of logical journal
			 * records. Construct the repl_histrec structure from the input message and add this history
			 * record to the replication instance file. In case of REPL_TRIPLE message, the source server
			 * is running a pre-supplementary version of GT.M so null-fill those fields in the history
			 * record that did not exist in that older version.
			 */
			if (JRT_TRIPLE == rectype)
			{
				repl_log(updproc_log_fp, TRUE, TRUE, "Processing REPL_TRIPLE message\n");
				input_old_triple = (repl_old_triple_jnl_ptr_t)readaddrs;
				histinfo.start_seqno = input_old_triple->start_seqno;
				histinfo.strm_seqno = 0;
				memcpy(histinfo.root_primary_instname, input_old_triple->instname, MAX_INSTNAME_LEN - 1);
				histinfo.root_primary_cycle = input_old_triple->cycle;
				histinfo.creator_pid = 0;
				histinfo.created_time = 0;
				histinfo.strm_index = 0;
				histinfo.history_type = HISTINFO_TYPE_NORMAL;
				NULL_INITIALIZE_REPL_INST_UUID(histinfo.lms_group);
				/* The following fields will be initialized in the "repl_inst_histinfo_add" function call below.
				 *	histinfo.histinfo_num
				 *	histinfo.prev_histinfo_num
				 *	histinfo.last_histinfo_num[]
				 */
				expected_rec_len = SIZEOF(repl_old_triple_jnl_t);
			} else
			{
				repl_log(updproc_log_fp, TRUE, TRUE, "Processing REPL_HISTREC message\n");
				input_histjrec = (repl_histrec_jnl_ptr_t)readaddrs;
				histinfo = input_histjrec->histcontent;
				expected_rec_len = SIZEOF(repl_histrec_jnl_t);
			}
			if (expected_rec_len != rec_len)
			{
				bad_trans_type = upd_bad_histinfo_len;
				assert(FALSE);
			} else if (histinfo.start_seqno != recvpool.upd_proc_local->read_jnl_seqno)
			{
				bad_trans_type = upd_bad_histinfo_start_seqno1;
				assert(FALSE);
			} else if (histinfo.start_seqno > recvpool_ctl->jnl_seqno)
			{
				bad_trans_type = upd_bad_histinfo_start_seqno2;
				assert(FALSE);
			} else
				bad_trans_type = upd_good_record;
			if (upd_good_record != bad_trans_type)
			{	/* Signal a BADTRANS */
				repl_log(updproc_log_fp, TRUE, TRUE,
					"-> Bad trans :: bad_trans_type = %ld type = %ld len = %ld "
					"start_seqno = %llu [0x%llx] upd_read_seqno = %llu [0x%llx] "
					"recvpool_jnl_seqno = %llu [0x%llx]\n",
					bad_trans_type, rectype, rec_len, histinfo.start_seqno, histinfo.start_seqno,
					recvpool.upd_proc_local->read_jnl_seqno, recvpool.upd_proc_local->read_jnl_seqno,
					recvpool_ctl->jnl_seqno, recvpool_ctl->jnl_seqno);
				upd_proc_local->bad_trans = TRUE;
				assert(!dollar_tlevel);
				if (dollar_tlevel)
				{
					repl_log(updproc_log_fp, TRUE, TRUE, "OP_TROLLBACK IS CALLED "
						"-->Bad trans :: dollar_tlevel = %ld\n", dollar_tlevel);
					OP_TROLLBACK(0);
				}
				readaddrs = recvpool.recvdata_base;
				upd_proc_local->read = 0;
				temp_read = 0;
				temp_write = 0;
				upd_rec_seqno = tupd_num = tcom_num = 0;
				continue;
			}
			if (jnlpool.repl_inst_filehdr->is_supplementary)
			{
				assert(0 <= histinfo.strm_index);
				assert((0 == histinfo.strm_index) || IS_REPL_INST_UUID_NON_NULL(histinfo.lms_group));
				assert((0 != histinfo.strm_index) || IS_REPL_INST_UUID_NULL(histinfo.lms_group));
				/* Check if this is an -UPDATERESYNC type of history record that starts a new stream of updates
				 * (or a -NORESYNC which creates a discontinuity from the previous stream).
				 * If so, reset all stream-related information (corresponding to the prior stream) in the db file
				 * hdr and journal pool. Otherwise if we crash after having processed one logical record after the
				 * history record, it is possible the regions unaffected by the logical update might have some
				 * garbage "strm_seqno" value which might affect the max_strm_seqno determination in case we do a
				 * rollback (this uses the strm_seqno values from the db file header) right after the crash.
				 */
				if ((HISTINFO_TYPE_NORESYNC == histinfo.history_type)
					|| (HISTINFO_TYPE_UPDRESYNC == histinfo.history_type))
				{	/* Also write an EPOCH with the new strm_seqno. This will help journal recovery know a fresh
					 * stream of updates begins and that the sequence of strm_seqnos might see a discontinuity.
					 */
					assert((0 < histinfo.strm_index) && (MAX_SUPPL_STRMS > histinfo.strm_index));
					/* Assert strm_seqno is non-zero as this is going to be filled in cs_data and jnlpool_ctl.
					 * Note that in case of a root primary supplementary instance, the history record is
					 * received from a non-supplementary instance whose history record has a zero strm_seqno
					 * so use the start_seqno instead as the strm_seqno. On the other hand, in case of a
					 * propagating primary supplementary, the history record is received from a supplementary
					 * instance in which case the strm_seqno field would have been properly populated so use it.
					 */
					strm_seqno = (suppl_propagate_primary ? histinfo.strm_seqno : histinfo.start_seqno);
					assert(strm_seqno);
					save_reg = gv_cur_region;
					for (curr = gld_db_files; NULL != curr; curr = curr->next)
					{
						TP_CHANGE_REG(curr->gd);
						assert(!gv_cur_region->read_only);
						grab_crit(gv_cur_region);
						if (cs_addrs->onln_rlbk_cycle != cs_addrs->nl->onln_rlbk_cycle)
							UPDPROC_ONLN_RLBK_CLNUP(gv_cur_region); /* No return */
						cs_data->strm_reg_seqno[histinfo.strm_index] = strm_seqno;
						/* Before doing "wcs_flu", do a "jnl_ensure_open" to ensure the journal file
						 * is open in shared memory as otherwise we might skip writing the EPOCH
						 * record in case this is the first history record that is being received on
						 * this instance and no other process on this instance has opened the
						 * journal file yet. But before doing jnl_ensure_open, do some time adjustments
						 * like is done in t_end.c (see comments there for details).
						 */
						jpc = cs_addrs->jnl;
						DO_JNL_ENSURE_OPEN(cs_addrs, jpc); /* does "jnl_ensure_open" and pre-requisites */
						wcs_flu(WCSFLU_FSYNC_DB | WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
						rel_crit(gv_cur_region);
					}
					TP_CHANGE_REG(save_reg);
					grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
					if (repl_csa->onln_rlbk_cycle != jnlpool_ctl->onln_rlbk_cycle)
						UPDPROC_ONLN_RLBK_CLNUP(jnlpool.jnlpool_dummy_reg); /* No return */
					jnlpool_ctl->strm_seqno[histinfo.strm_index] = strm_seqno;
					rel_lock(jnlpool.jnlpool_dummy_reg);
					/* Ideally we also want to do a "repl_inst_flush_filehdr" to make the strm_seqno in the
					 * instance file header also uptodate but that is anyways going to be done as part of
					 * the "repl_inst_histinfo_add" call below so we skip doing it separately here.
					 */
				}
 				assert(jnlpool.jnlpool_ctl->upd_disabled || (strm_index == histinfo.strm_index));
			}
			/* Now that we have constructed the history, add it to the instance file. */
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
			if (repl_csa->onln_rlbk_cycle != jnlpool_ctl->onln_rlbk_cycle)
				UPDPROC_ONLN_RLBK_CLNUP(jnlpool.jnlpool_dummy_reg); /* No return */
			repl_inst_histinfo_add(&histinfo);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			/* Dump the history contents AFTER the "repl_inst_histinfo_add" and "wcs_flu" above.
			 * This way, in case of a -updateresync or -noresync history record addition,
			 * (particularly for the first A->P connection), the user is guaranteed that
			 * seeing the history dump message indicates the receiver on P can be cleanly shutdown
			 * and a restart of the receiver does not need to use the -updateresync.
			 */
			repl_dump_histinfo(updproc_log_fp, TRUE, TRUE, "New History Content", &histinfo);
			/* Update pointers to indicate this record is now processed and move on to the next. */
			readaddrs += rec_len;
			temp_read += rec_len;
			upd_proc_local->read = temp_read;
			continue;
		}
#		endif
		/* NOTE: All journal sequence number fields are at same offset */
		if (ROUND_DOWN2(rec_len, JNL_REC_START_BNDRY) != rec_len)
		{	/* We need that so REC_LEN_FROM_SUFFIX does not access unaligned int */
			bad_trans_type = upd_bad_forwptr;
			assert(FALSE);
		} else if ((0 == rec_len) || (rec_len != (backptr = REC_LEN_FROM_SUFFIX(readaddrs, rec_len))))
		{
			bad_trans_type = upd_bad_backptr;
			assert(FALSE);
		} else if (!IS_REPLICATED(rectype))
		{
			bad_trans_type = upd_rec_not_replicated;
			assert(FALSE);
		} else if (jnl_seqno != rec->jrec_null.jnl_seqno)
		{
			bad_trans_type = upd_bad_jnl_seqno;
			assert(FALSE);
		} else
		{
			bad_trans_type = upd_good_record;
			assert((jnl_seqno < recvpool.recvpool_ctl->jnl_seqno) ||
				(0 == recvpool.recvpool_ctl->jnl_seqno));
			if (JRT_TCOM == rectype)
			{
				assert(0 != upd_rec_seqno);	/* we know TCOM is not created without a SET/KILL/ZKILL */
				if (0 != upd_rec_seqno)
					tcom_num++;
			} else if (IS_SET_KILL_ZKILL_ZTRIG(rectype))
			{
				assert(!IS_ZTP(rectype));
				keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
				GTMTRIG_ONLY(
					nodeflags = keystr->nodeflags;
					TRIG_PROCESS_JNL_STR_NODEFLAGS(nodeflags);
				)
				key_len = keystr->length;	/* local copy of shared recvpool key */
				if ((MAX_KEY_SZ >= key_len && 0 < key_len && 0 == keystr->text[key_len - 1]) &&
					(upd_good_record == updproc_get_gblname(keystr->text, key_len, gv_mname, &mname)))
				{
					if (IS_SET(rectype))
					{
						val_mv.mvtype = MV_STR;
						val_ptr = &keystr->text[keystr->length];
						GET_MSTR_LEN(val_mv.str.len, val_ptr); /* length of value validated later */
						val_mv.str.addr = val_ptr + SIZEOF(mstr_len_t);
					}
					if (IS_FENCED(rectype))
					{
						if (IS_TP(rectype))
						{
							assert(0 <= tupd_num);
							assert(0 == tcom_num);
							if (0 > tupd_num || 0 != tcom_num)
							{
								bad_trans_type = upd_fence_bad_t_num;
								assert(FALSE);
							} else if (IS_TUPD(rectype))
							{
								ts_mv.mvtype = MV_STR;
								ts_mv.str.len = 0;
								ts_mv.str.addr = NULL;
								assert((!dollar_tlevel && !tupd_num) || dollar_tlevel
										&& (tupd_num || dollar_trestart));
								if (!dollar_tlevel)
								{
									assert(!donot_INVOKE_MUMTSTART);
									DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE);
									op_tstart(IMPLICIT_TSTART, TRUE, &ts_mv, 0);
										/* 0 ==> save no locals but RESTART OK */
								}
								tupd_num++;
							}
							upd_rec_seqno++;
						} else if (IS_FUPD(rectype))
							op_ztstart();
					} else if (0 != tupd_num)
					{
						bad_trans_type = upd_nofence_bad_tupd_num;
						assert(FALSE);
					}
				} else
				{
					bad_trans_type = upd_bad_key;
					assert(FALSE);
				}
			} else if (IS_ZTWORM(rectype))
			{
				assert(IS_FENCED(rectype));
				assert(IS_TP(rectype));
				assert(0 <= tupd_num);
				assert(0 == tcom_num);
				if (0 > tupd_num || 0 != tcom_num)
				{
					bad_trans_type = upd_fence_bad_ztworm_t_num;
					assert(FALSE);
				} else if (IS_TUPD(rectype))
				{
					ts_mv.mvtype = MV_STR;
					ts_mv.str.len = 0;
					ts_mv.str.addr = NULL;
					assert((!dollar_tlevel && !tupd_num) || dollar_tlevel && (tupd_num || dollar_trestart));
					if (!dollar_tlevel)
						 /* 0 ==> save no locals but RESTART OK */
						op_tstart(IMPLICIT_TSTART, TRUE, &ts_mv, 0);
					tupd_num++;
				}
				upd_rec_seqno++;
			}
		}
		if (upd_good_record == bad_trans_type)
		{
#			ifdef UNIX
			if (suppl_propagate_primary)
			{
				rec_strm_seqno = GET_STRM_SEQNO(rec);
				strm_index = GET_STRM_INDEX(rec_strm_seqno);
				rec_strm_seqno = GET_STRM_SEQ60(rec_strm_seqno);
				strm_seqno = jnlpool_ctl->strm_seqno[strm_index];
				/* In the event of a concurrent ONLINE ROLLBACK, it is likely that the strm_seqno is less than
				 * rec_strm_seqno. In that case, do not issue STRMSEQMISMTCH error as that would be incorrect.
				 * Instead, continue and eventually, either the update process or the transaction processing logic
				 * will detect the online rollback and take appropriate action.
				 */
				if ((rec_strm_seqno != strm_seqno) && (repl_csa->onln_rlbk_cycle == jnlpool_ctl->onln_rlbk_cycle))
				{
					assert(FALSE);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_STRMSEQMISMTCH, 3,
							strm_index, &rec_strm_seqno, &strm_seqno);
				}
			}
#			endif
			if (JRT_NULL == rectype)
			{	/* Play the NULL transaction into the database and journal files */
				save_reg = gv_cur_region;
				gv_cur_region = gld_db_files->gd;
				tp_change_reg();
				DEBUG_ONLY(csa = cs_addrs;)
				assert(!csa->hold_onto_crit);
				assert(!csa->now_crit);
				gvcst_jrt_null();
				incr_seqno = TRUE;
				/* Restore gv_cur_region to what it was before the NULL record processing started. op_tstart
				 * relies on the fact that gv_target->gd_csa and cs_addrs be in sync. If prior to NULL record
				 * processing, gv_target->gd_csa pointed to AREG and after NULL record processing, gv_cur_region
				 * points to DEFAULT then the op_tstart assert DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC will fail.
				 */
				TP_CHANGE_REG(save_reg);
			} else if (JRT_TCOM == rectype)
			{
				if (tcom_num == tupd_num)
				{
					assert(0 != tcom_num);
					memcpy(tcom_record.jnl_tid, rec->jrec_tcom.jnl_tid, TID_STR_SIZE);
					op_tcommit();
					if (dollar_tlevel)
					{	/* op_tcommit restarted the transaction - do update process special
						 * handling for tpretry.  The error below has special handling in a
						 * few condition handlers because it not so much signals an error
			   			 * as it does drive the necessary mechanisms to invoke a restart.
						 * Consequently this error can be overridden by a "real" error.
						 * For VMS, the extra parameters are specified to provide "placeholders"
						 * on the stack in the signal array if a real error needs to be
						 * overlayed in place of this one (see code in updproc_ch).
						 * Defined in tp.h, the below issues ERR_TPRETRY.
						*/
						INVOKE_RESTART;
					}
					DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE);
					/* Following two changes to dollar_ztwormhole reset the value of $ztwormhole to
					 * "undefined" which is important since it points into the receiver pool area
					 * that is about to be allowed to be overwritten. Prior to the next reference,
					 * to dollar_ztwormhole, it should be included in a journal record. Note since
					 * the ISV must always be defined op_svget will turn this state into an empty
					 * string if somehow $ZTWORMHOLE is referenced before the replicating instance
					 * receives a new value (like in a jobexam dump).
					 */
					GTMTRIG_ONLY(dollar_ztwormhole.mvtype = 0);
					GTMTRIG_ONLY(dollar_ztwormhole.str.len = 0);
					tcom_num = tupd_num = upd_rec_seqno = 0;
					incr_seqno = TRUE;
				}
			} else if (JRT_ZTCOM == rectype)
			{
				assert(dollar_tlevel);
				op_ztcommit(1);
				incr_seqno = TRUE;
			} else if (IS_SET_KILL_ZKILL_ZTRIG(rectype))
			{
				key = keystr->text;
				UPD_GV_BIND_NAME_APPROPRIATE(gd_header, mname, key, key_len);	/* if ^#t do special processing */
				/* the above would have set gv_target and gv_cur_region appropriately */
				csa = &FILE_INFO(gv_cur_region)->s_addrs;
				if (!REPL_ALLOWED(csa))
				{	/* Replication/Journaling is NOT enabled on the database file that the current
					 * global maps to on the secondary even though it was enabled on the corresponding
					 * database file on the primary. Do NOT allow this update to happen as otherwise
					 * the journal seqno on the secondary database will get out-of-sync with that of
					 * the primary database.
					 */
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_UPDREPLSTATEOFF, 4,
						mname.len, mname.addr, DB_LEN_STR(gv_cur_region));
					/* Shut down the update process normally */
					upd_proc_local->upd_proc_shutdown = SHUTDOWN;
					break;
				}
				memcpy(gv_currkey->base, keystr->text, keystr->length);
				gv_currkey->base[keystr->length] = 0; 	/* second null of a key terminator */
				gv_currkey->end = keystr->length;
				if (gv_currkey->end + 1 > gv_cur_region->max_key_size)
				{
					bad_trans_type = upd_bad_key_size;
					tmpcsa = csa;
					assert(gtm_white_box_test_case_enabled
						&& (WBTEST_UPD_PROCESS_ERROR == gtm_white_box_test_case_number));
				} else
				{
					/* Scan the global for two reasons :
					 * (a) Need to setup gv_currkey->prev as update process can invoke triggers which
					 * could use naked references which relies on gv_currkey->prev being properly set
					 * (b) Set gv_some_subsc_null and gv_last_subsc_null accordingly to issue GTM-E-NULSUSBC
					 * if needed.
					 */
					key = (char *)(gv_currkey->base);
					keyend = gv_currkey->end;
					cntr = last_nullsubs = last_subs = 0;
					assert((0 < keyend) && (KEY_DELIMITER != *key)); /* we better not have an empty key */
					assert((KEY_DELIMITER == key[keyend])
						&& (KEY_DELIMITER == key[keyend - 1]));
					keytop = (char *)((&gv_currkey->base[0]) + keyend);
					scan_char = (unsigned char)(*key);
					while (cntr < keyend)
					{
						if (key >= keytop)
						{	/* We should never come here as this would mean that we are attempting
							 * to cross gv_currkey->base boundary. */
							assert(FALSE);
							break;
						}
						assert(scan_char == (unsigned char)(*key)); /* ensure that scan_char was updated in
											     * the previous iteration*/
						next_char = (unsigned char)(*(key + 1));
						/* null subscripts are identified based on whether the region has standard null
						 * collation or default null collation. If former, the sequence is 0x01 0x00.
						 * If latter, the sequence is 0xFF 0x00
						 */
						if (last_subs == cntr)
						{	/* Beginning of a new subscript. Ensure that if we have standard null
							 * collation, then we better don't see 0xFF 0x00 at the start of a
							 * subscript. Similary, if we have default null collation, we better not
							 * see 0x01 0x00 at the start of a subscript
							 */
							if (KEY_DELIMITER == next_char)
							{
								if (STR_SUB_PREFIX == scan_char)
								{
									VMS_ONLY(assert(!secondary_side_std_null_coll);)
									UNIX_ONLY(
										assert(!recvpool_ctl->this_side.is_std_null_coll);)
									last_nullsubs = cntr;
								} else if (SUBSCRIPT_STDCOL_NULL == scan_char)
								{
									VMS_ONLY(assert(secondary_side_std_null_coll);)
									UNIX_ONLY(assert(recvpool_ctl->this_side.is_std_null_coll);)
									last_nullsubs = cntr;
								}
							}
						}
						cntr++;
						if ((KEY_DELIMITER == scan_char) && (KEY_DELIMITER != next_char))
						{	/* New subscript. Note down the position for gv_currkey->prev. */
							last_subs = cntr;
						}
						key++;
						scan_char = next_char; /* set scan_char for next iteration */
					}
					assert(cntr == keyend);
					assert(last_subs < keyend);
					assert(last_nullsubs < keyend);
					TREF(gv_some_subsc_null) = (last_nullsubs && (last_nullsubs < last_subs));
					TREF(gv_last_subsc_null) = (last_nullsubs && (last_nullsubs == last_subs));
					gv_currkey->prev = last_subs;
					if (IS_KILL(rectype))
						op_gvkill();
					else if (IS_ZKILL(rectype))
						op_gvzwithdraw();
#					ifdef GTM_TRIGGER
					else if (IS_ZTRIG(rectype))
						op_ztrigger();
#					endif
					else
					{
						assert(IS_SET(rectype));
						if (VMS_ONLY(keystr->length + 1 + SIZEOF(rec_hdr) +) val_mv.str.len >
										gv_cur_region->max_rec_size)
						{
							bad_trans_type = upd_bad_val_size;
							tmpcsa = csa;
							assert(gtm_white_box_test_case_enabled
								&& (WBTEST_UPD_PROCESS_ERROR == gtm_white_box_test_case_number));
						} else
						op_gvput(&val_mv);
					}
#					ifdef GTM_TRIGGER
					if (!gv_target->trig_mismatch_test_done)
					{
						gv_target->trig_mismatch_test_done = TRUE; /* reset only in targ_alloc */
						primary_has_trigdef = (0 != (nodeflags & JS_HAS_TRIGGER_MASK));
						secondary_has_trigdef = (NULL != gv_target->gvt_trigger);
						if (primary_has_trigdef != secondary_has_trigdef)
						{	/* trigger definitions out-of-sync between the primary and secondary.
							 * Issue warning in operator log
							 */
							if (primary_has_trigdef)
							{
								trigdef_inst = "originating";
								no_trigdef_inst = "replicating";
							} else
							{
								trigdef_inst = "replicating";
								no_trigdef_inst = "originating";
							}
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_TRIGDEFNOSYNC, 7,
									mname.len, mname.addr, LEN_AND_STR(trigdef_inst),
									LEN_AND_STR(no_trigdef_inst), &jnl_seqno);
						}
					}
#					endif
					if ((upd_good_record == bad_trans_type) && !IS_TP(rectype))
						incr_seqno = TRUE;
					if (disk_blk_read || 0 >= csa->n_pre_read_trigger)
					{
						csd = csa->hdr;
						upd_helper_ctl->first_done = FALSE;
						upd_helper_ctl->pre_read_offset = temp_read + rec_len;
						REPL_DPRINT2("pre_read_offset = %x\n", upd_helper_ctl->pre_read_offset);
						csa->n_pre_read_trigger = (int)((csd->n_bts * (float)csd->reserved_for_upd /
						csd->avg_blks_per_100gbl) * csd->pre_read_trigger_factor / 100.0);
					} else
						csa->n_pre_read_trigger--;
					disk_blk_read = FALSE;
				}
			} else if (IS_ZTWORM(rectype))
			{
				assert(dollar_tlevel);	/* op_tstart should already have been done */
				val_mv.mvtype = MV_STR;
				val_mv.str.len = rec->jrec_ztworm.ztworm_str.length;
				val_mv.str.addr = &rec->jrec_ztworm.ztworm_str.text[0];
				op_svput(SV_ZTWORMHOLE, &val_mv);
			}
		}
		if (upd_good_record != bad_trans_type)
		{
			tmpseqno = IS_REPLICATED(rectype) ? rec->jrec_null.jnl_seqno : jnl_seqno;
			repl_log(updproc_log_fp, TRUE, TRUE,
				"-> Bad trans :: bad_trans_type = %ld type = %ld len = %ld backptr = %ld jnl_seqno = %llu "
				"[0x%llx]\n", bad_trans_type, rectype, rec_len, backptr, tmpseqno, tmpseqno);
			upd_proc_local->bad_trans = TRUE;
			/* We dont expect to be holding crit on any region in case of a bad_trans.
			 * Nevertheless release crit in PRO just in case we hold it.
			 */
			assert(0 == have_crit(CRIT_HAVE_ANY_REG));
			if (dollar_tlevel)
			{	/* Copy gv_currkey into temp variable before TROLLBACK overwrites the current variable. */
				gv_failed_key = (gv_key *)malloc(SIZEOF(gv_key) + gv_currkey->end);
				memcpy(gv_failed_key, gv_currkey, SIZEOF(gv_key) + gv_currkey->end);
				repl_log(updproc_log_fp, TRUE, TRUE,
					"OP_TROLLBACK IS CALLED -->Bad trans :: dollar_tlevel = %ld\n", dollar_tlevel);
				OP_TROLLBACK(0);	/* this should also release crit (if any) on all regions in TP */
				assert(!dollar_tlevel);
			} else
			{	/* Non-TP : Release crit if any */
				save_reg = gv_cur_region;
				if ((NULL != save_reg) && save_reg->open)
				{
					csa = (sgmnt_addrs *)&FILE_INFO(save_reg)->s_addrs;
					assert(NULL != csa);	/* since save_reg->open is TRUE */
					if (csa->now_crit)
						rel_crit(save_reg);
				}
			}
			assert(0 == have_crit(CRIT_HAVE_ANY_REG));
			readaddrs = recvpool.recvdata_base;
			upd_proc_local->read = 0;
			temp_read = 0;
			temp_write = 0;
			upd_rec_seqno = tupd_num = tcom_num = 0;
			/* KEY2BIG and REC2BIG are cases for which we need to make sure it is not a transmission hiccup before we
			 * go ahead and do the rts_error(GVSUBOFLOW) or rts_error(REC2BIG). That is the reason we give those two
			 * errors a second chance. Other errors are handled by either throwing an rts_error or asking for an
			 * unconditional re-transmission (as opposed to only two attempts for GVSUBOFLOW and REC2BIG). By asking for
			 * a re-transmission we increase our confidence level that this is a configuration issue (with smaller
			 * keysize on the secondary) and proceed with an rts_error if we see the same symptom. It is possible we
			 * might have two successive transmissions having the exact same corruption, but that is highly unlikely.
			 */
			if (last_errored_seqno == jnl_seqno)
			{
				last_errored_seqno = 0;
				switch(bad_trans_type)
				{
					case upd_bad_key_size: /* Not using ISSUE_GVSUBOFLOW_ERROR in order to free gv_failed_key */
						gv_failed_key_ptr = ((NULL == gv_failed_key) ? gv_currkey : gv_failed_key);
						/* Assert that input key to format_targ_key is double null terminated */
						assert(KEY_DELIMITER == gv_failed_key_ptr->base[gv_failed_key_ptr->end]);
						/* Note: might update "endBuff" */
						endBuff = format_targ_key(fmtBuff, ARRAYSIZE(fmtBuff), gv_failed_key_ptr, TRUE);
						GV_SET_LAST_SUBSCRIPT_INCOMPLETE(fmtBuff, endBuff);
						if (NULL != gv_failed_key)	/* Free memory if it has been used */
							free(gv_failed_key);
						assert(NULL != tmpcsa);
						rts_error_csa(CSA_ARG(tmpcsa) VARLSTCNT(6) ERR_GVSUBOFLOW, 0,
									ERR_GVIS, 2, endBuff - fmtBuff, fmtBuff);
						break;
					case upd_bad_val_size:
						if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ,
						    ((NULL == gv_failed_key) ? gv_currkey : gv_failed_key), TRUE)))
							end = &buff[MAX_ZWR_KEY_SZ - 1];
						if (NULL != gv_failed_key)	/* Free memory if it has been used */
							free(gv_failed_key);
						assert(NULL != tmpcsa);
						rts_error_csa(CSA_ARG(tmpcsa) VARLSTCNT(10) ERR_REC2BIG, 4,
							VMS_ONLY(gv_currkey->end + 1 + SIZEOF(rec_hdr) +) val_mv.str.len,
							(int4)gv_cur_region->max_rec_size, REG_LEN_STR(gv_cur_region),
							ERR_GVIS, 2, end - buff, buff);
						break;
				}
			} else
			{
				gv_currkey->base[0] = KEY_DELIMITER;
				last_errored_seqno = jnl_seqno;
				/* Free memory on the first unsuccessful attempt if gv_failed_key has been previously used */
				if (NULL != gv_failed_key)
					free(gv_failed_key);
			}
			continue;
		}
		if (incr_seqno)
		{
			if (jnl_seqno - lastlog_seqno >= log_interval)
			{
				repl_log(updproc_log_fp, TRUE, TRUE, "Rectype = %d Committed Jnl seq no is : "
					INT8_FMT" "INT8_FMTX"\n", rectype, INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
				lastlog_seqno = jnl_seqno;
			}
			upd_proc_local->read_jnl_seqno = ++jnl_seqno;
			/* Determine if all updates that we play from the receive pool do end up incrementing the jnl_seqno of the
			 * journal pool. In case of a root primary supplementary instance, we need to do this check only for the
			 * non-supplementary stream of interest (strm_index) that this update process is processing.
			 */
			UNIX_ONLY(
				assert(!suppl_root_primary || ((0 < strm_index) && (MAX_SUPPL_STRMS > strm_index)));
				jnlpool_ctl_seqno = (!suppl_root_primary ? jnlpool_ctl->jnl_seqno
										: jnlpool_ctl->strm_seqno[strm_index]);
			)
			VMS_ONLY(jnlpool_ctl_seqno = jnlpool_ctl->jnl_seqno;)
			if (jnlpool_ctl_seqno)
			{	/* Now that the update process has played an incoming seqno, we expect it to have incremented
				 * the corresponding jnl_seqno and strm_seqno fields in the current instance's journal pool
				 * as well. Not doing so will cause the source and receiver instances to go out of sync.
				 * We know of 4 ways in which this can occur and all of them have already been handled.
				 *	1) UPDREPLSTATEOFF error.
				 *	2) error_on_jnl_file_lost = JNL_FILE_LOST_ERRORS;
				 *	3) Duplicate KILL is journaled and increments seqno even though it does not touch db.
				 *	4) A concurrent online rollback on this instance
				 * First 3 cases are not expected in a typical update process. But, the 4th case is expected in a
				 * typical run. So, as long as the out-of-sync is due to the first 3 cases, stop right away and get
				 * a core dump for further analysis. In case of an online rollback, it is okay for us to continue
				 * to the next iteration which will eventually detect the online rollback (as part of commit or
				 * before that in the idle wait loop) and take appropriate action.
				 */
				if ((upd_proc_local->read_jnl_seqno != jnlpool_ctl_seqno)
					UNIX_ONLY(&& (repl_csa->onln_rlbk_cycle == jnlpool_ctl->onln_rlbk_cycle)))
				{
					repl_log(updproc_log_fp, TRUE, TRUE,
						"JNLSEQNO last updated by  update process = "INT8_FMT" "INT8_FMTX"\n",
						INT8_PRINT(upd_proc_local->read_jnl_seqno),
						INT8_PRINTX(upd_proc_local->read_jnl_seqno));
					repl_log(updproc_log_fp, TRUE, TRUE,
						"JNLSEQNO of last transaction written to journal pool = "INT8_FMT" "INT8_FMTX"\n",
						INT8_PRINT(jnlpool_ctl_seqno), INT8_PRINTX(jnlpool_ctl_seqno));
					GTMASSERT;
				}
			}
		}
		readaddrs += rec_len;
		temp_read += rec_len;
		if (0 == tupd_num)
			upd_proc_local->read = temp_read;
	}
	REVERT; /* of updproc_ch() */
	updproc_continue = FALSE;
}
