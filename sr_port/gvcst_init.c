/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>		/* for offsetof macro */
#include "gtm_string.h"
#include "gtm_time.h"
#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdskill.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h and MAX macro used below */
#include "gdsblkops.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "gtm_stdlib.h"		/* for ATOI */
#include "gtmimagename.h"
#include "cryptdef.h"
#include "mlkdef.h"
#include "error.h"
#include "gt_timer.h"
#include "trans_log_name.h"
#include "gtm_logicals.h"
#include "dbfilop.h"
#include "set_num_additional_processors.h"
#include "have_crit.h"
#include "t_retry.h"
#include "dpgbldir.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"		/* for CWS_INIT macro */
#include "gvcst_protos.h"	/* for gvcst_init,gvcst_init_sysops,gvcst_tp_init prototype */
#include "compswap.h"
#include "send_msg.h"
#include "targ_alloc.h"		/* for "targ_free" prototype */
#include "hashtab_mname.h"	/* for CWS_INIT macro */
#include "process_gvt_pending_list.h"
#include "gvt_hashtab.h"
#include "gtmmsg.h"
#include "op.h"
#include "set_gbuff_limit.h"
#include "gtm_reservedDB.h"
#include "anticipatory_freeze.h"
#include "wbox_test_init.h"
#include "ftok_sems.h"
#include "util.h"
#include "getzposition.h"
#include "gtmio.h"
#include "io.h"
#include "ipcrmid.h"
#include "gtm_ipc.h"
#include "gtm_semutils.h"
#include "gtm_sem.h"
#include "is_file_identical.h"

#ifdef	GTM_FD_TRACE
#include "gtm_dbjnl_dupfd_check.h"
#endif

/* Deferred database encryption initialization. Check the key handle and skip if already initialized  */
#define INIT_DEFERRED_DB_ENCRYPTION_IF_NEEDED(REG, CSA, CSD)								\
MBSTART {														\
	int		init_status;											\
	int		fn_len;												\
	char		*fn;												\
	boolean_t	do_crypt_init;											\
	DEBUG_ONLY(boolean_t	was_gtmcrypt_initialized = gtmcrypt_initialized);					\
															\
	do_crypt_init = ((USES_ENCRYPTION(CSD->is_encrypted)) && !IS_LKE_IMAGE && CSA->encr_ptr				\
				&& (GTMCRYPT_INVALID_KEY_HANDLE == (CSA)->encr_key_handle)				\
				&& !(CSA->encr_ptr->issued_db_init_crypt_warning)					\
				&& (CSA->encr_ptr->reorg_encrypt_cycle == CSA->nl->reorg_encrypt_cycle));		\
	if (do_crypt_init)												\
	{														\
		assert(was_gtmcrypt_initialized);									\
		fn = (char *)(REG->dyn.addr->fname);									\
		fn_len = REG->dyn.addr->fname_len;									\
		INIT_DB_OR_JNL_ENCRYPTION(CSA, CSD, fn_len, fn, init_status);						\
		if ((0 != init_status) && (CSA->encr_ptr->reorg_encrypt_cycle == CSA->nl->reorg_encrypt_cycle))		\
		{													\
			if (IS_GTM_IMAGE || mu_reorg_encrypt_in_prog)							\
			{												\
				GTMCRYPT_REPORT_ERROR(init_status, rts_error, fn_len, fn);				\
			} else												\
			{												\
				GTMCRYPT_REPORT_ERROR(MAKE_MSG_WARNING(init_status), gtm_putmsg, fn_len, fn);		\
				CSA->encr_ptr->issued_db_init_crypt_warning = TRUE;					\
			}												\
		}													\
	}														\
} MBEND

GBLREF	boolean_t		mu_reorg_process;
GBLREF	boolean_t		created_core, dont_want_core;
GBLREF  gd_region               *gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	sgmnt_addrs		*cs_addrs_list;
GBLREF	boolean_t		gtmcrypt_initialized;
GBLREF	boolean_t		gtcm_connection;
GBLREF	gd_addr			*gd_header;
GBLREF	bool			licensed;
GBLREF	int4			lkid;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	uint4			update_array_size, cumul_update_array_size;
GBLREF	ua_list			*first_ua, *curr_ua;
GBLREF	short			crash_count;
GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			dollar_trestart;
GBLREF	uint4			mu_reorg_encrypt_in_prog;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	buddy_list		*global_tlvl_info_list;
GBLREF	tp_region		*tp_reg_free_list;	/* Ptr to list of tp_regions that are unused */
GBLREF	tp_region		*tp_reg_list;		/* Ptr to list of tp_regions for this transaction */
GBLREF	unsigned int		t_tries;
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF	boolean_t		tp_in_use;
GBLREF	uint4			region_open_count;
GBLREF	sm_uc_ptr_t		reformat_buffer;
GBLREF	int			reformat_buffer_len;
GBLREF	volatile int		reformat_buffer_in_use;	/* used only in DEBUG mode */
GBLREF	volatile int4		fast_lock_count;
GBLREF	boolean_t		dse_running;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	int			pool_init;
GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	uint4			process_id;
LITREF char			gtm_release_name[];
LITREF int4			gtm_release_name_len;
LITREF mval			literal_statsDB_gblname;

#define MAX_DBINIT_RETRY	3
#define MAX_DBFILOPN_RETRY_CNT	4

/* In code below to (re)create a statsDB file, we are doing a number of operations under lock. Rather than deal with
 * getting out of the lock before the error, handle the possible errors after we are out from under the lock. These
 * values are the possible errors we need may need to raise.
 */
typedef enum {
	STATSDB_NOERR = 0,
	STATSDB_NOTEEXIST,
	STATSDB_OPNERR,
	STATSDB_READERR,
	STATSDB_NOTSTATSDB,
	STATSDB_NOTOURS,
	STATSDB_UNLINKERR,
	STATSDB_RECREATEERR,
	STATSDB_CLOSEERR,
	STATSDB_SHMRMIDERR,
	STATSDB_SEMRMIDERR,
	STATSDB_FTOKSEMRMIDERR
} statsdb_recreate_errors;

error_def(ERR_BADDBVER);
error_def(ERR_DBCREINCOMP);
error_def(ERR_DBFLCORRP);
error_def(ERR_DBGLDMISMATCH);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBOPNERR);
error_def(ERR_DBROLLEDBACK);
error_def(ERR_DBVERPERFWARN1);
error_def(ERR_DBVERPERFWARN2);
error_def(ERR_DRVLONGJMP);	/* Generic internal only error used to drive longjump() in a queued condition handler */
error_def(ERR_INVSTATSDB);
error_def(ERR_MMNODYNUPGRD);
error_def(ERR_REGOPENFAIL);
error_def(ERR_STATSDBFNERR);
error_def(ERR_STATSDBINUSE);

static readonly mval literal_poollimit =
	DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, (SIZEOF("POOLLIMIT") - 1), "POOLLIMIT", 0, 0);

