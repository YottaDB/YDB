/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

GBLREF	unsigned short		dollar_tlevel;
GBLREF	gv_key			*gv_currkey;
GBLREF  gd_region               *gv_cur_region;
GBLREF  sgmnt_addrs             *cs_addrs;
GBLREF	recvpool_addrs		recvpool;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl, temp_jnlpool_ctl;
GBLREF	boolean_t		is_updproc;
GBLREF	seq_num			seq_num_zero, seq_num_one;
GBLREF  gd_addr                 *gd_header;
GBLREF  boolean_t               repl_allowed;
GBLREF	FILE	 		*updproc_log_fp;
GBLREF	void			(*call_on_signal)();
GBLREF	sgm_info		*first_sgm_info;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	struct_jrec_tcom	tcom_record;
#ifdef VMS
GBLREF	struct chf$signal_array	*tp_restart_fail_sig;
GBLREF	boolean_t		tp_restart_fail_sig_used;
#endif
GBLREF	boolean_t		is_replicator;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	boolean_t		secondary_side_std_null_coll;
GBLREF	boolean_t		disk_blk_read;
GBLREF	seq_num			lastlog_seqno;
GBLREF	uint4			log_interval;

LITREF	int			jrt_update[JRT_RECTYPES];
LITREF	boolean_t		jrt_fixed_size[JRT_RECTYPES];
LITREF	boolean_t		jrt_is_replicated[JRT_RECTYPES];
static	boolean_t		updproc_continue = TRUE;

CONDITION_HANDLER(updproc_ch)
{
	unsigned char	seq_num_str[32], *seq_num_ptr;
	unsigned char	seq_num_strx[32], *seq_num_ptrx;

	error_def(ERR_TPRETRY);
	error_def(UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY));

	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
#if defined(DEBUG) && defined(DEBUG_UPDPROC_TPRETRY)
		assert(FALSE);
#endif
		repl_log(updproc_log_fp, TRUE, TRUE, " ----> TPRETRY for sequence number "INT8_FMT" "INT8_FMTX" \n",
			INT8_PRINT(recvpool.upd_proc_local->read_jnl_seqno),
			INT8_PRINTX(recvpool.upd_proc_local->read_jnl_seqno));
		/* This is a kludge. We can come here from 2 places.
		 *	( i) From a call to t_retry which does a rts_error(ERR_TPRETRY).
		 *	(ii) From updproc_actions() where immediately after op_tcommit we detect that dollar_tlevel is non-zero.
		 * In the first case, we need to do a tp_restart. In the second, op_tcommit would have already done it for us.
		 * The way we detect the second case is from the value of first_sgm_info since it is NULLified in tp_restart.
		 */
		if (first_sgm_info)
		{
			VMS_ONLY(assert(FALSE == tp_restart_fail_sig_used);)
			tp_restart(1);
#ifdef UNIX
			if (ERR_TPRETRY == SIGNAL)		/* (signal value undisturbed) */
#elif defined VMS
			if (!tp_restart_fail_sig_used)		/* If tp_restart ran clean */
#else
#error unsupported platform
#endif
			{
				UNWIND(NULL, NULL);
			}
#ifdef VMS
			else
			{	/* Otherwise tp_restart had a signal that we must now deal with.
				 * replace the TPRETRY information with that saved from tp_restart.
				 * first assert that we have room for these arguments and proper setup
				 */
				assert(TPRESTART_ARG_CNT >= tp_restart_fail_sig->chf$is_sig_args);
				memcpy(sig, tp_restart_fail_sig, (tp_restart_fail_sig->chf$l_sig_args + 1) * sizeof(int));
				tp_restart_fail_sig_used = FALSE;
			}
#endif
		} else
		{
			UNWIND(NULL, NULL);
		}
	} else if ((int)(UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY)) == SIGNAL)
	{ /* Change the severity of memory-exhaust errors to fatal so we will have a process dump for debugging */
		SIGNAL = MAKE_MSG_SEVERE(SIGNAL);
	}
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
	unsigned char		*mstack_ptr;
	uint4			status;
	gld_dbname_list 	*gld_db_files;
