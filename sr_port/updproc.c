/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"
#include "gtm_time.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef VMS
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
#include "hashdef.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
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
#include "gtmio.h"
#endif

#ifdef VMS
#include <ssdef.h>
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
#include "util.h"
#include "op.h"
#include "gvcst_init.h"
#include "targ_alloc.h"
#include "dpgbldir.h"
#include "read_db_files_from_gld.h"
#include "updproc.h"
#include "upd_open_files.h"
#include "tp_change_reg.h"
#include "wcs_flu.h"
#include "repl_log.h"
#include "tp_restart.h"
#include "gtmmsg.h"	/* for gtm_putmsg() prototype */
#include "mu_gv_stack_init.h"
#include "jnl_typedef.h"

#define MINIMUM_BUFFER_SIZE	(DISK_BLOCK_SIZE * 32)

GBLREF	gd_binding		*gd_map;
GBLREF	gd_binding              *gd_map_top;
GBLREF  mur_opt_struct          mur_options;
GBLREF	bool			gv_curr_subsc_null;
GBLREF	unsigned short		dollar_tlevel;
GBLREF	gv_key			*gv_currkey;
GBLREF  gd_region               *gv_cur_region;
GBLREF  sgmnt_addrs             *cs_addrs;
GBLREF  sgmnt_data_ptr_t	cs_data;
GBLREF  int4			gv_keysize;
GBLREF	mur_opt_struct		mur_options;
GBLREF	bool			is_standalone;
GBLREF	recvpool_addrs		recvpool;
GBLREF	uint4			process_id;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl, temp_jnlpool_ctl;
GBLREF	boolean_t		is_updproc;
GBLREF	seq_num			seq_num_zero, seq_num_one;
GBLREF  gld_dbname_list		*upd_db_files;
GBLREF  gd_addr                 *gd_header;
GBLREF  boolean_t               repl_enabled;
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
LITREF	int			jrt_update[JRT_RECTYPES];
LITREF	boolean_t		jrt_fixed_size[JRT_RECTYPES];
LITREF	boolean_t		jrt_is_replicated[JRT_RECTYPES];

error_def(ERR_NORECOVERERR);

static sgmnt_data_ptr_t csd;
static seq_num		seqnum_diff;
static 	boolean_t	updproc_continue = TRUE;
static	seq_num		start_jnl_seqno;

void			mupip_update();

CONDITION_HANDLER(updproc_ch)
{
	unsigned char	seq_num_str[32], *seq_num_ptr;
	unsigned char	seq_num_strx[32], *seq_num_ptrx;

	error_def(ERR_TPRETRY);

	START_CH;
#ifdef UNIX
#if defined(DEBUG) && defined(DEBUG_UPDPROC_TPRETRY)
	gtm_fork_n_core();
#endif
#endif
	if ((int)ERR_TPRETRY == SIGNAL)
	{
#if defined(DEBUG) && defined(DEBUG_UPDPROC_TPRETRY)
		assert(FALSE);
#endif
		repl_log(updproc_log_fp, TRUE, TRUE, " ----> TPRETRY for sequence number "INT8_FMT" "INT8_FMTX" \n",
			INT8_PRINT(recvpool.upd_proc_local->read_jnl_seqno),
			INT8_PRINTX(recvpool.upd_proc_local->read_jnl_seqno));
		recvpool.upd_proc_local->bad_trans = TRUE;
		recvpool.upd_proc_local->read = 0;
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
	}
	NEXTCH;
}

/* updproc performs its main processing in a function call invoked within a loop.
 * Unless there is a TPRESTART, the processing remains in the updproc_actions loop,
 *   but when a resource conflict triggers a TPRESTART, the condition handler drops
 *   back to the outer loop in updproc, which reissues the call.
 * The reason for this structure is that VMS condition handling continues from where
 *   something left off, and, in this case, it returns control to the loop around
 *   the function call in the routine (updproc) that established the handler.
 * The longjmp mechanism used for UNIX condition handling could have been bent to work
 *   without the extra call, but that approach was abandoned to keep the code portable.
 */