void	assert_jrec_member_offsets(void)
{
	assert(REAL_JNL_HDR_LEN % DISK_BLOCK_SIZE == 0);
	assert(JNL_HDR_LEN % DISK_BLOCK_SIZE == 0);
	/* We currently assume that the journal file header size is aligned relative to the filesystem block size.
	 * which is currently assumed to be a 2-power (e.g. 512 bytes, 1K, 2K, 4K etc.) but never more than 64K
	 * (MAX_IO_BLOCK_SIZE). Given this, we keep the journal file header size at 64K for Unix.
	 * This way any process updating the file header will hold crit and do aligned writes. Any process
	 * writing the journal file data (journal records) on disk will hold the qio lock and can safely do so without
	 * ever touching the journal file header area. If ever MAX_IO_BLOCK_SIZE changes (say because some filesystem
	 * block size changes to 128K) such that JNL_HDR_LEN is no longer aligned to that, we want to know hence this assert.
	 */
	assert(JNL_HDR_LEN % MAX_IO_BLOCK_SIZE == 0);
	assert(REAL_JNL_HDR_LEN == SIZEOF(jnl_file_header));
	assert(REAL_JNL_HDR_LEN <= JNL_HDR_LEN);
	assert(JNL_HDR_LEN == JNL_FILE_FIRST_RECORD);
	assert(DISK_BLOCK_SIZE >= PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN);
	assert((JNL_ALLOC_MIN * DISK_BLOCK_SIZE) > JNL_HDR_LEN);
	/* Following assert is for JNL_FILE_TAIL_PRESERVE macro in jnl.h */
	assert(PINI_RECLEN >= EPOCH_RECLEN && PINI_RECLEN >= PFIN_RECLEN && PINI_RECLEN >= EOF_RECLEN);
	/* jnl_string structure has a 8-bit nodeflags field and a 24-bit length field. In some cases, this is
	 * used as a 32-bit length field (e.g. in the value part of the SET record or ZTWORMHOLE or LGTRIG record).
	 * These usages treat the 32-bits as a jnl_str_len_t type and access it directly. Hence the requirement that
	 * jnl_str_len_t be the same size as 32-bits and also the same as the offset to the "text" member.
	 * If this assert fails, all places that reference jnl_str_len_t need to be revisited.
	 */
	assert(SIZEOF(jnl_str_len_t) == SIZEOF(uint4));
	assert(SIZEOF(jnl_str_len_t) == offsetof(jnl_string, text[0]));
	/* since time in jnl record is a uint4, and since JNL_SHORT_TIME expects time_t, we better ensure they are same.
	 * A change in the size of time_t would mean a redesign of the fields.  */

	assert(SIZEOF(time_t) == GTM64_ONLY(SIZEOF(gtm_int8)) NON_GTM64_ONLY(SIZEOF(int4)));

	/* Make sure all jnl_seqno fields start at same offset. mur_output_record and others rely on this. */
	assert(offsetof(struct_jrec_null, jnl_seqno) == offsetof(struct_jrec_upd, token_seq.jnl_seqno));
	assert(offsetof(struct_jrec_null, jnl_seqno) == offsetof(struct_jrec_epoch, jnl_seqno));
	assert(offsetof(struct_jrec_null, jnl_seqno) == offsetof(struct_jrec_eof, jnl_seqno));
	assert(offsetof(struct_jrec_null, jnl_seqno) == offsetof(struct_jrec_tcom, token_seq.jnl_seqno));
	assert(offsetof(struct_jrec_null, jnl_seqno) == offsetof(struct_jrec_ztworm, token_seq.jnl_seqno));
	assert(offsetof(struct_jrec_null, jnl_seqno) == offsetof(struct_jrec_lgtrig, token_seq.jnl_seqno));

	/* Make sure all strm_seqno fields start at same offset. Lot of modules rely on this */
	assert(offsetof(struct_jrec_null, strm_seqno) == offsetof(struct_jrec_upd, strm_seqno));
	assert(offsetof(struct_jrec_null, strm_seqno) == offsetof(struct_jrec_tcom, strm_seqno));
	assert(offsetof(struct_jrec_null, strm_seqno) == offsetof(struct_jrec_ztworm, strm_seqno));
	assert(offsetof(struct_jrec_null, strm_seqno) == offsetof(struct_jrec_lgtrig, strm_seqno));
	/* EOF and EPOCH are not included in the above asserts because they have not ONE but 16 strm_seqno values each */

	assert(offsetof(struct_jrec_ztcom, token) == offsetof(struct_jrec_upd, token_seq));
	/* Make sure all jnl_seqno and token fields start at 8-byte boundary */
	assert(offsetof(struct_jrec_upd, token_seq.jnl_seqno) ==
		(ROUND_UP(offsetof(struct_jrec_upd, token_seq.jnl_seqno), SIZEOF(seq_num))));
	assert(offsetof(struct_jrec_tcom, token_seq.jnl_seqno) ==
		(ROUND_UP(offsetof(struct_jrec_tcom, token_seq.jnl_seqno), SIZEOF(seq_num))));
	assert(offsetof(struct_jrec_null, jnl_seqno) ==
		(ROUND_UP(offsetof(struct_jrec_null, jnl_seqno), SIZEOF(seq_num))));
	assert(offsetof(struct_jrec_epoch, jnl_seqno) ==
		(ROUND_UP(offsetof(struct_jrec_epoch, jnl_seqno), SIZEOF(seq_num))));
	assert(offsetof(struct_jrec_eof, jnl_seqno) ==
		(ROUND_UP(offsetof(struct_jrec_eof, jnl_seqno), SIZEOF(seq_num))));
	/* All fixed size records must be multiple of 8-byte */
	assert(TCOM_RECLEN == (ROUND_UP(SIZEOF(struct_jrec_tcom), JNL_REC_START_BNDRY)));
	assert(ZTCOM_RECLEN == (ROUND_UP(SIZEOF(struct_jrec_ztcom), JNL_REC_START_BNDRY)));
	assert(INCTN_RECLEN == (ROUND_UP(SIZEOF(struct_jrec_inctn), JNL_REC_START_BNDRY)));
	assert(PINI_RECLEN == (ROUND_UP(SIZEOF(struct_jrec_pini), JNL_REC_START_BNDRY)));
	assert(PFIN_RECLEN == (ROUND_UP(SIZEOF(struct_jrec_pfin), JNL_REC_START_BNDRY)));
	assert(NULL_RECLEN == (ROUND_UP(SIZEOF(struct_jrec_null), JNL_REC_START_BNDRY)));
	assert(EPOCH_RECLEN == (ROUND_UP(SIZEOF(struct_jrec_epoch), JNL_REC_START_BNDRY)));
	assert(EOF_RECLEN == (ROUND_UP(SIZEOF(struct_jrec_eof), JNL_REC_START_BNDRY)));
	/* Assumption about the structures in code */
	assert(0 == MIN_ALIGN_RECLEN % JNL_REC_START_BNDRY);
	assert(SIZEOF(uint4) == SIZEOF(jrec_suffix));
	assert((SIZEOF(jnl_record) + MAX_LOGI_JNL_REC_SIZE + SIZEOF(jrec_suffix)) < MAX_JNL_REC_SIZE);
	assert((DISK_BLOCK_SIZE * JNL_DEF_ALIGNSIZE) >= MAX_JNL_REC_SIZE);/* default alignsize supports max jnl record length */
	assert(MAX_MAX_NONTP_JNL_REC_SIZE <= MAX_JNL_REC_SIZE);
	assert(MAX_DB_BLK_SIZE < MAX_MAX_NONTP_JNL_REC_SIZE);	/* Ensure a PBLK record can accommodate a full GDS block */
	assert(MAX_JNL_REC_SIZE <= (1 << 24));
		/* Ensure that the 24-bit length field in the journal record can accommodate the maximum journal record size */
	assert(tcom_record.prefix.forwptr == tcom_record.suffix.backptr);
	assert(TCOM_RECLEN == tcom_record.suffix.backptr);
	assert(SIZEOF(token_split_t) == SIZEOF(token_build));   /* Required for TOKEN_SET macro */
}

CONDITION_HANDLER(gvcst_init_autoDB_ch)
{
	START_CH(TRUE);
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
	{
		assert(FALSE);  /* don't know of any possible INFO/SUCCESS errors */
		CONTINUE;                       /* Keep going for non-error issues */
	}
	/* Enable interrupts in case we are here with intrpt_ok_state == INTRPT_IN_GVCST_INIT due to an rts error.
	 * Normally we would have the new state stored in "prev_intrpt_state" but that is not possible here because
	 * the corresponding DEFER_INTERRUPTS happened in "gvcst_init" (a different function) so we have an assert
	 * there that the previous state was INTRPT_OK_TO_INTERRUPT and use that instead of prev_intrpt_state here.
	 */
	ENABLE_INTERRUPTS(INTRPT_IN_GVCST_INIT, INTRPT_OK_TO_INTERRUPT);
	NEXTCH;
}