#ifdef VMS
	char			proc_name[PROC_NAME_MAXLEN+1];
	struct dsc$descriptor_s proc_name_desc;
#endif

	error_def(ERR_TEXT);
	error_def(ERR_SECONDAHEAD);
	error_def(ERR_RECVPOOLSETUP);

	call_on_signal = updproc_sigstop;

#ifdef VMS
	/* Get a meaningful process name */
	proc_name_desc.dsc$w_length = get_proc_name(LIT_AND_LEN("GTMUPD"), getpid(), proc_name);
	proc_name_desc.dsc$a_pointer = proc_name;
	proc_name_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	proc_name_desc.dsc$b_class = DSC$K_CLASS_S;
	if (SS$_NORMAL != (status = sys$setprn(&proc_name_desc)))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to change update process name"), status);
#endif
	is_updproc = TRUE;
	is_replicator = TRUE;	/* as update process goes through t_end() and can write jnl recs to the jnlpool for replicated db */
	gvdupsetnoop = FALSE;	/* disable optimization to avoid multiple updates to the database and journal for duplicate sets */
	/* if duplicate SETs cause multiple updates to the database and journal in the primary, we want to do the same
	 * here in order to maintain the jnl_seqno in sync. note that the primary can run the VIEW "GVDUPSETNOOP" command
	 * to enable/disable this feature on a per-process basis so it is not under our control. if the primary does run with
	 * this feature enabled, then we will not be receiving journal records for the duplicate set anyways so running with
	 * this flag always set to FALSE does not hurt.
	 */
	memset((uchar_ptr_t)&recvpool, 0, sizeof(recvpool)); /* For util_base_ch and mupip_exit */
	if (updproc_init(&gld_db_files, &start_jnl_seqno) == UPDPROC_EXISTS) /* we got the global directory header already */
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Update Process already exists"));
	/* Initialization of all the relevant global datastructures and allocation for TP */
	mu_gv_stack_init(&mstack_ptr);
	recvpool.upd_proc_local->read = 0;
	recvpool.recvpool_ctl->std_null_coll = secondary_side_std_null_coll;
	jnl_seqno = start_jnl_seqno;
	UNIX_ONLY(
		recvpool.recvpool_ctl->max_dualsite_resync_seqno = jgbl.max_dualsite_resync_seqno;
	)
	recvpool.recvpool_ctl->jnl_seqno = jnl_seqno;
	recvpool.upd_proc_local->read_jnl_seqno = jnl_seqno;
	if (repl_allowed)
	{	/* Check if the secondary is ahead of the primary */
		VMS_ONLY(
			if (jnlpool_ctl->jnl_seqno > start_jnl_seqno && jnlpool_ctl->upd_disabled)
			{
				repl_log(updproc_log_fp, TRUE, TRUE,
					"JNLSEQNO last updated by  update process = "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(start_jnl_seqno), INT8_PRINTX(start_jnl_seqno));
				repl_log(updproc_log_fp, TRUE, TRUE,
					"JNLSEQNO of last transaction written to journal pool = "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(jnlpool_ctl->jnl_seqno), INT8_PRINTX(jnlpool_ctl->jnl_seqno));
				rts_error(VARLSTCNT(1) ERR_SECONDAHEAD);
			}
		)
		UNIX_ONLY(
			/* The SECONDAHEAD check is performed in the receiver server after it has connected with the source
			 * server. This is because the check is relevant only if the source server is dualsite. That is
			 * not known now but instead at connection time. Hence the deferred check.
			 */
			assert(jnlpool_ctl->jnl_seqno == start_jnl_seqno);
		)
	}
	while (updproc_continue)
		updproc_actions(gld_db_files);
	updproc_end();
	return(SS_NORMAL);
}

