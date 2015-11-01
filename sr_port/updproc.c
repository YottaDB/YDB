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
#include "tp.h"
#include "muprec.h"
#include "iosp.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "gtmrecv.h"
#include "cli.h"
#include "error.h"
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
#include "updproc.h"
#include "tp_change_reg.h"
#include "wcs_flu.h"
#include "repl_log.h"
#include "tp_restart.h"

#define USER_STACK_SIZE		16384			/* (16 * 1024) */
#define MINIMUM_BUFFER_SIZE	(DISK_BLOCK_SIZE * 32)

LITREF  int			jnl_fixed_size[];

GBLREF	gd_binding		*gd_map;
GBLREF	gd_binding              *gd_map_top;
GBLREF  mur_opt_struct          mur_options;
GBLREF  bool                    mur_error_allowed;
GBLREF  int4                    mur_error_count;
GBLREF	bool			gv_curr_subsc_null;
GBLREF	unsigned short		dollar_tlevel;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF  gd_region               *gv_cur_region;
GBLREF  sgmnt_addrs             *cs_addrs;
GBLREF  sgmnt_data_ptr_t	cs_data;
GBLREF  int4			gv_keysize;
GBLREF  gv_key                  *gv_altkey;
GBLREF  mv_stent                *mv_chain;
GBLREF  stack_frame             *frame_pointer;
GBLREF  unsigned char           *msp, *stackbase, *stacktop, *stackwarn;
GBLREF	mur_opt_struct		mur_options;
GBLREF	bool			is_standalone;
GBLREF	recvpool_addrs		recvpool;
GBLREF	seq_num			start_jnl_seqno;
GBLREF	bool			is_db_updater;
GBLREF	uint4			process_id;
GBLREF	boolean_t		tstarted;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl, temp_jnlpool_ctl;
GBLREF	uint4			cumul_jnl_rec_len;
GBLREF	boolean_t		is_updproc;
GBLREF	seq_num			seq_num_zero, seq_num_one;
GBLREF  upd_proc_ctl            *upd_db_files;
GBLREF  gd_addr                 *gd_header;
GBLREF  boolean_t               repl_enabled;
GBLREF  seq_num			max_resync_seqno;
GBLREF	FILE	 		*updproc_log_fp;
GBLREF	void			(*call_on_signal)();
GBLREF	sgm_info		*first_sgm_info;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#ifdef VMS
GBLREF	struct chf$signal_array	*tp_restart_fail_sig;
GBLREF	boolean_t	tp_restart_fail_sig_used;
#endif

error_def(ERR_NORECOVERERR);

static sgmnt_data_ptr_t csd;
static seq_num		seqnum_diff;
static 	boolean_t	updproc_continue = TRUE;

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
#ifdef VMS
			/* Prep structures for potential use by tp_restart's condition handler */
			if (!tp_restart_fail_sig)
				tp_restart_fail_sig = (struct chf$signal_array *)malloc((TPRESTART_ARG_CNT + 1) * sizeof(int));
			assert(FALSE == tp_restart_fail_sig_used);
#endif
			tp_restart(1);
#ifdef VMS
			if (!tp_restart_fail_sig_used)	/* If tp_restart ran clean */
#else
			if (ERR_TPRETRY == SIGNAL)      /* (signal value undisturbed) */
#endif
			{
				UNWIND(NULL, NULL);
			}