void gvcst_init(gd_region *reg, gd_addr *addr)
{
	gd_segment		*seg;
	sgmnt_addrs		*baseDBcsa, *csa, *prevcsa, *regcsa;
	sgmnt_data_ptr_t	csd;
	sgmnt_data		statsDBcsd;
	jnlpool_addrs_ptr_t	save_jnlpool;
	uint4			segment_update_array_size;
	int4			bsize, padsize;
	boolean_t		is_statsDB, realloc_alt_buff, retry_dbinit;
	file_control		*fc;
	gd_region		*prev_reg, *reg_top, *baseDBreg, *statsDBreg;
#	ifdef DEBUG
	cache_rec_ptr_t		cr;
	bt_rec_ptr_t		bt;
	blk_ident		tmp_blk;
#	endif
	int			db_init_ret, loopcnt, max_fid_index, fd, rc, save_errno, errrsn_text_len, status;
	mstr			log_nam, trans_log_nam;
	char			trans_buff[MAX_FN_LEN + 1], statsdb_path[MAX_FN_LEN + 1], *errrsn_text;
	unique_file_id		*reg_fid, *tmp_reg_fid;
	gd_id			replfile_gdid, *tmp_gdid;
	tp_region		*tr;
	ua_list			*tmp_ua;
	time_t			curr_time;
	uint4			curr_time_uint4, next_warn_uint4;
	unsigned int            minus1 = (unsigned)-1;
	enum db_acc_method	reg_acc_meth;
	boolean_t		onln_rlbk_cycle_mismatch = FALSE;
	boolean_t		replpool_valid = FALSE, replfilegdid_valid = FALSE, jnlpool_found = FALSE;
	intrpt_state_t		save_intrpt_ok_state;
	replpool_identifier	replpool_id;
	unsigned int		full_len;
	int4			db_init_retry;
	srch_blk_status		*bh;
	mstr			*gld_str;
	node_local_ptr_t	baseDBnl;
	unsigned char		cstatus;
	statsdb_recreate_errors	statsdb_rcerr;
	jbuf_rsrv_struct_t	*nontp_jbuf_rsrv_lcl;
	intrpt_state_t		prev_intrpt_state;
	key_t			ftok_key;
	int			ftok_semid;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	UNSUPPORTED_PLATFORM_CHECK;
	/* If this is a statsDB, then open its baseDB first (if not already open). Most of the times the baseDB would be open.
	 * In rare cases like direct use of ^%YGS global, it is possible baseDB is not open.
	 */
	assert(!reg->open);
	is_statsDB = IS_STATSDB_REG(reg);
	if (is_statsDB)
	{
		STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
		if (!baseDBreg->open)
		{
			DBGRDB((stderr, "gvcst_init: !baseDBreg->open (NOT open)\n"));
			gvcst_init(baseDBreg, addr);
			assert(baseDBreg->open);
			if (reg->open)	/* statsDB was opened as part of opening baseDB. No need to do anything more here */
			{
				DBGRDB((stderr, "gvcst_init: reg->open return\n"));
				return;
			}
			/* At this point, the baseDB is open but the statsDB is not automatically opened. This is possible if
			 *	a) TREF(statshare_opted_in) is FALSE. In that case, this call to "gvcst_init" is coming through
			 *		a direct reference to the statsDB (e.g. ZWR ^%YGS). OR
			 *	b) baseDBreg->was_open is TRUE. In that case, the statsDB open would have been short-circuited
			 *		in "gvcst_init".
			 *	c) Neither (a) nor (b). This means an open of the statsDB was attempted as part of the baseDB
			 *		"gvcst_init" done above. Since it did not, that means some sort of error occurred that
			 *		has sent messages to the user console or syslog so return right away to the caller which
			 *		would silently adjust gld map entries so they do not point to this statsDB anymore
			 *		(NOSTATS should already be set in the baseDB in this case, assert that).
			 */
			if (TREF(statshare_opted_in) && !baseDBreg->was_open)
			{
				assert(RDBF_NOSTATS & baseDBreg->reservedDBFlags);
				return;
			}
		} else if (RDBF_NOSTATS & baseDBreg->reservedDBFlags)
		{	/* The baseDB was already open and the statsDB was NOT open. This could be because of either the baseDB
			 * has NOSTATS set in it or it could be that NOSTATS was set when we attempted before to open the statsDB
			 * but failed for whatever reason (privs, noexistant directory, space, etc). In either case, return
			 * right away (for same reason as described before the "if" block above).
			 */
			return;
		}
		baseDBcsa = &FILE_INFO(baseDBreg)->s_addrs;
		baseDBnl = baseDBcsa->nl;
		if (0 == baseDBnl->statsdb_fname_len)
		{	/* This can only be true if it was set that way in gvcst_set_statsdb_fname(). Each error case sets a
			 * reason value in TREF(statsdb_fnerr_reason) so use that to give a useful error message.
			 */
			switch(TREF(statsdb_fnerr_reason))
			{
				case FNERR_NOSTATS:
					errrsn_text = FNERR_NOSTATS_TEXT;
					errrsn_text_len = SIZEOF(FNERR_NOSTATS_TEXT) - 1;
					break;
				case FNERR_STATSDIR_TRNFAIL:
					errrsn_text = FNERR_STATSDIR_TRNFAIL_TEXT;
					errrsn_text_len = SIZEOF(FNERR_STATSDIR_TRNFAIL_TEXT) - 1;
					break;
				case FNERR_STATSDIR_TRN2LONG:
					errrsn_text = FNERR_STATSDIR_TRN2LONG_TEXT;
					errrsn_text_len = SIZEOF(FNERR_STATSDIR_TRN2LONG_TEXT) - 1;
					break;
				case FNERR_INV_BASEDBFN:
					errrsn_text = FNERR_INV_BASEDBFN_TEXT;
					errrsn_text_len = SIZEOF(FNERR_INV_BASEDBFN_TEXT) - 1;
					break;
				case FNERR_FTOK_FAIL:
					errrsn_text = FNERR_FTOK_FAIL_TEXT;
					errrsn_text_len = SIZEOF(FNERR_FTOK_FAIL_TEXT) - 1;
					break;
				case FNERR_FNAMEBUF_OVERFLOW:
					errrsn_text = FNERR_FNAMEBUF_OVERFLOW_TEXT;
					errrsn_text_len = SIZEOF(FNERR_FNAMEBUF_OVERFLOW_TEXT) - 1;
					break;
				case FNERR_NOERR:
				default:
					assertpro(FALSE);
			}
			assert(TREF(gvcst_statsDB_open_ch_active));	/* so the below error goes to syslog and not to user */
			baseDBreg->reservedDBFlags |= RDBF_NOSTATS;	/* Disable STATS in base DB */
			baseDBcsa->reservedDBFlags |= RDBF_NOSTATS;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(baseDBreg), ERR_STATSDBFNERR, 2,
				      errrsn_text_len, errrsn_text);
		}
		COPY_STATSDB_FNAME_INTO_STATSREG(reg, baseDBnl->statsdb_fname, baseDBnl->statsdb_fname_len);
		seg = reg->dyn.addr;
		if (!seg->blk_size)
		{	/* This is a region/segment created by "mu_gv_cur_reg_init" (which sets most of reg/seg fields to 0).
			 * Now that we need a non-zero blk_size, do what GDE did to calculate the statsdb block-size.
			 * But since we cannot duplicate that code here, we set this to the same value effectively but
			 * add an assert that the two are the same in "gd_load" function.
			 * Take this opportunity to initialize other seg/reg fields for statsdbs like what GDE would have done.
			 */
			seg->blk_size = STATSDB_BLK_SIZE;
			/* Similar code for a few other critical fields that need initialization before "mu_cre_file" */
			seg->allocation = STATSDB_ALLOCATION;
			seg->ext_blk_count = STATSDB_EXTENSION;
			reg->max_key_size = STATSDB_MAX_KEY_SIZE;
			reg->max_rec_size = STATSDB_MAX_REC_SIZE;
			/* The below is directly inherited from the base db so no macro/assert like above fields */
			seg->mutex_slots = NUM_CRIT_ENTRY(baseDBcsa->hdr);
			reg->mumps_can_bypass = TRUE;
		}
		/* Before "dbfilopn" of a statsdb, check if it has been created. If not, create it (as it is an autodb).
		 * Use FTOK of the base db as a lock, the same lock that is obtained when statsdb is auto deleted.
		 */
		if (!baseDBnl->statsdb_created)
		{
			/* Disable interrupts for the time we hold the ftok lock as it is otherwise possible we get a SIG-15
			 * and go to exit handling and try a nested "ftok_sem_lock" on the same basedb and that could pose
			 * multiple issues (e.g. ftok_sem_lock starts a timer while waiting in the "semop" call and we will
			 * have the same timer-id added twice due to the nested call).
			 */
			DBGRDB((stderr, "gvcst_init: !baseDBnl->statsdb_created\n"));
			if (IS_TP_AND_FINAL_RETRY && baseDBcsa->now_crit)
			{	/* If this is a TP transaction and in the final retry, we are about to request the ftok
				 * sem lock on baseDBreg while already holding crit on baseDBReg. That is an out-of-order
				 * request which can lead to crit/ftok deadlocks so release crit before requesting it.
				 * This code is similar to the TPNOTACID_CHECK macro with the below exceptions.
				 *	a) We do not want to issue the TPNOTACID syslog message since there is no ACID
				 *		violation here AND
				 *	b) We have to check for baseDBcsa->now_crit in addition to IS_TP_AND_FINAL_RETRY
				 *		as it is possible this call comes from "tp_restart -> gv_init_reg -> gvcst_init"
				 *		AND t_tries is still 3 but we do not hold crit on any region at that point
				 *		(i.e. "tp_crit_all_regions" call is not yet done) and in that case we should
				 *		not decrement t_tries (TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK call below)
				 *		as it would result in we later starting the final retry with t_tries = 2 but
				 *		holding crit on all regions which is an out-of-design situation.
				 */
				TP_REL_CRIT_ALL_REG;
				assert(!baseDBcsa->now_crit);
				assert(!mupip_jnl_recover);
				TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK;
			}
			DEFER_INTERRUPTS(INTRPT_IN_GVCST_INIT, prev_intrpt_state);
			assert(INTRPT_OK_TO_INTERRUPT == prev_intrpt_state);	/* relied upon by ENABLE_INTERRUPTS
										 * in "gvcst_init_autoDB_ch".
										 */
			ESTABLISH(gvcst_init_autoDB_ch);
			if (!ftok_sem_lock(baseDBreg, FALSE))
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(baseDBcsa) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(baseDBreg));
			}
			for ( ; ; )     /* for loop only to let us break from error cases without having a deep if-then-else */
			{
				/* Now that we have the lock, check if the db is already created */
				if (baseDBnl->statsdb_created)
					break;
				/* File still not created. Do it now under the ftok lock. */
				DBGRDB((stderr, "gvcst_init: !baseDBnl->statsdb_created (under lock)\n"));
				cstatus = gvcst_cre_autoDB(reg);
				if (EXIT_NRM == cstatus)
				{
					baseDBnl->statsdb_created = TRUE;
					break;
				}
				/* File failed to create - if this is a case where the file exists (but was not supposed
				 * to exist), we should remove/recreate the file. Otherwise, we have a real error -
				 * probably a missing directory or permissions or some such. In that case, turn off stats
				 * and unwind the failed open.
				 */
				statsdb_rcerr = STATSDB_NOERR;		/* Assume no error to occur */
				save_errno = TREF(mu_cre_file_openrc);	/* Save the errno that stopped our recreate */
				DBGRDB((stderr, "gvcst_init: Create of statsDB failed - rc = %d\n", save_errno));
				for ( ; ; )     /* for loop only to handle error cases without having a deep if-then-else */
				{
					if (EEXIST != TREF(mu_cre_file_openrc))
					{
						statsdb_rcerr = STATSDB_NOTEEXIST;
						break;
					}
					/* See if this statsdb file is really supposed to be linked to the baseDB we
					 * also just opened. To do this, we need to open the statsdb temporarily, read
					 * its fileheader and then close it to get the file-header field we want. Note
					 * this is a very quick operation so we don't use the normal DB utilities on it.
					 * We just want to read the file header and get out. Since we have the lock, no
					 * other processes will have this database open unless we are illegitimately
					 * opening it. This is an infrequent issue so the overhead is not relevant.
					 */
					DBGRDB((stderr, "gvcst_init: Test to see if file is 'ours'\n"));
					memcpy(statsdb_path, reg->dyn.addr->fname, reg->dyn.addr->fname_len);
					statsdb_path[reg->dyn.addr->fname_len] = '\0';	/* Rebuffer fn to include null */
					fd = OPEN(statsdb_path, O_RDONLY);
					if (0 > fd)
					{	/* Some sort of open error occurred */
						statsdb_rcerr = STATSDB_OPNERR;
						save_errno = errno;
						break;
					}
					/* Open worked - read file header from it*/
					LSEEKREAD(fd, 0, (char *)&statsDBcsd, SIZEOF(sgmnt_data), rc);
					if (0 > rc)
					{	/* Wasn't enough data to be a file header - not a statsDB */
						statsdb_rcerr = STATSDB_NOTSTATSDB;
						CLOSEFILE(fd, rc);
						break;
					}
					if (0 < rc)
					{	/* Unknown error while doing read */
						statsdb_rcerr = STATSDB_READERR;
						save_errno = rc;
						CLOSEFILE(fd, rc);
						break;
					}
					/* This is the case (0 == rc) */
					/* Read worked - check if these two files are a proper pair */
					if ((baseDBreg->dyn.addr->fname_len != statsDBcsd.basedb_fname_len)
						|| (0 != memcmp(baseDBreg->dyn.addr->fname,
							statsDBcsd.basedb_fname, statsDBcsd.basedb_fname_len)))
					{	/* This file is in use by another database */
						statsdb_rcerr = STATSDB_NOTOURS;
						CLOSEFILE(fd, rc);
						break;
					}
					/* This file was for us - unlink and recreate */
					DBGRDB((stderr, "gvcst_init: File is ours - unlink and recreate it\n"));
#					ifdef BYPASS_UNLINK_RECREATE_STATSDB
					baseDBnl->statsdb_created = TRUE;
#					else
					/* Before removing a leftover statsdb file, check if it has corresponding private
					 * semid/shmid or ftok_semid. If so, remove them too.
					 */
					if ((INVALID_SHMID != statsDBcsd.shmid) && (0 != shm_rmid(statsDBcsd.shmid)))
					{
						statsdb_rcerr = STATSDB_SHMRMIDERR;
						save_errno = errno;
						CLOSEFILE(fd, rc);
						break;
					}
					if ((INVALID_SEMID != statsDBcsd.semid) && (0 != sem_rmid(statsDBcsd.semid)))
					{
						statsdb_rcerr = STATSDB_SEMRMIDERR;
						save_errno = errno;
						CLOSEFILE(fd, rc);
						break;
					}
					if ((-1 != (ftok_key = FTOK(statsdb_path, GTM_ID)))
						&& (INVALID_SEMID
							!= (ftok_semid = semget(ftok_key, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT)))
						&& (0 == semctl(ftok_semid, DB_COUNTER_SEM, GETVAL))
						&& (0 != sem_rmid(ftok_semid)))
					{
						statsdb_rcerr = STATSDB_FTOKSEMRMIDERR;
						save_errno = errno;
						CLOSEFILE(fd, rc);
						break;
					}
					rc = UNLINK(statsdb_path);
					if (0 > rc)
					{	/* Unlink failed - may not have permissions */
						statsdb_rcerr = STATSDB_UNLINKERR;
						save_errno = errno;
						CLOSEFILE(fd, rc);
						break;
					}
					/* Unlink succeeded - recreate now */
					cstatus = gvcst_cre_autoDB(reg);
					if (EXIT_NRM != cstatus)
					{	/* Recreate failed */
						statsdb_rcerr = STATSDB_RECREATEERR;
						save_errno = TREF(mu_cre_file_openrc);
						CLOSEFILE(fd, rc);
						break;
					}
					baseDBnl->statsdb_created = TRUE;
#					endif
					CLOSEFILE(fd, rc);
					if (0 < rc)
					{	/* Close failed */
						statsdb_rcerr = STATSDB_CLOSEERR;
						save_errno = rc;
					}
					break;
				}
				if (STATSDB_NOERR != statsdb_rcerr)
				{	/* If we could not create or recreate the file, finish our error processing here */
					assert(TREF(gvcst_statsDB_open_ch_active));	/* so the below rts_error_csa calls
											 * go to syslog and not to user.
											 */
					baseDBreg->reservedDBFlags |= RDBF_NOSTATS;	/* Disable STATS in base DB */
					baseDBcsa->reservedDBFlags |= RDBF_NOSTATS;
					if (!ftok_sem_release(baseDBreg, FALSE, FALSE))
					{	/* Release the lock before unwinding back */
						assert(FALSE);
						rts_error_csa(CSA_ARG(baseDBcsa) VARLSTCNT(4) ERR_DBFILERR, 2,
							      DB_LEN_STR(baseDBreg));
					}
					/* For those errors that need a special error message, take care of that here
					 * now that we've released the lock.
					 */
					switch(statsdb_rcerr)
					{
						case STATSDB_NOTEEXIST:		/* Some error occurred handled elsewhere */
							break;
						case STATSDB_NOTOURS:		/* This statsdb already in use elsewhere */
							/* We are trying to attach a statsDB to our baseDB should not be
							 * associated with that baseDB. First check if this IS a statsdb.
							 */
							if (IS_RDBF_STATSDB(&statsDBcsd))
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_STATSDBINUSE,
									      6, DB_LEN_STR(reg),
									      statsDBcsd.basedb_fname_len,
									      statsDBcsd.basedb_fname,
									      DB_LEN_STR(baseDBreg));
							else
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVSTATSDB,
									      4, DB_LEN_STR(reg), REG_LEN_STR(reg));
							break;			/* For the compiler */
						case STATSDB_OPNERR:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBOPNERR, 2,
								      DB_LEN_STR(reg), save_errno);
							break;			/* For the compiler */
						case STATSDB_READERR:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2,
								      DB_LEN_STR(reg), save_errno);
							break;			/* For the compiler */
						case STATSDB_NOTSTATSDB:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVSTATSDB, 4,
								      DB_LEN_STR(reg), REG_LEN_STR(reg));
							break;			/* For the compiler */
						case STATSDB_UNLINKERR:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
								      LEN_AND_LIT("unlink()"), CALLFROM, save_errno);
							break;			/* For the compiler */
						case STATSDB_RECREATEERR:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBOPNERR, 2,
								      DB_LEN_STR(reg), save_errno);
							break;			/* For the compiler */
						case STATSDB_CLOSEERR:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
								      LEN_AND_LIT("close()"), CALLFROM, save_errno);
							break;			/* For the compiler */
						case STATSDB_SHMRMIDERR:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
								      LEN_AND_LIT("shm_rmid()"), CALLFROM, save_errno);
							break;			/* For the compiler */
						case STATSDB_SEMRMIDERR:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
								      LEN_AND_LIT("sem_rmid()"), CALLFROM, save_errno);
							break;			/* For the compiler */
						case STATSDB_FTOKSEMRMIDERR:
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
								      LEN_AND_LIT("ftok sem_rmid()"), CALLFROM, save_errno);
							break;			/* For the compiler */
						default:
							assertpro(FALSE);
					}
					if (TREF(gvcst_statsDB_open_ch_active))
					{	/* Unwind back to ESTABLISH_NORET where did gvcst_init() call to open this
						 * statsDB which now won't open.
						 */
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DRVLONGJMP);
					} else
					{	/* We are not nested so can give the appropriate error ourselves */
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DBFILERR, 2,
							      DB_LEN_STR(reg), ERR_TEXT, 2,
							      RTS_ERROR_TEXT("See preceding errors written to syserr"
									     " and/or syslog for details"));
					}
				}
				break;
			}
			if (!ftok_sem_release(baseDBreg, FALSE, FALSE))
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(baseDBcsa) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(baseDBreg));
			}
			REVERT;
			ENABLE_INTERRUPTS(INTRPT_IN_GVCST_INIT, prev_intrpt_state);
		}
	}