void updproc_actions(gld_dbname_list *gld_db_files)
{
	mval			ts_mv, val_mv;
	jnl_record		*rec;
	uint4			temp_write, temp_read;
	enum jnl_record_type	rectype;
	int4			upd_rec_seqno = 0; /* the total no of journal reocrds excluding TCOM records */
	int4			tupd_num; /* the number of tset/tkill/tzkill records encountered */
	int4			tcom_num; /* the number of tcom records encountered */
	seq_num			jnl_seqno, tmpseqno; /* the current jnl_seq no of the Update process */
	int			key_len, rec_len, backptr;
	char			fn[MAX_FN_LEN];
	sm_uc_ptr_t		readaddrs;	/* start of current rec in pool */
	struct_jrec_null	null_record;
	jnldata_hdr_ptr_t	jnl_header;
	boolean_t		incr_seqno;
	seq_num			temp_df_seqnum;
	uint4			jnl_status = 0;
	char			*val_ptr;
	jnl_string		*keystr;
	mstr			mname;
	enum upd_bad_trans_type	bad_trans_type;
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	char	           	gv_mname[MAX_KEY_SZ];
	static	seq_num		seqnum_diff = 0;

	UNIX_ONLY(
		repl_triple		triple;
		repl_triple_jnl_ptr_t	triplecontent;
	)

	error_def(ERR_TPRETRY);

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
	while (TRUE)
	{
		incr_seqno = FALSE;
		if (repl_allowed)
		{
			temp_df_seqnum =  jnlpool_ctl->jnl_seqno - upd_proc_local->read_jnl_seqno;
			if ((0 != temp_df_seqnum) && seqnum_diff != temp_df_seqnum)
			{
				seqnum_diff = temp_df_seqnum;
				repl_log(updproc_log_fp, TRUE, TRUE,
					"JNLSEQNO last updated by  update process = "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(upd_proc_local->read_jnl_seqno),
					INT8_PRINTX(upd_proc_local->read_jnl_seqno));
				repl_log(updproc_log_fp, TRUE, TRUE,
					"JNLSEQNO of last transaction written to journal pool = "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(jnlpool_ctl->jnl_seqno), INT8_PRINTX(jnlpool_ctl->jnl_seqno));
				repl_log(updproc_log_fp, TRUE, TRUE, "Secondary Ahead of Primary by "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(temp_df_seqnum), INT8_PRINTX(temp_df_seqnum));

			}
		}
		if (SHUTDOWN == upd_proc_local->upd_proc_shutdown)
			break;
		if (GTMRECV_NO_RESTART != gtmrecv_local->restart)
		{
			/* wait for restart to become GTMRECV_NO_RESTART (set by the Receiver Server) */
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
					assert(dollar_tlevel);
					op_trollback(0);
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
				upd_log_init(UPDPROC);
			upd_proc_local->changelog = 0;
		}
		if (upd_proc_local->bad_trans
			|| (0 == tupd_num && FALSE == recvpool.recvpool_ctl->wrapped
				&& (temp_write = recvpool.recvpool_ctl->write) == upd_proc_local->read))
		{
			/* to take care of the startup case where jnl_seqno is 0 in the recvpool_ctl */
			assert((0 == recvpool.recvpool_ctl->jnl_seqno)
				||  jnl_seqno <= recvpool.recvpool_ctl->jnl_seqno);
			SHORT_SLEEP(10);
			continue;
		}
		/* To take the wrapping of buffer in case of over flow ------------ */
		/*     assume receiver will update wrapped even for exact overflows */
		if (temp_read >= recvpool.recvpool_ctl->write_wrap)
		{
			if (0 < tupd_num)	/* receive pool cannot wrap in the middle of TP */
				GTMASSERT;	/* see process_tr_buff in gtmrecv_process for why */
			if (FALSE == recvpool.recvpool_ctl->wrapped)
			{ 	/* Update process in keeping up with receiver
				 * server, notices that there was a wrap
				 * (thru write and write_wrap). It has to
				 * wait till receiver sets wrapped */
				SHORT_SLEEP(1);
				continue;
			}
			DEBUG_ONLY(
				repl_log(updproc_log_fp, TRUE, FALSE,
				       "-- Wrapping -- read = %ld :: write_wrap = %ld :: upd_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
					temp_read, recvpool.recvpool_ctl->write_wrap, INT8_PRINT(jnl_seqno),INT8_PRINTX(jnl_seqno));
				repl_log(updproc_log_fp, TRUE, TRUE,
					"-------------> wrapped = %ld :: write = %ld :: recv_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
					recvpool.recvpool_ctl->wrapped, recvpool.recvpool_ctl->write,
					INT8_PRINT(recvpool.recvpool_ctl->jnl_seqno),
					INT8_PRINTX(recvpool.recvpool_ctl->jnl_seqno));
			)
			temp_read = 0;
			temp_write = recvpool.recvpool_ctl->write;
			upd_proc_local->read = 0;
			recvpool.recvpool_ctl->wrapped = FALSE;
			readaddrs = recvpool.recvdata_base;
			if (0 == temp_write)
				continue; /* Receiver server wrapped but hasn't yet written anything into the pool */
		}
		rec = (jnl_record *)readaddrs;
		rectype = rec->prefix.jrec_type;
		rec_len = rec->prefix.forwptr;
		assert(IS_REPLICATED(rectype));
		UNIX_ONLY(
			if (JRT_TRIPLE == rectype)
			{	/* Source server has sent a REPL_NEW_TRIPLE message in the middle of logical journal records.
				 * Construct the triple from the input message and add it to the replication instance file.
				 */
				repl_log(updproc_log_fp, TRUE, TRUE, "Processing REPL_NEW_TRIPLE message\n");
				triplecontent = (repl_triple_jnl_ptr_t)readaddrs;
				memset(&triple, 0, sizeof(triple));
				triple.start_seqno = triplecontent->start_seqno;
				memcpy(triple.root_primary_instname, triplecontent->instname, MAX_INSTNAME_LEN - 1);
				triple.root_primary_cycle = triplecontent->cycle;
				memcpy(triple.rcvd_from_instname, triplecontent->rcvd_from_instname, MAX_INSTNAME_LEN - 1);
				repl_log(updproc_log_fp, TRUE, TRUE, "New Triple Content : Start Seqno = %llu [0x%llx] : "
					"Root Primary = [%s] : Cycle = [%d] : Received from instance = [%s]\n",
					triple.start_seqno, triple.start_seqno, triple.root_primary_instname,
					triple.root_primary_cycle, triple.rcvd_from_instname);
				if (sizeof(repl_triple_jnl_t) != rec_len)
				{
					bad_trans_type = upd_bad_triple_len;
					assert(FALSE);
				} else if (triple.start_seqno != recvpool.upd_proc_local->read_jnl_seqno)
				{
					bad_trans_type = upd_bad_triple_start_seqno1;
					assert(FALSE);
				} else if (triple.start_seqno > recvpool_ctl->jnl_seqno)
				{
					bad_trans_type = upd_bad_triple_start_seqno2;
					assert(FALSE);
				} else
					bad_trans_type = upd_good_record;
				if (upd_good_record != bad_trans_type)
				{	/* Signal a BADTRANS */
					repl_log(updproc_log_fp, TRUE, TRUE,
						"-> Bad trans :: bad_trans_type = %ld type = %ld len = %ld "
						"start_seqno = %llu [0x%llx] upd_read_seqno = %llu [0x%llx] "
						"recvpool_jnl_seqno = %llu [0x%llx]\n",
						bad_trans_type, rectype, rec_len, triple.start_seqno, triple.start_seqno,
						recvpool.upd_proc_local->read_jnl_seqno, recvpool.upd_proc_local->read_jnl_seqno,
						recvpool_ctl->jnl_seqno, recvpool_ctl->jnl_seqno);
					upd_proc_local->bad_trans = TRUE;
					assert(0 == dollar_tlevel);
					if (0 < dollar_tlevel)
					{
						repl_log(updproc_log_fp, TRUE, TRUE, "OP_TROLLBACK IS CALLED "
							"-->Bad trans :: dollar_tlevel = %ld\n", dollar_tlevel);
						op_trollback(0);
					}
					readaddrs = recvpool.recvdata_base;
					upd_proc_local->read = 0;
					temp_read = 0;
					temp_write = 0;
					upd_rec_seqno = tupd_num = tcom_num = 0;
					continue;
				}
				/* Now that we have constructed the triple, add it to the instance file. */
				repl_inst_ftok_sem_lock();
				repl_inst_triple_add(&triple);
				repl_inst_ftok_sem_release();
				/* Update pointers to indicate this record is now processed and move on to the next. */
				readaddrs += rec_len;
				temp_read += rec_len;
				upd_proc_local->read = temp_read;
				continue;
			}
		)
		/* NOTE: All journal sequence number fields are at same offset */
		if (ROUND_DOWN2(rec_len, JNL_REC_START_BNDRY) != rec_len)
		{	/* We need that so REC_LEN_FROM_SUFFIX does not access unaligned int */
			bad_trans_type = upd_bad_forwptr;
			assert(FALSE);
		} else if (rec_len != (backptr = REC_LEN_FROM_SUFFIX(readaddrs, rec_len)))
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
			} else if (IS_SET_KILL_ZKILL(rectype))
			{
				if (IS_ZTP(rectype))
					keystr = (jnl_string *)&rec->jrec_fset.mumps_node;
				else
					keystr = (jnl_string *)&rec->jrec_set.mumps_node;
				key_len = keystr->length;	/* local copy of shared recvpool key */
				if ((MAX_KEY_SZ >= key_len && 0 <= key_len && 0 == keystr->text[key_len - 1]) &&
					(upd_good_record == updproc_get_gblname(keystr->text, key_len, gv_mname, &mname)))
				{
					if (IS_SET(rectype))
					{
						val_mv.mvtype = MV_STR;
						val_ptr = &keystr->text[keystr->length];
						GET_MSTR_LEN(val_mv.str.len, val_ptr); /* length of value validated later */
						val_mv.str.addr = val_ptr + sizeof(mstr_len_t);
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
								assert((!dollar_tlevel && !tupd_num)
									|| dollar_tlevel && (tupd_num || t_tries ||
										cdb_sc_helpedout == t_fail_hist[t_tries]));
								if (!dollar_tlevel)
									op_tstart(TRUE, TRUE, &ts_mv, 0); /* not equal to -1
														==> RESTARTABLE */
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
					assert(FALSE);
			}
		}
		if (upd_good_record == bad_trans_type)
		{
			if (JRT_NULL == rectype)
			{
				gv_cur_region = gld_db_files->gd;
				tp_change_reg();
				csa = cs_addrs;
				assert(!csa->now_crit);
				if (!csa->now_crit)
					grab_crit(gv_cur_region);
				grab_lock(jnlpool.jnlpool_dummy_reg);
				temp_jnlpool_ctl->write_addr = jnlpool_ctl->write_addr;
				temp_jnlpool_ctl->write = jnlpool_ctl->write;
				temp_jnlpool_ctl->jnl_seqno = jnlpool_ctl->jnl_seqno;
				assert((temp_jnlpool_ctl->write_addr % temp_jnlpool_ctl->jnlpool_size) == temp_jnlpool_ctl->write);
				jgbl.cumul_jnl_rec_len = sizeof(jnldata_hdr_struct) + NULL_RECLEN;
				temp_jnlpool_ctl->write += sizeof(jnldata_hdr_struct);
				if (temp_jnlpool_ctl->write >= temp_jnlpool_ctl->jnlpool_size)
				{
					assert(temp_jnlpool_ctl->write == temp_jnlpool_ctl->jnlpool_size);
					temp_jnlpool_ctl->write = 0;
				}
				jnlpool_ctl->early_write_addr = jnlpool_ctl->write_addr +  jgbl.cumul_jnl_rec_len;
				/* Source server does not read in crit. It relies on early_write_addr, the transaction
				 * data, lastwrite_len, write_addr being updated in that order. To ensure this order,
				 * we have to force out early_write_addr to its coherency point now. If not, the source
				 * server may read data that is overwritten (or stale). This is true only on
				 * architectures and OSes that allow unordered memory access
				 */
				SHM_WRITE_MEMORY_BARRIER;
				csa->ti->early_tn = csa->ti->curr_tn + 1;
				JNL_SHORT_TIME(jgbl.gbl_jrec_time);	/* needed for jnl_put_jrt_pini() */
				jnl_status = jnl_ensure_open();
				if (0 == jnl_status)
				{
					if (0 == csa->jnl->pini_addr)
						jnl_put_jrt_pini(csa);
					null_record.prefix.jrec_type = JRT_NULL;
					null_record.prefix.forwptr = null_record.suffix.backptr = NULL_RECLEN;
					null_record.prefix.time = jgbl.gbl_jrec_time;
					null_record.prefix.tn = csa->ti->curr_tn;
					null_record.prefix.pini_addr = csa->jnl->pini_addr;
					null_record.prefix.checksum = INIT_CHECKSUM_SEED;
					null_record.jnl_seqno = jnl_seqno;
					null_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
					jnl_write(csa->jnl, JRT_NULL, (jnl_record *)&null_record, NULL, NULL);
				} else
					rts_error(VARLSTCNT(6) jnl_status, 4,
						JNL_LEN_STR(csa->hdr), DB_LEN_STR(gv_cur_region));
				temp_jnlpool_ctl->jnl_seqno++;
				csa->hdr->reg_seqno = temp_jnlpool_ctl->jnl_seqno;
				VMS_ONLY(
					csa->hdr->resync_seqno = temp_jnlpool_ctl->jnl_seqno;
					csa->hdr->resync_tn = csa->ti->curr_tn;
					csa->hdr->old_resync_seqno = temp_jnlpool_ctl->jnl_seqno;
				)
				UNIX_ONLY(
					assert(REPL_PROTO_VER_UNINITIALIZED != gtmrecv_local->last_valid_remote_proto_ver);
					if (REPL_PROTO_VER_DUALSITE == gtmrecv_local->last_valid_remote_proto_ver)
						csa->hdr->dualsite_resync_seqno = temp_jnlpool_ctl->jnl_seqno;
				)
				/* the following statements should be atomic */
				jnl_header = (jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jnlpool_ctl->write);
				jnl_header->jnldata_len = temp_jnlpool_ctl->write - jnlpool_ctl->write +
					(temp_jnlpool_ctl->write > jnlpool_ctl->write ? 0 : jnlpool_ctl->jnlpool_size);
				jnl_header->prev_jnldata_len = jnlpool_ctl->lastwrite_len;
				jnlpool_ctl->lastwrite_len = jnl_header->jnldata_len;
				/* For systems with UNORDERED memory access (example, ALPHA, POWER4, PA-RISC 2.0), on a multi
				 * processor system, it is possible that the source server notices the change in write_addr
				 * before seeing the change to jnlheader->jnldata_len, leading it to read an invalid
				 * transaction length. To avoid such conditions, we should commit the order of shared
				 * memory updates before we update write_addr. This ensures that the source server sees all
				 * shared memory updates related to a transaction before the change in write_addr
				 */
				SHM_WRITE_MEMORY_BARRIER;
				jnlpool_ctl->write_addr += jnl_header->jnldata_len;
				jnlpool_ctl->write = temp_jnlpool_ctl->write;
				jnlpool_ctl->jnl_seqno = temp_jnlpool_ctl->jnl_seqno;
				INCREMENT_CURR_TN(csa->hdr);
				rel_lock(jnlpool.jnlpool_dummy_reg);
				rel_crit(gv_cur_region);
				jgbl.cumul_jnl_rec_len = 0;
				incr_seqno = TRUE;
			} else if (JRT_TCOM == rectype)
			{
				if (tcom_num == tupd_num)
				{
					assert(0 != tcom_num);
					memcpy(tcom_record.jnl_tid, rec->jrec_tcom.jnl_tid, TID_STR_SIZE);
					op_tcommit();
					if (0 != dollar_tlevel)
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
					tcom_num = tupd_num = upd_rec_seqno = 0;
					incr_seqno = TRUE;
				}
			} else if (JRT_ZTCOM == rectype)
			{
				assert(0 != dollar_tlevel);
				op_ztcommit(1);
				incr_seqno = TRUE;
			}
			else if (IS_SET_KILL_ZKILL(rectype))
			{
				gv_bind_name(gd_header, &mname);	/* this sets gv_target and gv_cur_region */
				memcpy(gv_currkey->base, keystr->text, keystr->length);
				gv_currkey->base[keystr->length] = 0; 	/* second null of a key terminator */
				gv_currkey->end = keystr->length;
				if (IS_KILL(rectype))
					op_gvkill();
				else if (IS_ZKILL(rectype))
					op_gvzwithdraw();
				else
				{
					assert(IS_SET(rectype));
					if (keystr->length + 1 + val_mv.str.len + sizeof(rec_hdr) > gv_cur_region->max_rec_size)
					{
						bad_trans_type = upd_bad_val_size;
						assert(FALSE);
					} else
						op_gvput(&val_mv);
				}
				if ((upd_good_record == bad_trans_type) && !IS_TP(rectype))
					incr_seqno = TRUE;
				csa = &FILE_INFO(gv_cur_region)->s_addrs;
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
		}
		if (upd_good_record != bad_trans_type)
		{
			tmpseqno = IS_REPLICATED(rectype) ? rec->jrec_null.jnl_seqno : jnl_seqno;
			repl_log(updproc_log_fp, TRUE, TRUE,
				"-> Bad trans :: bad_trans_type = %ld type = %ld len = %ld backptr = %ld jnl_seqno = %llu "
				"[0x%llx]\n", bad_trans_type, rectype, rec_len, backptr, tmpseqno, tmpseqno);
			upd_proc_local->bad_trans = TRUE;
			if (0 < dollar_tlevel)
			{
				repl_log(updproc_log_fp, TRUE, TRUE,
					"OP_TROLLBACK IS CALLED -->Bad trans :: dollar_tlevel = %ld\n", dollar_tlevel);
				op_trollback(0);
			}
			readaddrs = recvpool.recvdata_base;
			upd_proc_local->read = 0;
			temp_read = 0;
			temp_write = 0;
			upd_rec_seqno = tupd_num = tcom_num = 0;
			continue;
		}
		if (incr_seqno)
		{
			if (jnl_seqno - lastlog_seqno >= log_interval)
			{
				repl_log(updproc_log_fp, TRUE, TRUE, "Rectype = %d Committed Jnl seq no is : "
					INT8_FMT" "INT8_FMTX" \n", rectype, INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
				lastlog_seqno = jnl_seqno;
			}
			upd_proc_local->read_jnl_seqno = ++jnl_seqno;
		}
		readaddrs += rec_len;
		temp_read += rec_len;
		if (0 == tupd_num)
			upd_proc_local->read = temp_read;
	}
	REVERT; /* of updproc_ch() */
	updproc_continue = FALSE;
}