int updproc(void)
{
	seq_num			jnl_seqno; /* the current jnl_seq no of the Update process */
	unsigned char		*mstack_ptr;
	uint4			status;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	unsigned char		seq_num_strx[32], *seq_num_ptrx;
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
	/* is_standalone = TRUE; */
	is_updproc = TRUE;
	is_replicator = TRUE;	/* as update process goes through t_end() and can write jnl recs to the jnlpool for replicated db */
	memset((uchar_ptr_t)&recvpool, 0, sizeof(recvpool)); /* For util_base_ch and mupip_exit */
	if (updproc_init() == UPDPROC_EXISTS) /* we got the global directory header already */
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Update Process already exists"));
	/* Initialization of all the relevant global datastructures and allocation for TP */
	mu_gv_stack_init(&mstack_ptr);
	recvpool.upd_proc_local->read = 0;
	QWASSIGN(seqnum_diff, seq_num_zero);
	QWASSIGN(jnl_seqno, start_jnl_seqno);
	QWASSIGN(recvpool.recvpool_ctl->jnl_seqno, jnl_seqno);
	QWASSIGN(recvpool.upd_proc_local->read_jnl_seqno, jnl_seqno);
	if (repl_enabled)
	{
		if (QWGT(jnlpool_ctl->jnl_seqno, start_jnl_seqno) && jnlpool_ctl->upd_disabled)
		{
			repl_log(updproc_log_fp, TRUE, TRUE,
				"JNLSEQNO last updated by  update process = "INT8_FMT" "INT8_FMTX" \n",
				INT8_PRINT(start_jnl_seqno), INT8_PRINTX(start_jnl_seqno));
			repl_log(updproc_log_fp, TRUE, TRUE,
				"JNLSEQNO of last transaction written to journal pool = "INT8_FMT" "INT8_FMTX" \n",
				INT8_PRINT(jnlpool_ctl->jnl_seqno), INT8_PRINTX(jnlpool_ctl->jnl_seqno));
			rts_error(VARLSTCNT(1) ERR_SECONDAHEAD);
		}
	}
	while (updproc_continue)
		updproc_actions();
	updproc_end();
	return(SS_NORMAL);
}