#	ifdef DEBUG
	else
	{
		BASEDBREG_TO_STATSDBREG(reg, statsDBreg);
		assert(!statsDBreg->open);
	}
#	endif
	assert(!jgbl.forw_phase_recovery);
	CWS_INIT;	/* initialize the cw_stagnate hash-table */
	/* check the header design assumptions */
	assert(SIZEOF(th_rec) == (SIZEOF(bt_rec) - SIZEOF(bt->blkque)));
	assert(SIZEOF(cache_rec) == (SIZEOF(cache_state_rec) + SIZEOF(cr->blkque)));
	DEBUG_ONLY(assert_jrec_member_offsets());
	assert(MAX_DB_BLK_SIZE < (1 << NEXT_OFF_MAX_BITS));	/* Ensure a off_chain record's next_off member
								 * can work with all possible block sizes */
	set_num_additional_processors();
#	ifdef DEBUG
	/* Note that the "block" member in the blk_ident structure in gdskill.h has 30 bits.
	 * Currently, the maximum number of blocks is 2**30. If ever this increases, something
	 * has to be correspondingly done to the "block" member to increase its capacity.
	 * The following assert checks that we always have space in the "block" member
	 * to represent a GDS block number.
	 */
	tmp_blk.block = minus1;
	assert(MAXTOTALBLKS_MAX - 1 <= tmp_blk.block);