#ifdef VMS
			else
			{	/* Otherwise tp_restart had a signal that we must now deal with.
				 * replace the TPRETRY information with that saved from tp_restart.
				 */
				assert(TPRESTART_ARG_CNT >= tp_restart_fail_sig->chf$is_sig_args);
						/* Assert we have room for these arguments */
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
	is_db_updater = TRUE;
	memset((uchar_ptr_t)&recvpool, 0, sizeof(recvpool)); /* For util_base_ch and mupip_exit */
	if (updproc_init() == UPDPROC_EXISTS) /* we got the global directory header already */
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Update Process already exists"));
	/* Initialization of all the relevant global datastructures and allocation for TP */
	gv_currkey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
	gv_altkey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
	gv_currkey->top = gv_altkey->top = gv_keysize;
	gv_currkey->end = gv_currkey->prev = gv_altkey->end = gv_altkey->prev = 0;
	gv_altkey->base[0] = gv_currkey->base[0] = '\0';
	mstack_ptr = (unsigned char *)malloc(USER_STACK_SIZE);
	msp = stackbase = mstack_ptr + USER_STACK_SIZE - 4;
	mv_chain = (mv_stent *)msp;
	stacktop = mstack_ptr + 2 * mvs_size[MVST_NTAB];
	stackwarn = stacktop + 1024;
	msp -= sizeof(stack_frame);
	frame_pointer = (stack_frame *)msp;
	memset(frame_pointer, 0, sizeof(stack_frame));
	frame_pointer->type = SFT_COUNT;
	--frame_pointer;
	memset(frame_pointer, 0, sizeof(stack_frame));
	frame_pointer->type = SFT_COUNT;
	frame_pointer->old_frame_pointer = (stack_frame *)msp;
	msp = (unsigned char *)frame_pointer;
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
	mval			v;
	mstr_len_t		*data_len;
	jnl_record		*rec;
	uint4			temp_write, temp_read, jnl_status;
	enum jnl_record_type	rectype;
	jnl_process_vector	*pv;
	int4			rec_seqno = 0; /* the total no of journal reocrds excluding TCOM records */
	int4			tset_num = 0; /* the number of tset/tkill records encountered */
	int4			tcom_num = 0; /* the number of tcom records encountered */
	seq_num			jnl_seqno; /* the current jnl_seq no of the Update process */
	bool			is_valid_hist();
	struct stat		stat_buf;
	int			fd, n;
	int			m, rec_len;
	char			fn[MAX_FN_LEN];
	sm_uc_ptr_t		readaddrs;
	struct_jrec_null	null_record;
	jnldata_hdr_ptr_t	jnl_header;
	boolean_t		bad_trans;
	seq_num			temp_df_seqnum;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	unsigned char		seq_num_strx[32], *seq_num_ptrx;

	error_def(ERR_TPRETRY);

	ESTABLISH(updproc_ch);
	temp_read = 0;
	temp_write = 0;
	readaddrs = recvpool.recvdata_base;
	rec_seqno = tset_num = tcom_num = 0;
	QWASSIGN(jnl_seqno, recvpool.upd_proc_local->read_jnl_seqno);
	while (1)
	{
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
			repl_log(updproc_log_fp, TRUE, TRUE,
				"-- Wrapping -- read = %ld :: write_wrap = %ld :: upd_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
				temp_read, recvpool.recvpool_ctl->write_wrap, INT8_PRINT(jnl_seqno),INT8_PRINTX(jnl_seqno));
			repl_log(updproc_log_fp, TRUE, TRUE,
				"-------------> wrapped = %ld :: write = %ld :: recv_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
				recvpool.recvpool_ctl->wrapped, recvpool.recvpool_ctl->write,
				INT8_PRINT(recvpool.recvpool_ctl->jnl_seqno), INT8_PRINTX(recvpool.recvpool_ctl->jnl_seqno));
			temp_read = 0;
			temp_write = recvpool.recvpool_ctl->write;
			recvpool.upd_proc_local->read = 0;
			recvpool.recvpool_ctl->wrapped = FALSE;
			readaddrs = recvpool.recvdata_base;
			if (0 == temp_write)
				continue; /* Receiver server wrapped but hasn't yet written anything into the pool */
		}
		m = (temp_write > temp_read ? temp_write : recvpool.recvpool_ctl->write_wrap) - temp_read;
		bad_trans = FALSE;
		if (-1 == (rec_len = jnl_record_length((jnl_record *)readaddrs, m)))
		{
			repl_log(updproc_log_fp, TRUE, TRUE, "-> Bad trans :: Invalid rec_len = %ld\n ", rec_len);
			assert(FALSE);
			bad_trans = recvpool.upd_proc_local->bad_trans = TRUE;
		}
		if (!bad_trans)
		{
			assert(QWLT(jnl_seqno, recvpool.recvpool_ctl->jnl_seqno)
				|| QWEQ(seq_num_zero, recvpool.recvpool_ctl->jnl_seqno));
			rec = (jnl_record *)readaddrs;
			rectype = REF_CHAR(&rec->jrec_type);
			switch (REF_CHAR(&rec->jrec_type))
			{
			case JRT_TCOM:
				assert(QWEQ(jnl_seqno, rec->val.jrec_tset.jnl_seqno));
				if (QWNE(jnl_seqno, rec->val.jrec_tset.jnl_seqno))
				{
					bad_trans = recvpool.upd_proc_local->bad_trans = TRUE;
					repl_log(updproc_log_fp, TRUE, TRUE,
					 "-> Bad trans found  for record JRT_TCOM :: tset_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
				 	 INT8_PRINT(rec->val.jrec_tset.jnl_seqno),
					 INT8_PRINTX(rec->val.jrec_tset.jnl_seqno));
					repl_log(updproc_log_fp, TRUE, TRUE,
					 "-> Bad trans found for record JRT_TCOM:: jnl_seqno = "INT8_FMT" "INT8_FMTX" n",
					 INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
				} else if (0 != rec_seqno)
					tcom_num++;
				break;
			case JRT_TSET:
			case JRT_TKILL:
			case JRT_TZKILL:
				assert(&rec->val.jrec_tset.jnl_seqno == &rec->val.jrec_tkill.jnl_seqno);
				assert(&rec->val.jrec_tkill.jnl_seqno == &rec->val.jrec_tzkill.jnl_seqno);
				assert(QWEQ(jnl_seqno, rec->val.jrec_tset.jnl_seqno));
				assert(0 <= tset_num);
				assert(0 == tcom_num);
				if (QWNE(jnl_seqno, rec->val.jrec_tset.jnl_seqno) ||
							0 > tset_num  ||  0 != tcom_num)
				{
					if (0 > tset_num)
						repl_log(updproc_log_fp, TRUE, TRUE, "-> Bad trans at tset_num :: = %ld\n ",
							tset_num);
					if (0 != tcom_num)
						repl_log(updproc_log_fp, TRUE, TRUE, "-> Bad trans at tcom_num :: = %ld\n ",
							tcom_num);
					repl_log(updproc_log_fp, TRUE, TRUE,
					 "-> Bad trans found for record JRT_TSET:: tset_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
					 INT8_PRINT(rec->val.jrec_tset.jnl_seqno), INT8_PRINTX(rec->val.jrec_tset.jnl_seqno));
					repl_log(updproc_log_fp, TRUE, TRUE,
					 "-> Bad trans found for record JRT_TSET:: jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
					 INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
					bad_trans = recvpool.upd_proc_local->bad_trans = TRUE;
					break;
				}
				v.mvtype = MV_STR;
				v.str.len = 0;
				v.str.addr = NULL;
				assert((!dollar_tlevel && !tset_num)
					|| dollar_tlevel && (tset_num || t_tries || cdb_sc_helpedout == t_fail_hist[t_tries]));
				if (!dollar_tlevel)
					op_tstart(TRUE, TRUE, &v, 0);   /* not equal to -1 ==> RESTARTABLE */
				tset_num++;
				rec_seqno++;
				break;
			case JRT_USET:
			case JRT_UKILL:
			case JRT_UZKILL:
				assert(QWEQ(jnl_seqno, rec->val.jrec_tset.jnl_seqno));
				assert(0 < tset_num);
				assert(0 == tcom_num);
				if (QWNE(jnl_seqno, rec->val.jrec_tset.jnl_seqno)
					|| 0 >= tset_num  ||  0 != tcom_num)
				{
					if (0 >= tset_num)
						repl_log(updproc_log_fp, TRUE, TRUE, "-> Bad trans at tset_num :: = %ld\n ",
							tset_num);
					if (0 != tcom_num)
						repl_log(updproc_log_fp, TRUE, TRUE, "-> Bad trans at tcom_num :: = %ld\n ",
							tcom_num);
					repl_log(updproc_log_fp, TRUE, TRUE,
					 "-> Bad trans found for record JRT_USET:: tset_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
					 INT8_PRINT(rec->val.jrec_tset.jnl_seqno), INT8_PRINTX(rec->val.jrec_tset.jnl_seqno));
					repl_log(updproc_log_fp, TRUE, TRUE,
						"-> Bad trans found for record JRT_USET:: jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
						INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
					bad_trans = recvpool.upd_proc_local->bad_trans = TRUE;
					break;
				}
				rec_seqno++;
				break;
			case JRT_SET:
			case JRT_KILL:
			case JRT_ZKILL:
				assert(QWEQ(jnl_seqno, rec->val.jrec_set.jnl_seqno));
				if (QWNE(jnl_seqno, rec->val.jrec_tset.jnl_seqno)  ||  0 != tset_num)
				{
					if (0 != tset_num)
						repl_log(updproc_log_fp, TRUE, TRUE, "-> Bad trans at tset_num :: = %ld\n ",
							tset_num);
					repl_log(updproc_log_fp, TRUE, TRUE,
					 "-> Bad trans found for record JRT_SET:: set_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
						INT8_PRINT(rec->val.jrec_set.jnl_seqno), INT8_PRINTX(rec->val.jrec_set.jnl_seqno));
					repl_log(updproc_log_fp, TRUE, TRUE,
						"-> Bad trans found for record JRT_SET:: jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
						INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
					bad_trans = recvpool.upd_proc_local->bad_trans = TRUE;
				}
				break;
			case JRT_NULL:
				assert(QWEQ(jnl_seqno, rec->val.jrec_null.jnl_seqno));
				if (QWNE(jnl_seqno, rec->val.jrec_null.jnl_seqno))
				{
					repl_log(updproc_log_fp, TRUE, TRUE,
					 "-> Bad trans found for record JRT_NULL:: null_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
					 INT8_PRINT(rec->val.jrec_null.jnl_seqno), INT8_PRINTX(rec->val.jrec_null.jnl_seqno));
					repl_log(updproc_log_fp, TRUE, TRUE,
					 "-> Bad trans found for record JRT_SET:: null_jnl_seqno = "INT8_FMT" "INT8_FMTX" \n",
						INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
					bad_trans = recvpool.upd_proc_local->bad_trans = TRUE;
				}
				break;
			default:
				assert(FALSE);
				bad_trans = recvpool.upd_proc_local->bad_trans = TRUE;
				break;
			}
		}
		if (bad_trans)
		{
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
			rec_seqno = tset_num = tcom_num = 0;
			continue;
		}
		switch (REF_CHAR(&rec->jrec_type))
		{
		case JRT_NULL:
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
			cumul_jnl_rec_len = sizeof(jnldata_hdr_struct)
					+ JREC_PREFIX_SIZE + jnl_fixed_size[JRT_NULL] + JREC_SUFFIX_SIZE;
			temp_jnlpool_ctl->write += sizeof(jnldata_hdr_struct);
			if (temp_jnlpool_ctl->write >= temp_jnlpool_ctl->jnlpool_size)
			{
				assert(temp_jnlpool_ctl->write == temp_jnlpool_ctl->jnlpool_size);
				temp_jnlpool_ctl->write = 0;
			}
			QWADDDW(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr, cumul_jnl_rec_len);
			cs_addrs->ti->early_tn = cs_addrs->ti->curr_tn + 1;
			null_record.tn = cs_addrs->ti->curr_tn;
			JNL_SHORT_TIME(null_record.short_time);
			QWASSIGN(null_record.jnl_seqno, jnl_seqno);
			jnl_status = jnl_ensure_open();
			if (0 == jnl_status)
			{
				if (0 == cs_addrs->jnl->pini_addr)
					jnl_put_jrt_pini(cs_addrs);
				jnl_write(cs_addrs->jnl, JRT_NULL, (jrec_union *)&null_record, NULL, NULL);
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
			cumul_jnl_rec_len = 0;
			if (0 == QWMODDW(jnl_seqno, 1000))
				repl_log(updproc_log_fp, TRUE, TRUE, "Committed NULL Jnl seq no is : "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
			QWINCRBY(jnl_seqno, seq_num_one);
			QWASSIGN(recvpool.upd_proc_local->read_jnl_seqno, jnl_seqno);
			break;
		case JRT_SET:
			v.mvtype = MV_STR;
			v.str.addr = rec->val.jrec_set.mumps_node.text;
			v.str.len = (char *)strchr(v.str.addr, '\0') - (char *)v.str.addr;
			gv_bind_name(gd_header, &(v.str));	/* this sets gv_target */
			v.str.len = rec->val.jrec_set.mumps_node.length;
			/* if (0 != gv_target->clue.end  &&  (FALSE == is_valid_hist(&gv_target->hist))) */
				gv_target->clue.end = 0;
			memcpy(gv_currkey->base, v.str.addr, v.str.len);
			gv_currkey->base[v.str.len] = '\0';
			gv_currkey->end = v.str.len;
			data_len = (mstr_len_t *)((char *)&rec->val.jrec_set.mumps_node + rec->val.jrec_set.mumps_node.length
				+ sizeof(short));
			GET_MSTR_LEN(v.str.len, data_len);
			v.mvtype = MV_STR;
			v.str.addr = (char *)data_len + sizeof(mstr_len_t);
			op_gvput(&v);
			if (0 == QWMODDW(jnl_seqno, 1000))
				repl_log(updproc_log_fp, TRUE, TRUE, "Committed SET Jnl seq no is : "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
			QWINCRBY(jnl_seqno, seq_num_one);
			QWASSIGN(recvpool.upd_proc_local->read_jnl_seqno, jnl_seqno);
			break;
		case JRT_TSET:
		case JRT_USET:
		case JRT_FSET:
		case JRT_GSET:
			v.mvtype = MV_STR;
			v.str.addr = rec->val.jrec_fset.mumps_node.text;
			v.str.len = (char *)strchr(v.str.addr, '\0') - (char *)v.str.addr;
			gv_bind_name(gd_header, &(v.str));	/* this sets gv_target */
			v.str.len = rec->val.jrec_fset.mumps_node.length;
			/* if (0 != gv_target->clue.end  &&  (FALSE == is_valid_hist(&gv_target->hist))) */
				gv_target->clue.end = 0;
			memcpy(gv_currkey->base, v.str.addr, v.str.len);
			gv_currkey->base[v.str.len] = '\0';
			gv_currkey->end = v.str.len;
			assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_gset.mumps_node);
			assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_tset.mumps_node);
			assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_uset.mumps_node);
			data_len = (mstr_len_t *)((char *)&rec->val.jrec_fset.mumps_node + rec->val.jrec_fset.mumps_node.length
				+ sizeof(short));
			GET_MSTR_LEN(v.str.len, data_len);
			v.mvtype = MV_STR;
			v.str.addr = (char *)data_len + sizeof(mstr_len_t);
			op_gvput(&v);
			break;
		case JRT_KILL:
			v.mvtype = MV_STR;
			v.str.addr = rec->val.jrec_kill.mumps_node.text;
			v.str.len = (char *)strchr(v.str.addr, '\0') - (char *)v.str.addr;
			gv_bind_name(gd_header, &(v.str));	/* this sets gv_target */
			v.str.len = rec->val.jrec_kill.mumps_node.length;
			/* if (0 != gv_target->clue.end  &&  (FALSE == is_valid_hist(&gv_target->hist))) */
				gv_target->clue.end = 0;
			memcpy(gv_currkey->base, v.str.addr, v.str.len);
			gv_currkey->base[v.str.len] = '\0';
			gv_currkey->end = v.str.len;
			op_gvkill();
			if (0 == QWMODDW(jnl_seqno, 1000))
				repl_log(updproc_log_fp, TRUE, TRUE, "Committed KILL Jnl seq no is : "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
			QWINCRBY(jnl_seqno, seq_num_one);
			QWASSIGN(recvpool.upd_proc_local->read_jnl_seqno, jnl_seqno);
			break;
		case JRT_ZKILL:
			v.mvtype = MV_STR;
			v.str.addr = rec->val.jrec_zkill.mumps_node.text;
			v.str.len = (char *)strchr(v.str.addr, '\0') - (char *)v.str.addr;
			gv_bind_name(gd_header, &(v.str));	/* this sets gv_target */
			v.str.len = rec->val.jrec_zkill.mumps_node.length;
			/* if (0 != gv_target->clue.end  &&  (FALSE == is_valid_hist(&gv_target->hist))) */
				gv_target->clue.end = 0;
			memcpy(gv_currkey->base, v.str.addr, v.str.len);
			gv_currkey->base[v.str.len] = '\0';
			gv_currkey->end = v.str.len;
			op_gvzwithdraw();
			if (0 == QWMODDW(jnl_seqno, 1000))
				repl_log(updproc_log_fp, TRUE, TRUE, "Committed ZKILL Jnl seq no is : "INT8_FMT" "INT8_FMTX" \n",
					INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
			QWINCRBY(jnl_seqno, seq_num_one);
			QWASSIGN(recvpool.upd_proc_local->read_jnl_seqno, jnl_seqno);
			break;
		case JRT_TKILL:
		case JRT_UKILL:
		case JRT_FKILL:
		case JRT_GKILL:
			v.mvtype = MV_STR;
			v.str.addr = rec->val.jrec_fkill.mumps_node.text;
			v.str.len = (char *)strchr(v.str.addr, '\0') - (char *)v.str.addr;
			gv_bind_name(gd_header, &(v.str));	/* this sets gv_target */
			v.str.len = rec->val.jrec_fkill.mumps_node.length;
			/* if (0 != gv_target->clue.end  &&  (FALSE == is_valid_hist(&gv_target->hist))) */
				gv_target->clue.end = 0;
			memcpy(gv_currkey->base, v.str.addr, v.str.len);
			gv_currkey->base[v.str.len] = '\0';
			gv_currkey->end = v.str.len;
			assert(&rec->val.jrec_fkill.mumps_node == &rec->val.jrec_gkill.mumps_node);
			assert(&rec->val.jrec_fkill.mumps_node == &rec->val.jrec_tkill.mumps_node);
			assert(&rec->val.jrec_fkill.mumps_node == &rec->val.jrec_ukill.mumps_node);
			op_gvkill();
			break;
		case JRT_TZKILL:
		case JRT_UZKILL:
		case JRT_FZKILL:
		case JRT_GZKILL:
			v.mvtype = MV_STR;
			v.str.addr = rec->val.jrec_fzkill.mumps_node.text;
			v.str.len = (char *)strchr(v.str.addr, '\0') - (char *)v.str.addr;
			gv_bind_name(gd_header, &(v.str));	/* this sets gv_target */
			v.str.len = rec->val.jrec_fzkill.mumps_node.length;
			/* if (0 != gv_target->clue.end  &&  (FALSE == is_valid_hist(&gv_target->hist))) */
				gv_target->clue.end = 0;
			memcpy(gv_currkey->base, v.str.addr, v.str.len);
			gv_currkey->base[v.str.len] = '\0';
			gv_currkey->end = v.str.len;
			assert(&rec->val.jrec_fzkill.mumps_node == &rec->val.jrec_gzkill.mumps_node);
			assert(&rec->val.jrec_fzkill.mumps_node == &rec->val.jrec_tzkill.mumps_node);
			assert(&rec->val.jrec_fzkill.mumps_node == &rec->val.jrec_uzkill.mumps_node);
			op_gvzwithdraw();
			break;
		case JRT_TCOM:
			if (tcom_num == tset_num)
			{
				assert(0 != tcom_num);
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
				if (0 == QWMODDW(jnl_seqno, 1000))
					repl_log(updproc_log_fp, TRUE, TRUE,
						"Committed TCOM Jnl seq no is : "INT8_FMT" "INT8_FMTX" \n",
						INT8_PRINT(jnl_seqno), INT8_PRINTX(jnl_seqno));
				QWINCRBY(jnl_seqno, seq_num_one);
				QWASSIGN(recvpool.upd_proc_local->read_jnl_seqno, jnl_seqno);
				tstarted = FALSE;
				tcom_num = tset_num = rec_seqno = 0;
			}
			break;
		}
		readaddrs = readaddrs + rec_len;
		temp_read += rec_len;
		recvpool.upd_proc_local->read = temp_read;
	}
	updproc_continue = FALSE;
}

bool	upd_open_files(upd_proc_ctl **upd_db_files)
{
	upd_proc_ctl	*curr, *prev;
	sgmnt_addrs	*csa;
	char		*fn;
	sm_uc_ptr_t	gld_fn;
	int		i, db_fd;
	uint4		status;
	unsigned char	seq_num_str[32], *seq_num_ptr;
	unsigned char	seq_num_strx[32], *seq_num_ptrx;

	error_def(ERR_REPLEN_JNLDISABLE);
	error_def(ERR_NOREPLCTDREG);

	QWASSIGN(start_jnl_seqno, seq_num_zero);
	QWASSIGN(max_resync_seqno, seq_num_zero);
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
		if (QWLT(max_resync_seqno, csa->hdr->resync_seqno))
			QWASSIGN(max_resync_seqno, csa->hdr->resync_seqno);
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
		repl_log(updproc_log_fp, TRUE, TRUE, "             -------->  start_jnl_seqno = "INT8_FMT" "INT8_FMTX"\n",
			INT8_PRINT(start_jnl_seqno), INT8_PRINTX(start_jnl_seqno));
		if (!REPL_ENABLED(csd))
		{
			curr = curr->next;
			continue;
		} else if (!JNL_ENABLED(csd))
			rts_error(VARLSTCNT(6) ERR_REPLEN_JNLDISABLE, 4, JNL_LEN_STR(csa->hdr), DB_LEN_STR(curr->gd));
		else
			repl_enabled = TRUE;
		prev = curr;
		curr = curr->next;
		prev->next = *upd_db_files;
		*upd_db_files = prev;
	}
	if (recvpool.upd_proc_local->updateresync)
		wcs_flu(TRUE);
	if (NULL == *upd_db_files)
		gtm_putmsg(VARLSTCNT(3) ERR_NOREPLCTDREG, 1, gld_fn);
	return TRUE;
}