void updproc_actions(void)
{
	mval			ts_mv, key_mv, val_mv;
	mstr_len_t		*data_len;
	jnl_record		*rec;
	uint4			temp_write, temp_read;
	enum jnl_record_type	rectype;
	jnl_process_vector	*pv;
	int4			upd_rec_seqno = 0; /* the total no of journal reocrds excluding TCOM records */
	int4			tupd_num; /* the number of tset/tkill/tzkill records encountered */
	int4			tcom_num; /* the number of tcom records encountered */
	seq_num			jnl_seqno; /* the current jnl_seq no of the Update process */
	bool			is_valid_hist();
	struct stat		stat_buf;
	int			fd, n;
	int			rec_len, backptr;
	char			fn[MAX_FN_LEN];
	sm_uc_ptr_t		readaddrs;
	struct_jrec_null	null_record;
	jnldata_hdr_ptr_t	jnl_header;
	boolean_t		incr_seqno;
	seq_num			temp_df_seqnum;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	unsigned char		seq_num_strx[32], *seq_num_ptrx;
	uint4			jnl_status = 0;
	char			*val_ptr;
	jnl_string		*keystr;
	enum upd_bad_trans_type	bad_trans_type;

	error_def(ERR_TPRETRY);

	ESTABLISH(updproc_ch);
	temp_read = 0;
	temp_write = 0;
	readaddrs = recvpool.recvdata_base;
	upd_rec_seqno = tupd_num = tcom_num = 0;
	QWASSIGN(jnl_seqno, recvpool.upd_proc_local->read_jnl_seqno);
	while (TRUE)
	{
		incr_seqno = FALSE;
		if (repl_enabled)
		{
			QWSUB(temp_df_seqnum, jnlpool_ctl->jnl_seqno, recvpool.upd_proc_local->read_jnl_seqno);
			if (QWNE(seq_num_zero, temp_df_seqnum) && QWNE(seqnum_diff, temp_df_seqnum))
			{
				QWASSIGN(seqnum_diff, temp_df_seqnum);
				repl_log(updproc_log_fp, TRUE, TRUE,
					"JNLSEQNO last updated by  update process = "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(recvpool.upd_proc_local->read_jnl_seqno),
					INT8_PRINTX(recvpool.upd_proc_local->read_jnl_seqno));
				repl_log(updproc_log_fp, TRUE, TRUE,
					"JNLSEQNO of last transaction written to journal pool = "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(jnlpool_ctl->jnl_seqno), INT8_PRINTX(jnlpool_ctl->jnl_seqno));
				repl_log(updproc_log_fp, TRUE, TRUE, "Secondary Ahead of Primary by "INT8_FMT" "INT8_FMT" \n",
					INT8_PRINT(temp_df_seqnum), INT8_PRINTX(temp_df_seqnum));

			}
		}
		if (SHUTDOWN == recvpool.upd_proc_local->upd_proc_shutdown)
			break;
		if (GTMRECV_NO_RESTART != recvpool.gtmrecv_local->restart)
		{
			/* wait for restart to become GTMRECV_NO_RESTART (set by the Receiver Server) */
			QWASSIGN(recvpool.recvpool_ctl->jnl_seqno, jnl_seqno);
			readaddrs = recvpool.recvdata_base;
			recvpool.upd_proc_local->read = 0;
			recvpool.gtmrecv_local->restart = GTMRECV_UPD_RESTARTED;
			temp_read = 0;
			temp_write = 0;
			SHORT_SLEEP(10);
			continue;
		}
		if (recvpool.upd_proc_local->changelog)
		{
			updproc_log_init();
			recvpool.upd_proc_local->changelog = FALSE;
		}
		if (recvpool.upd_proc_local->bad_trans
			|| (FALSE == recvpool.recvpool_ctl->wrapped
				&& (temp_write = recvpool.recvpool_ctl->write) == recvpool.upd_proc_local->read))
		{
			/* to take care of the startup case where jnl_seqno is 0 in the recvpool_ctl */
			assert(QWEQ(seq_num_zero, recvpool.recvpool_ctl->jnl_seqno)
				||  QWLE(jnl_seqno, recvpool.recvpool_ctl->jnl_seqno));
			SHORT_SLEEP(10);
			continue;
		}
		/* To take the wrapping of buffer in case of over flow ------------ */
		/*     assume receiver will update wrapped even for exact overflows */
		if (temp_read >= recvpool.recvpool_ctl->write_wrap)
		{
			if (FALSE == recvpool.recvpool_ctl->wrapped)
			{
				/* Update process in keeping up with receiver
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
			recvpool.upd_proc_local->read = 0;
			recvpool.recvpool_ctl->wrapped = FALSE;
			readaddrs = recvpool.recvdata_base;
			if (0 == temp_write)
				continue; /* Receiver server wrapped but hasn't yet written anything into the pool */
		}
		rec = (jnl_record *)readaddrs;
		rectype = rec->prefix.jrec_type;
		rec_len = rec->prefix.forwptr;
		backptr = REC_LEN_FROM_SUFFIX(readaddrs, rec_len);
		assert(IS_REPLICATED(rectype));
		/* NOTE: All journal sequence number fields are at same offset */
		if (rec_len != backptr)
		{
			bad_trans_type = upd_bad_backptr;
			assert(FALSE);
		} else if (!IS_REPLICATED(rectype))
		{
			bad_trans_type = upd_rec_not_replicated;
			assert(FALSE);
		} else if (QWNE(jnl_seqno, rec->jrec_null.jnl_seqno))
		{
			bad_trans_type = upd_bad_jnl_seqno;
			assert(FALSE);
		} else
		{
			bad_trans_type = upd_good_record;
			assert(QWLT(jnl_seqno, recvpool.recvpool_ctl->jnl_seqno) ||
				QWEQ(seq_num_zero, recvpool.recvpool_ctl->jnl_seqno));
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
				key_mv.mvtype = MV_STR;
				key_mv.str.addr = keystr->text;
				key_mv.str.len = strlen(key_mv.str.addr);
				if (key_mv.str.len > MAX_KEY_SZ || keystr->length > MAX_KEY_SZ)
				{
					bad_trans_type = upd_bad_key_size;
					assert(FALSE);
				} else
				{
					if (IS_SET(rectype))
					{
						val_mv.mvtype = MV_STR;
						val_ptr = &keystr->text[keystr->length];
						GET_MSTR_LEN(val_mv.str.len, val_ptr);
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
				}
			}
		}
		if (upd_good_record == bad_trans_type)
		{
			if (JRT_NULL == rectype)
			{
				gv_cur_region = upd_db_files->gd;
				tp_change_reg();
				assert(!cs_addrs->now_crit);
				if (!cs_addrs->now_crit)
					grab_crit(gv_cur_region);
				grab_lock(jnlpool.jnlpool_dummy_reg);
				QWASSIGN(temp_jnlpool_ctl->write_addr, jnlpool_ctl->write_addr);
				QWASSIGN(temp_jnlpool_ctl->write, jnlpool_ctl->write);
				QWASSIGN(temp_jnlpool_ctl->jnl_seqno, jnlpool_ctl->jnl_seqno);
				assert(QWMODDW(temp_jnlpool_ctl->write_addr, temp_jnlpool_ctl->jnlpool_size) == temp_jnlpool_ctl->write);
				jgbl.cumul_jnl_rec_len = sizeof(jnldata_hdr_struct) + NULL_RECLEN;
				temp_jnlpool_ctl->write += sizeof(jnldata_hdr_struct);
				if (temp_jnlpool_ctl->write >= temp_jnlpool_ctl->jnlpool_size)
				{
					assert(temp_jnlpool_ctl->write == temp_jnlpool_ctl->jnlpool_size);
					temp_jnlpool_ctl->write = 0;
				}
				QWADDDW(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr, jgbl.cumul_jnl_rec_len);
				cs_addrs->ti->early_tn = cs_addrs->ti->curr_tn + 1;
				JNL_SHORT_TIME(jgbl.gbl_jrec_time);	/* needed for jnl_put_jrt_pini() */
				jnl_status = jnl_ensure_open();
				if (0 == jnl_status)
				{
					if (0 == cs_addrs->jnl->pini_addr)
						jnl_put_jrt_pini(cs_addrs);
					null_record.prefix.jrec_type = JRT_NULL;
					null_record.prefix.forwptr = null_record.suffix.backptr = NULL_RECLEN;
					null_record.prefix.time = jgbl.gbl_jrec_time;
					null_record.prefix.tn = cs_addrs->ti->curr_tn;
					null_record.prefix.pini_addr = cs_addrs->jnl->pini_addr;
					QWASSIGN(null_record.jnl_seqno, jnl_seqno);
					null_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
					jnl_write(cs_addrs->jnl, JRT_NULL, (jnl_record *)&null_record, NULL, NULL);
				} else
					rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
				QWINCRBY(temp_jnlpool_ctl->jnl_seqno, seq_num_one);
				QWASSIGN(cs_addrs->hdr->reg_seqno, temp_jnlpool_ctl->jnl_seqno);
				QWASSIGN(cs_addrs->hdr->resync_seqno, temp_jnlpool_ctl->jnl_seqno);
				cs_addrs->hdr->resync_tn = cs_addrs->ti->curr_tn;
				QWASSIGN(cs_addrs->hdr->old_resync_seqno, temp_jnlpool_ctl->jnl_seqno);
				/* the following statements should be atomic */
				jnl_header = (jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jnlpool_ctl->write);
				jnl_header->jnldata_len = temp_jnlpool_ctl->write - jnlpool_ctl->write +
					(temp_jnlpool_ctl->write > jnlpool_ctl->write ? 0 : jnlpool_ctl->jnlpool_size);
				jnl_header->prev_jnldata_len = jnlpool_ctl->lastwrite_len;
				jnlpool_ctl->lastwrite_len = jnl_header->jnldata_len;
				QWINCRBYDW(jnlpool_ctl->write_addr, jnl_header->jnldata_len);
				jnlpool_ctl->write = temp_jnlpool_ctl->write;
				jnlpool_ctl->jnl_seqno = temp_jnlpool_ctl->jnl_seqno;
				cs_addrs->ti->curr_tn = cs_addrs->ti->early_tn;
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
					{	/* op_tcommit restarted the transaction - do update process special handling for tpretry */
#ifdef VMS
			/* The error below has special handling in a few condition handlers because it not so much signals an error
			   as it does drive the necessary mechanisms to invoke a restart. Consequently this error can be
			   overridden by a "real" error. For VMS, the extra parameters are specified to provide "placeholders" on
			   the stack in the signal array if a real error needs to be overlayed in place of this one (see
			   code in updproc_ch).
			*/
						rts_error(VARLSTCNT(6) ERR_TPRETRY, 4, 0, 0, 0, 0);
#else
						rts_error(VARLSTCNT(1) ERR_TPRETRY);
#endif
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
				gv_bind_name(gd_header, &(key_mv.str));	/* this sets gv_target and gv_cur_region */
				key_mv.str.len = keystr->length;
				memcpy(gv_currkey->base, key_mv.str.addr, key_mv.str.len);
				gv_currkey->base[key_mv.str.len] = 0;
				gv_currkey->end = key_mv.str.len;
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
			}
		}
		if (upd_good_record != bad_trans_type)
		{
			if (IS_REPLICATED(rectype))
				repl_log(updproc_log_fp, TRUE, TRUE,
		 "-> Bad trans :: bad_trans_type = %ld type = %ld len = %ld backptr = %ld jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
				rectype, rec_len, backptr, bad_trans_type, INT8_PRINT(rec->jrec_null.jnl_seqno),
				INT8_PRINTX(rec->jrec_null.jnl_seqno));
			else
				repl_log(updproc_log_fp, TRUE, TRUE,
		 "-> Bad trans :: bad_trans_type = %ld type = %ld len = %ld backptr = %ld jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
					rectype, rec_len, backptr, bad_trans_type, INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
			recvpool.upd_proc_local->bad_trans = TRUE;
			if (0 < dollar_tlevel)
			{
				repl_log(updproc_log_fp, TRUE, TRUE,
					"OP_TROLLBACK IS CALLED -->Bad trans :: dollar_tlevel = %ld\n", dollar_tlevel);
				op_trollback(0);
			}
			readaddrs = recvpool.recvdata_base;
			recvpool.upd_proc_local->read = 0;
			temp_read = 0;
			temp_write = 0;
			upd_rec_seqno = tupd_num = tcom_num = 0;
			continue;
		}
		if (incr_seqno)
		{
			QWINCRBY(jnl_seqno, seq_num_one);
			QWASSIGN(recvpool.upd_proc_local->read_jnl_seqno, jnl_seqno);
			if (0 == QWMODDW(jnl_seqno, LOGTRNUM_INTERVAL))
				repl_log(updproc_log_fp, TRUE, TRUE,
					"Rectype = %d Committed Jnl seq no is : "INT8_FMT" "INT8_FMTX" \n",
					rectype, INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
		}
		readaddrs = readaddrs + rec_len;
		temp_read += rec_len;
		recvpool.upd_proc_local->read = temp_read;
	}
	updproc_continue = FALSE;
}

bool	upd_open_files(gld_dbname_list **upd_db_files)
{
	gld_dbname_list	*curr, *prev;
	sgmnt_addrs	*csa;
	char		*fn;
	sm_uc_ptr_t	gld_fn;
	int		i, db_fd;
	uint4		status;
	unsigned char	seq_num_str[32], *seq_num_ptr;
	unsigned char	seq_num_strx[32], *seq_num_ptrx;

	error_def(ERR_NOREPLCTDREG);

	QWASSIGN(start_jnl_seqno, seq_num_zero);
	QWASSIGN(jgbl.max_resync_seqno, seq_num_zero);
	/*
	 *	Open all of the database files
	 */
	/* Unix and VMS have different field names for now, but will both be soon changed to instname instead of gtmgbldir */
	UNIX_ONLY(gld_fn = (sm_uc_ptr_t)recvpool.recvpool_ctl->recvpool_id.instname;)
	VMS_ONLY(gld_fn = (sm_uc_ptr_t)recvpool.recvpool_ctl->recvpool_id.gtmgbldir;)
	for (curr = *upd_db_files, *upd_db_files = NULL;  NULL != curr;)
	{
		fn = (char *)curr->gd->dyn.addr->fname;
		csa = &FILE_INFO(curr->gd)->s_addrs;	/* Work of dbfilopn i.e. assigning file_cntl has been done already in read
								db_files_from_gld module */
		gvcst_init(curr->gd);
		csd = csa->hdr;
		if (((csa->hdr->max_key_size + MAX_NUM_SUBSC_LEN + 4) & (-4)) > gv_keysize)
			gv_keysize = (csa->hdr->max_key_size + MAX_NUM_SUBSC_LEN + 4) & (-4);
		csa->dir_tree = (gv_namehead *)targ_alloc(curr->gd->max_key_size);
		csa->dir_tree->root = DIR_ROOT;
		csa->dir_tree->gd_reg = curr->gd;
		csa->now_crit = FALSE;
		if (curr->gd->was_open)	 /* Should never happen as only open one at a time, but handle for safety */
		{
			assert(FALSE);
			util_out_print("Error opening database file !AZ", TRUE, fn);
			return FALSE;
		}
		repl_log(updproc_log_fp, TRUE, TRUE, " Opening File -- %s :: reg_seqno = "INT8_FMT" "INT8_FMTX" \n", fn,
			INT8_PRINT(csa->hdr->reg_seqno), INT8_PRINTX(csa->hdr->reg_seqno));
		/* The assignment of Seqno needs to be done before checking the state of replication since receiver
			server expects the update process to write Seqno in the recvpool before initiating
			communication with the source server */
		if (!(recvpool.upd_proc_local->updateresync))
		{
			if (QWLT(start_jnl_seqno, csa->hdr->resync_seqno))
				QWASSIGN(start_jnl_seqno, csa->hdr->resync_seqno);
		} else
		{
			QWASSIGN(csa->hdr->resync_seqno, csa->hdr->reg_seqno);
			if (QWLT(start_jnl_seqno, csa->hdr->reg_seqno))
				QWASSIGN(start_jnl_seqno, csa->hdr->reg_seqno);
		}
		/* jgbl.max_resync_seqno should be set only after the test for updateresync because resync_seqno gets modified
			to reg_seqno in case receiver is started with -updateresync option */
		if (QWLT(jgbl.max_resync_seqno, csa->hdr->resync_seqno))
			QWASSIGN(jgbl.max_resync_seqno, csa->hdr->resync_seqno);

		repl_log(updproc_log_fp, TRUE, TRUE, "             -------->  start_jnl_seqno = "INT8_FMT" "INT8_FMTX"\n",
			INT8_PRINT(start_jnl_seqno), INT8_PRINTX(start_jnl_seqno));
		if (!REPL_ENABLED(csd))
		{
			curr = curr->next;
			continue;
		} else if (!JNL_ENABLED(csd))
			GTMASSERT;
		else
			repl_enabled = TRUE;
		if (recvpool.upd_proc_local->updateresync)
		{
			TP_CHANGE_REG(curr->gd);
			wcs_flu(WCSFLU_FLUSH_HDR);
		}
		prev = curr;
		curr = curr->next;
		prev->next = *upd_db_files;
		*upd_db_files = prev;
	}
	if (NULL == *upd_db_files)
		gtm_putmsg(VARLSTCNT(3) ERR_NOREPLCTDREG, 1, gld_fn);
	return TRUE;
}