#	endif
	/* TH_BLOCK is currently a hardcoded constant as basing it on the offsetof macro does not work with the VMS compiler.
	 * Therefore assert that TH_BLOCK points to the 512-byte block where the "trans_hist" member lies in the fileheader.
	 */
	assert(DIVIDE_ROUND_UP(offsetof(sgmnt_data, trans_hist), DISK_BLOCK_SIZE) == TH_BLOCK);
	/* Here's the shared memory layout:
	 *
	 * low address
	 *
	 * both
	 *	segment_data
	 *	(file_header)
	 *	MM_BLOCK
	 *	(master_map)
	 *	TH_BLOCK
	 * BG
	 *	bt_header
	 *	(bt_buckets * bt_rec)
	 *	th_base (SIZEOF(que_ent) into an odd bt_rec)
	 *	bt_base
	 *	(n_bts * bt_rec)
	 *	LOCK_BLOCK (lock_space)
	 *	(lock_space_size)
	 *	cs_addrs->acc_meth.bg.cache_state
	 *	(cache_que_heads)
	 *	(bt_buckets * cache_rec)
	 *	(n_bts * cache_rec)
	 *	critical
	 *	(mutex_struct)
	 *	nl
	 *	(node_local)
	 *	[jnl_name
	 *	jnl_buffer]
	 * MM
	 *	file contents
	 *	LOCK_BLOCK (lock_space)
	 *	(lock_space_size)
	 *	cs_addrs->acc_meth.mm.mmblk_state
	 *	(mmblk_que_heads)
	 *	(bt_buckets * mmblk_rec)
	 *	(n_bts * mmblk_rec)
	 *	critical
	 *	(mutex_struct)
	 *	nl
	 *	(node_local)
	 *	[jnl_name
	 *	jnl_buffer]
	 * high address
	 */
 	/* Ensure first 3 members (upto now_running) of node_local are at the same offset for any version.
	 *
	 * Structure ----> node_local <----    size 59392 [0xe800]
	 *
	 *	offset = 0000 [0x0000]      size = 0012 [0x000c]    ----> node_local.label
	 *	offset = 0012 [0x000c]      size = 0256 [0x0100]    ----> node_local.fname
	 *	offset = 0268 [0x010c]      size = 0036 [0x0024]    ----> node_local.now_running
	 *
	 * This is so that the VERMISMATCH error can be successfully detected in db_init/mu_rndwn_file
	 *	and so that the db-file-name can be successfully obtained from orphaned shm by mu_rndwn_all.
	 */
 	assert(0 == OFFSETOF(node_local, label[0]));
 	assert(12 == SIZEOF(((node_local *)NULL)->label));
 	assert(12 == GDS_LABEL_SZ);
	assert(12 == OFFSETOF(node_local, fname[0]));
 	assert(256 == SIZEOF(((node_local *)NULL)->fname));
 	assert(256 == (MAX_FN_LEN + 1));
	assert(268 == OFFSETOF(node_local, now_running[0]));
 	assert(36 == SIZEOF(((node_local *)NULL)->now_running));
	assert(36 == MAX_REL_NAME);
	for (loopcnt = 0; loopcnt < MAX_DBFILOPN_RETRY_CNT; loopcnt++)
	{
		prev_reg = dbfilopn(reg);
		if (prev_reg != reg)
		{	/* (gd_region *)-1 == prev_reg => cm region open attempted */
			if (NULL == prev_reg || (gd_region *)-1L == prev_reg)
				return;
			/* Found same database already open - prev_reg contains addr of originally opened region */
			FILE_CNTL(reg) = FILE_CNTL(prev_reg);
			memcpy(reg->dyn.addr->fname, prev_reg->dyn.addr->fname, prev_reg->dyn.addr->fname_len);
			reg->dyn.addr->fname_len = prev_reg->dyn.addr->fname_len;
			csa = (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs;
			if (NULL == csa->gvt_hashtab)
				gvt_hashtab_init(csa);	/* populate csa->gvt_hashtab; needed BEFORE PROCESS_GVT_PENDING_LIST */
			PROCESS_GVT_PENDING_LIST(reg, csa);
			csd = csa->hdr;
			reg->max_rec_size = csd->max_rec_size;
			reg->max_key_size = csd->max_key_size;
			reg->null_subs = csd->null_subs;
			reg->std_null_coll = csd->std_null_coll;
			reg->jnl_state = csd->jnl_state;
			reg->jnl_file_len = csd->jnl_file_len;		/* journal file name length */
			memcpy(reg->jnl_file_name, csd->jnl_file_name, reg->jnl_file_len);	/* journal file name */
			reg->jnl_alq = csd->jnl_alq;
			reg->jnl_deq = csd->jnl_deq;
			reg->jnl_buffer_size = csd->jnl_buffer_size;
			reg->jnl_before_image = csd->jnl_before_image;
			reg->dyn.addr->asyncio = csd->asyncio;
			reg->dyn.addr->read_only = csd->read_only;
			assert(csa->reservedDBFlags == csd->reservedDBFlags);	/* Should be same already */
			SYNC_RESERVEDDBFLAGS_REG_CSA_CSD(reg, csa, csd, ((node_local_ptr_t)NULL));
			SET_REGION_OPEN_TRUE(reg, WAS_OPEN_TRUE);
			assert(1 <= csa->regcnt);
			csa->regcnt++;	/* Increment # of regions that point to this csa */
			return;
		}
		/* Note that if we are opening a statsDB, it is possible baseDBreg->was_open is TRUE at this point. */
		reg->was_open = FALSE;
		/* We shouldn't have crit on any region unless we are in TP and in the final retry or we are in mupip_set_journal
		 * trying to switch journals across all regions. WBTEST_HOLD_CRIT_ENABLED is an exception because it exercises a
		 * deadlock situation so it needs to hold multiple crits at the same time. Currently, there is no fine-granular
		 * checking for mupip_set_journal, hence a coarse MUPIP_IMAGE check for image_type.
		 */
		assert(dollar_tlevel && (CDB_STAGNATE <= t_tries) || IS_MUPIP_IMAGE || (0 == have_crit(CRIT_HAVE_ANY_REG))
		       || WBTEST_ENABLED(WBTEST_HOLD_CRIT_ENABLED));
		if (dollar_tlevel && (0 != have_crit(CRIT_HAVE_ANY_REG)))
		{	/* To avoid deadlocks with currently holding crits and the DLM lock request to be done in "db_init",
			 * we should insert this region in the tp_reg_list and tp_restart should do the gvcst_init after
			 * having released crit on all regions.
			 */
			insert_region(reg, &tp_reg_list, &tp_reg_free_list, SIZEOF(tp_region));
			t_retry(cdb_sc_needcrit);
			assert(FALSE);	/* should never reach here since t_retry should have unwound the M-stack and restarted TP */
		}
		csa = (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs;
#		ifdef NOLICENSE
		licensed = TRUE;
#		else
		CRYPT_CHKSYSTEM;
#		endif
		csa->hdr = NULL;
		csa->nl = NULL;
		csa->jnl = NULL;
		csa->gbuff_limit = 0;
		csa->our_midnite = NULL;
		csa->our_lru_cache_rec_off = 0;
		csa->persistent_freeze = FALSE;	/* want secshr_db_clnup() to clear an incomplete freeze/unfreeze codepath */
		csa->regcnt = 1;	/* At this point, only one region points to this csa */
		csa->db_addrs[0] = csa->db_addrs[1] = NULL;
		csa->lock_addrs[0] = csa->lock_addrs[1] = NULL;
		csa->gd_ptr = addr ? addr : gd_header;
		if (csa->gd_ptr)
			csa->gd_instinfo = csa->gd_ptr->instinfo;
		if ((IS_GTM_IMAGE || !pool_init) && jnlpool_init_needed && CUSTOM_ERRORS_AVAILABLE && REPL_ALLOWED(csa)
				&& REPL_INST_AVAILABLE(addr))
		{
			replpool_valid = TRUE;
			jnlpool_init(GTMRELAXED, (boolean_t)FALSE, (boolean_t *)NULL, addr);
			status = filename_to_id(&replfile_gdid, replpool_id.instfilename);	/* set by REPL_INST_AVAILABLE */
			replfilegdid_valid = (SS_NORMAL == status);
		} else
			replpool_valid = replfilegdid_valid = FALSE;
		/* Any LSEEKWRITEs hence forth will wait if the instance is frozen. To aid in printing the region information before
		 * and after the wait, csa->region is referenced. Since it is NULL at this point, set it to reg. This is a safe
		 * thing to do since csa->region is anyways set in db_common_init (few lines below).
		 */
		csa->region = reg;
		/* Protect the db_init and the code below until we set reg->open to TRUE. This is needed as otherwise,
		 * if a MUPIP STOP is issued to this process at a time-window when db_init is completed but reg->open
		 * is NOT set to TRUE, will cause gds_rundown NOT to clean up the shared memory created by db_init and
		 * thus would be left over in the system.
		 */
		DEFER_INTERRUPTS(INTRPT_IN_GVCST_INIT, prev_intrpt_state);
		assert(INTRPT_OK_TO_INTERRUPT == prev_intrpt_state);	/* relied upon by ENABLE_INTERRUPTS
									 * in dbinit_ch and gvcst_init_autoDB_ch
									 */
		/* Do a loop of "db_init". This is to account for the fact that "db_init" can return an error in some cases.
		 * e.g. If DSE does "db_init" first time with OK_TO_BYPASS_TRUE and somewhere in the middle of "db_init" it
		 * notices that a concurrent mumps process has deleted the semid/shmid since it was the last process attached
		 * to the db. In this case we retry the "db_init" a few times and the last time we retry with OK_TO_BYPASS_FALSE
		 * even if this is DSE.
		 */
		db_init_retry = 0;
		GTM_WHITE_BOX_TEST(WBTEST_HOLD_FTOK_UNTIL_BYPASS, db_init_retry, MAX_DBINIT_RETRY);
		do
		{
			db_init_ret = db_init(reg, (MAX_DBINIT_RETRY == db_init_retry) ? OK_TO_BYPASS_FALSE : OK_TO_BYPASS_TRUE);
			if (0 == db_init_ret)
				break;
			if (-1 == db_init_ret)
			{
				assert(MAX_DBINIT_RETRY > db_init_retry);
				retry_dbinit = TRUE;
			} else
			{	/* Set "retry_dbinit" to FALSE that way "db_init_err_cleanup" call below takes care of
				 * closing udi->fd opened in "dbfilopn" call early in this iteration. This avoids fd leak
				 * since another call to "dbfilopn" in the next iteration would not know about the previous fd.
				 */
				retry_dbinit = FALSE;
			}
			db_init_err_cleanup(retry_dbinit);
			if (!retry_dbinit)
				break;
		} while (MAX_DBINIT_RETRY >= ++db_init_retry);
		assert(-1 != db_init_ret);
		if (0 == db_init_ret)
			break;
		if (ERR_DBGLDMISMATCH == db_init_ret)
		{	/* "db_init" would have adjusted seg->asyncio to reflect the db file header's asyncio settings.
			 * Retry but do not count this try towards the total tries as otherwise it is theoretically possible
			 * for the db fileheader to be recreated with just the opposite asyncio setting in each try and
			 * we might eventually error out when "loopcnt" is exhausted. Not counting this try implies we
			 * might loop indefinitely but the chances are infinitesimally small. And since this error is never
			 * issued, the user should never expect to see this and hence this error is not documented.
			 */
			loopcnt--;
		}
	}
	if (db_init_ret)
	{
		assert(FALSE);	/* we don't know of a practical way to get errors in each of the for-loop attempts above */
		/* "db_init" returned with an unexpected error. Issue a generic error to note this out-of-design state */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REGOPENFAIL, 4, REG_LEN_STR(reg), DB_LEN_STR(reg));
	}
	/* At this point, we have initialized the database, but haven't yet set reg->open to TRUE. If any rts_errors happen in
	 * the meantime, there are no condition handlers established to handle the rts_error. More importantly, it is non-trivial
	 * to add logic to such a condition handler to undo the effects of db_init. Also, in some cases, the rts_error can can
	 * confuse future calls of db_init. By invoking DBG_MARK_RTS_ERROR_UNUSABLE, we can catch any rts_errors in future and
	 * eliminate it on a case by case basis.
	 */
	DBG_MARK_RTS_ERROR_UNUSABLE;
	crash_count = csa->critical->crashcnt;
	csa->regnum = ++region_open_count;
	csd = csa->hdr;
#	ifdef GTM_TRIGGER
	/* Take copy of db trigger cycle into csa at db startup. Any concurrent changes to the
	 * db trigger cycle (by MUPIP TRIGGER) will be detected at tcommit (t_end/tp_tend) time.
	 */
	csa->db_trigger_cycle = csd->db_trigger_cycle;
#	endif
	/* set csd and fill in selected fields */
	assert(REG_ACC_METH(reg) == csd->acc_meth); /* db_init should have made sure this assert holds good */
	reg_acc_meth = csd->acc_meth;
	/* It is necessary that we do the pending gv_target list reallocation BEFORE db_common_init as the latter resets
	 * reg->max_key_size to be equal to the csd->max_key_size and hence process_gvt_pending_list might wrongly conclude
	 * that NO reallocation (since it checks reg->max_key_size with csd->max_key_size) is needed when in fact a
	 * reallocation might be necessary (if the user changed max_key_size AFTER database creation)
	 */
	PROCESS_GVT_PENDING_LIST(reg, csa);
	db_common_init(reg, csa, csd);	/* do initialization common to db_init() and mu_rndwn_file() */
	/* If we are not fully upgraded, see if we need to send a warning to the operator console about
	   performance. Compatibility mode is a known performance drain. Actually, we can send one of two
	   messages. If the desired_db_format is for an earlier release than the current release, we send
	   a performance warning that this mode degrades performance. However, if the desired_db_format is
	   for the current version but there are blocks to convert still, we send a gengle reminder that
	   running mupip reorg upgrade would be a good idea to get the full performance benefit of V5.
	*/
	time(&curr_time);
	assert(MAXUINT4 > curr_time);
	curr_time_uint4 = (uint4)curr_time;
	next_warn_uint4 = csd->next_upgrd_warn.cas_time;
	if (!csd->fully_upgraded && curr_time_uint4 > next_warn_uint4
	    && COMPSWAP_LOCK(&csd->next_upgrd_warn.time_latch, next_warn_uint4, 0, (curr_time_uint4 + UPGRD_WARN_INTERVAL), 0))
	{	/* The msg is due and we have successfully updated the next time interval */
		if (GDSVCURR != csd->desired_db_format)
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBVERPERFWARN1, 2, DB_LEN_STR(reg));
		else
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBVERPERFWARN2, 2, DB_LEN_STR(reg));
	}
	csa->lock_crit_with_db = csd->lock_crit_with_db;	/* put a copy in csa where it's cheaper for mlk* to access */
	/* Compute the maximum journal space requirements for a PBLK (including possible ALIGN record).
	 * Use this variable in the TOTAL_TPJNL_REC_SIZE and TOTAL_NONTP_JNL_REC_SIZE macros instead of recomputing.
	 */
	csa->pblk_align_jrecsize = (int4)MIN_PBLK_RECLEN + csd->blk_size + (int4)MIN_ALIGN_RECLEN;
	segment_update_array_size = UA_SIZE(csd);
	assert(!reg->was_open);
	SET_REGION_OPEN_TRUE(reg, WAS_OPEN_FALSE);
	GTM_FD_TRACE_ONLY(gtm_dbjnl_dupfd_check());	/* check if any of db or jnl fds collide (D9I11-002714) */
	if (NULL != csa->dir_tree)
	{	/* It is possible that dir_tree has already been targ_alloc'ed. This is because GT.CM or VMS DAL
		 * calls can run down regions without the process halting out. We don't want to double malloc.
		 */
		csa->dir_tree->clue.end = 0;
	}
	SET_CSA_DIR_TREE(csa, reg->max_key_size, reg);
	/* Now that reg->open is set to TRUE and directory tree is initialized, go ahead and set rts_error back to being usable */
	DBG_MARK_RTS_ERROR_USABLE;
	/* Do the deferred encryption initialization now in case it needs to issue an rts_error */
	INIT_DEFERRED_DB_ENCRYPTION_IF_NEEDED(reg, csa, csd);
	/* gds_rundown if invoked from now on will take care of cleaning up the shared memory segment */
	/* The below code, until the ENABLE_INTERRUPTS(INTRPT_IN_GVCST_INIT, prev_intrpt_state), can do mallocs which in turn
	 * can issue a GTM-E-MEMORY error which would invoke rts_error. Hence these have to be done AFTER the
	 * DBG_MARK_RTS_ERROR_USABLE call. Since these are only private memory initializations, it is safe to
	 * do these after reg->open is set. Any rts_errors from now on still do the needful cleanup of shared memory in
	 * gds_rundown since reg->open is already TRUE.
	 */
	if (first_ua == NULL)
	{	/* first open of first database - establish an update array system */
		assert(update_array == NULL);
		assert(update_array_ptr == NULL);
		assert(update_array_size == 0);
		tmp_ua = (ua_list *)malloc(SIZEOF(ua_list));
		memset(tmp_ua, 0, SIZEOF(ua_list));	/* initialize tmp_ua->update_array and tmp_ua->next_ua to NULL */
		tmp_ua->update_array = (char *)malloc(segment_update_array_size);
		tmp_ua->update_array_size = segment_update_array_size;
		/* assign global variables only after malloc() succeeds */
		update_array_size = cumul_update_array_size = segment_update_array_size;
		update_array = update_array_ptr = tmp_ua->update_array;
		first_ua = curr_ua = tmp_ua;
	} else
	{	/* there's already an update_array system in place */
		assert(update_array != NULL);
		assert(update_array_size != 0);
		if (!dollar_tlevel && segment_update_array_size > first_ua->update_array_size)
		{
			/* no transaction in progress and the current array is too small - replace it */
			assert(first_ua->update_array == update_array);
			assert(first_ua->update_array_size == update_array_size);
			assert(first_ua->next_ua == NULL);
			tmp_ua = first_ua;
			first_ua = curr_ua = NULL;
			free(update_array);
			tmp_ua->update_array = update_array = update_array_ptr = NULL;
			tmp_ua->update_array = (char *)malloc(segment_update_array_size);
			tmp_ua->update_array_size = segment_update_array_size;
			/* assign global variables only after malloc() succeeds */
			update_array_size = cumul_update_array_size = segment_update_array_size;
			update_array = update_array_ptr = tmp_ua->update_array;
			first_ua = curr_ua = tmp_ua;
		}
	}
	assert(global_tlvl_info_list || !csa->sgm_info_ptr);
	if (JNL_ALLOWED(csa))
	{
		bsize = csd->blk_size;
		realloc_alt_buff = FALSE;
		if (NULL == non_tp_jfb_ptr)
		{
			non_tp_jfb_ptr = (jnl_format_buffer *)malloc(SIZEOF(jnl_format_buffer));
			non_tp_jfb_ptr->hi_water_bsize = bsize;
			non_tp_jfb_ptr->buff = (char *)malloc(MAX_NONTP_JNL_REC_SIZE(bsize));
			non_tp_jfb_ptr->record_size = 0;	/* initialize it to 0 since TOTAL_NONTPJNL_REC_SIZE macro uses it */
			non_tp_jfb_ptr->alt_buff = NULL;
			assert(NULL == TREF(nontp_jbuf_rsrv));
			ALLOC_JBUF_RSRV_STRUCT(nontp_jbuf_rsrv_lcl, csa);
			TREF(nontp_jbuf_rsrv) = nontp_jbuf_rsrv_lcl;
		} else if (bsize > non_tp_jfb_ptr->hi_water_bsize)
		{	/* Need a larger buffer to accommodate larger non-TP journal records */
			non_tp_jfb_ptr->hi_water_bsize = bsize;
			free(non_tp_jfb_ptr->buff);
			non_tp_jfb_ptr->buff = (char *)malloc(MAX_NONTP_JNL_REC_SIZE(bsize));
			if (NULL != non_tp_jfb_ptr->alt_buff)
			{
				free(non_tp_jfb_ptr->alt_buff);
				realloc_alt_buff = TRUE;
			}
			assert(NULL != TREF(nontp_jbuf_rsrv));
		}
		/* If the journal records need to be encrypted in the journal file and if replication is in use, we will need access
		 * to both the encrypted (for the journal file) and unencrypted (for the journal pool) journal record contents.
		 * Allocate an alternative buffer if any open journaled region is encrypted.
		 */
		if (realloc_alt_buff || (USES_ENCRYPTION(csd->is_encrypted) && (NULL == non_tp_jfb_ptr->alt_buff)))
			non_tp_jfb_ptr->alt_buff = (char *)malloc(MAX_NONTP_JNL_REC_SIZE(non_tp_jfb_ptr->hi_water_bsize));
		/* csa->min_total_tpjnl_rec_size represents the minimum journal buffer space needed for a TP transaction.
		 * It is a conservative estimate assuming that one ALIGN record and one PINI record will be written for
		 * one set of fixed size jnl records written.
		 * si->total_jnl_rec_size is initialized/reinitialized  to this value here and in tp_clean_up().
		 * The purpose of this field is to avoid recomputation of the variable in tp_clean_up().
		 * In addition to this, space requirements for whatever journal records get formatted as part of
		 * jnl_format() need to be taken into account.
		 * This is done in jnl_format() where si->total_jnl_rec_size is appropriately incremented.
		 */
		csa->min_total_tpjnl_rec_size = PINI_RECLEN + TCOM_RECLEN + MIN_ALIGN_RECLEN;
		/* Similarly csa->min_total_nontpjnl_rec_size represents the minimum journal buffer space needed
		 * for a non-TP transaction.
		 * It is a conservative estimate assuming that one ALIGN record and one PINI record will be written for
		 * one set of fixed size jnl records written.
		 */
		csa->min_total_nontpjnl_rec_size = PINI_RECLEN + MIN_ALIGN_RECLEN;
	}
	if (tp_in_use || !IS_GTM_IMAGE)
		gvcst_tp_init(reg);	/* Initialize TP structures, else postpone till TP is used (only if GTM) */
	if (!global_tlvl_info_list)
	{
		global_tlvl_info_list = (buddy_list *)malloc(SIZEOF(buddy_list));
		initialize_list(global_tlvl_info_list, SIZEOF(global_tlvl_info), GBL_TLVL_INFO_LIST_INIT_ALLOC);
	}
	ENABLE_INTERRUPTS(INTRPT_IN_GVCST_INIT, prev_intrpt_state);
	if (dba_bg == reg_acc_meth)
	{	/* Check if (a) this region has non-upgraded blocks and if so, (b) the reformat buffer exists and
		 * (c) if it is big enough to deal with this region. If the region does not have any non-upgraded
		 * block (blks_to_upgrd is 0) we will not allocate the buffer at this time. Note that this opens up
		 * a small window for errors. If this buffer is not allocated and someone turns on compatibility
		 * mode and before the process can discover this and allocate the buffer, it runs out of memory,
		 * errors out and finds it is responsible for running down the database, it could fail on a recursive
		 * memory error when it tries to allocate the block. This is (to me) an acceptable risk as it is
		 * very low and compares favorably to the cost of every process allocating a database block sized
		 * chunk of private storage that will be seldom if ever used (SE 3/2005).
		 */
		if (0 != csd->blks_to_upgrd && csd->blk_size > reformat_buffer_len)
		{	/* Buffer not big enough (or does not exist) .. get a new one releasing old if it exists */
			assert(0 == fast_lock_count);	/* this is mainline (non-interrupt) code */
			++fast_lock_count;		/* No interrupts across this use of reformat_buffer */
			/* reformat_buffer_in_use should always be incremented only AFTER incrementing fast_lock_count
			 * as it is the latter that prevents interrupts from using the reformat buffer. Similarly
			 * the decrement of fast_lock_count should be done AFTER decrementing reformat_buffer_in_use.
			 */
			assert(0 == reformat_buffer_in_use);
			DEBUG_ONLY(reformat_buffer_in_use++);
			if (reformat_buffer)
				free(reformat_buffer);	/* Different blksized databases in use .. keep only largest one */
			reformat_buffer = malloc(csd->blk_size);
			reformat_buffer_len = csd->blk_size;
			DEBUG_ONLY(reformat_buffer_in_use--);
			assert(0 == reformat_buffer_in_use);
			--fast_lock_count;
		}
		assert(MV_STR & (TREF(gbuff_limit)).mvtype);
		if (mu_reorg_process && (0 == (TREF(gbuff_limit)).str.len))
		{	/* if the environment variable wasn't supplied, use the default for REORG */
			(TREF(gbuff_limit)).str.len = SIZEOF(REORG_GBUFF_LIMIT);
			(TREF(gbuff_limit)).str.addr = malloc(SIZEOF(REORG_GBUFF_LIMIT));
			memcpy((TREF(gbuff_limit)).str.addr, REORG_GBUFF_LIMIT, SIZEOF(REORG_GBUFF_LIMIT));
		}
		if ((mu_reorg_process DEBUG_ONLY(|| IS_GTM_IMAGE)) && (0 != (TREF(gbuff_limit)).str.len))
		{	/* if reorg or dbg apply env var */
			set_gbuff_limit(&csa, &csd, &(TREF(gbuff_limit)));
#			ifdef DEBUG
			if ((process_id & 2) && (process_id & (csd->n_bts - 1)))		/* also randomize our_midnite */
			{
				csa->our_midnite = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
				csa->our_midnite += (process_id & (csd->n_bts - 1));
				assert((csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets + csd->n_bts)
				       > csa->our_midnite);
				cr = csa->our_midnite - csa->gbuff_limit;
				if (cr < csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets)
					cr += csd->n_bts;
				csa->our_lru_cache_rec_off = GDS_ANY_ABS2REL(csa, cr);
			}
			assert((csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets) < csa->our_midnite);
#			endif
		}
	}
	if (IS_ACC_METH_BG_OR_MM(reg_acc_meth))
	{
		/* Determine fid_index of current region's file_id across sorted file_ids of all regions open until now.
		 * All regions which have a file_id lesser than that of current region will have no change to their fid_index
		 * All regions which have a file_id greater than that of current region will have their fid_index incremented by 1
		 * The fid_index determination algorithm below has an optimization in that if the current region's file_id is
		 * determined to be greater than that of a particular region, then all regions whose fid_index is lesser
		 * than that particular region's fid_index are guaranteed to have a lesser file_id than the current region
		 * so we do not compare those against the current region's file_id.
		 * Note that the sorting is done only on DB/MM regions. GT.CM/DDP regions should not be part of TP transactions,
		 * hence they will not be sorted.
		 */
		prevcsa = NULL;
		reg_fid = &(csa->nl->unique_id);
		max_fid_index = 1;
		for (regcsa = cs_addrs_list; NULL != regcsa; regcsa = regcsa->next_csa)
		{
			onln_rlbk_cycle_mismatch |= (regcsa->db_onln_rlbkd_cycle != regcsa->nl->db_onln_rlbkd_cycle);
			if ((NULL != prevcsa) && (regcsa->fid_index < prevcsa->fid_index))
				continue;
			tmp_reg_fid = &((regcsa)->nl->unique_id);
			if (0 < gdid_cmp(&(reg_fid->uid), &(tmp_reg_fid->uid)))
			{
				if ((NULL == prevcsa) || (regcsa->fid_index > prevcsa->fid_index))
					prevcsa = regcsa;
			} else
			{
				regcsa->fid_index++;
				max_fid_index = MAX(max_fid_index, regcsa->fid_index);
			}
		}
		if (NULL == prevcsa)
			csa->fid_index = 1;
		else
		{
			csa->fid_index = prevcsa->fid_index + 1;
			max_fid_index = MAX(max_fid_index, csa->fid_index);
		}
		if (onln_rlbk_cycle_mismatch)
		{
			csa->root_search_cycle--;
			if (REPL_ALLOWED(csd))
			{
				csa->onln_rlbk_cycle--;
				csa->db_onln_rlbkd_cycle--;
			}
		}
		/* Add current csa into list of open csas */
		csa->next_csa = cs_addrs_list;
		cs_addrs_list = csa;
		/* Also update tp_reg_list fid_index's as insert_region relies on it */
		for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
			tr->file.fid_index = (&FILE_INFO(tr->reg)->s_addrs)->fid_index;
		DBG_CHECK_TP_REG_LIST_SORTING(tp_reg_list);
		TREF(max_fid_index) = max_fid_index;
	}
	if (pool_init && REPL_ALLOWED(csd) && jnlpool_init_needed)
	{
		/* Last parameter to VALIDATE_INITIALIZED_JNLPOOL is TRUE if the process does logical updates and FALSE otherwise.
		 * This parameter governs whether the macro can do SCNDDBNOUPD check or not. All the utilities that sets
		 * jnlpool_init_needed global variable don't do logical updates (REORG, EXTEND, etc.). But, for GT.M,
		 * jnlpool_init_needed is set to TRUE unconditionally. Even though GT.M can do logical updates, we pass FALSE
		 * unconditionally to the macro (indicating no logical updates). This is because, at this point, there is no way to
		 * tell if this process wants to open the database for read or write operation. If it is for a read operation, we
		 * don't want the below macro to issue SCNDDBNOUPD error. If it is for write operation, we will skip the
		 * SCNDDBNOUPD error message here. But, eventually when this process goes to gvcst_{put,kill} or op_ztrigger,
		 * SCNDDBNOUPD is issued.
		 */
		save_jnlpool = jnlpool;
		if (IS_GTM_IMAGE)
		{	/* csa->jnlpool not set until validate jnlpool so need to compare gd_id of instance file */
			if (!replfilegdid_valid)
			{
				assertpro(replpool_valid || REPL_INST_AVAILABLE(addr));	/* if any pool inited this should succeed */
				status = filename_to_id(&replfile_gdid, replpool_id.instfilename);
				replfilegdid_valid = (SS_NORMAL == status);
			}
			if (replfilegdid_valid)
			{
				if (jnlpool && jnlpool->pool_init)
				{
					tmp_gdid = &FILE_ID(jnlpool->jnlpool_dummy_reg);
					if (!gdid_cmp(tmp_gdid, &replfile_gdid))
						jnlpool_found = TRUE;
				}
				if (!jnlpool_found)
					for (jnlpool = jnlpool_head; jnlpool; jnlpool = jnlpool->next)
						if (jnlpool->pool_init)
						{
							tmp_gdid = &FILE_ID(jnlpool->jnlpool_dummy_reg);
							if (!gdid_cmp(tmp_gdid, &replfile_gdid))
							{
								jnlpool_found = TRUE;
								break;
							}
						}
				if (!jnlpool_found)
				{
					jnlpool_init(GTMRELAXED, (boolean_t)FALSE, (boolean_t *)NULL, addr);
					tmp_gdid = &FILE_ID(jnlpool->jnlpool_dummy_reg);
					if (!gdid_cmp(tmp_gdid, &replfile_gdid))
						jnlpool_found = TRUE;
				}
			}
		} else
		{
			assertpro(jnlpool);	/* only one for utilities */
			jnlpool_found = TRUE;
		}
		if (jnlpool_found)
			VALIDATE_INITIALIZED_JNLPOOL(csa, csa->nl, reg, GTMRELAXED, SCNDDBNOUPD_CHECK_FALSE);
		if (save_jnlpool != jnlpool)
			jnlpool = save_jnlpool;
	}
	/* At this point, the database is officially open but one of two condition can exist here where we need to do more work:
	 *   1. This was a normal database open and this process has opted-in for global shared stats so we need to also open
	 *      the associated statsDB (IF the baseDB does not have the NOSTATS qualifier).
	 *   2. We are opening a statsDB in which case we are guaranteed the baseDB has already been open (assert at start of
	 *	this function). The baseDB open would have done the needed initialization for the statsDB in a lot of cases.
	 *	But in case it did not (e.g. TREF(statshare_opted_in) is FALSE), keep the statsDB open read-only until the
	 *	initialization happens later (when TREF(statshare_opted_in) becomes TRUE again).
	 * Note since this database is officially opened and has gone through db_init(), the current reservedDBFlags in all of
	 * region, sgmnt_addrs, and sgmnt_data are sync'd to the value that was in sgmnt_data (fileheader).
	 */
	if (!IS_DSE_IMAGE)
	{	/* DSE does not open statsdb automatically. It does it only when asked to */
		if (TREF(statshare_opted_in))
		{
			if (!is_statsDB)
			{	/* This is a baseDB - so long as NOSTATS is *not* turned on, we should initialize the statsDB */
				if (!(RDBF_NOSTATS & reg->reservedDBFlags))
				{
					BASEDBREG_TO_STATSDBREG(reg, statsDBreg);
					assert(NULL != statsDBreg);
					if (!statsDBreg->statsDB_setup_started)
					{       /* The associated statsDB may or may not be open but it hasn't been initialized
						 * so take care of that now. Put a handler around it so if the open of the statsDB
						 * file fails for whatever reason, we can ignore it and just keep going.
						 */
						statsDBreg->statsDB_setup_started = TRUE;
						gvcst_init_statsDB(reg, DO_STATSDB_INIT_TRUE);
					}
				}
			}
		} else if (is_statsDB)
		{	/* We are opening a statsDB file but not as a statsDB (i.e. not opted-in) but we still need to set the
			 * database to R/O mode so it is only updated by GTM's C code and never in M mode which could lead to
			 * too-small records being added that would cause the gvstats records to move which must never happen.
			 * For DSE, we want it to open the db R/W (in case we need some repair of the statsdb).
			 * So skip this R/O setting for DSE.
			 */
			assert(!reg->was_open);
			reg->read_only = TRUE;
			csa->read_write = FALSE;	/* Maintain read_only/read_write in parallel */
		}
	}
	return;
}
