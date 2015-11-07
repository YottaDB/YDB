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

#include <stddef.h>		/* for offsetof macro */

#include "gtm_string.h"
#include "gtm_time.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdskill.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "gtm_stdlib.h"		/* for ATOI */
#include "cryptdef.h"
#include "mlkdef.h"
#include "error.h"
#include "gt_timer.h"
#include "gtmimagename.h"
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
#include "hashtab_mname.h"
#include "process_gvt_pending_list.h"
#include "gtmmsg.h"
#ifdef UNIX
#include "heartbeat_timer.h"
#include "anticipatory_freeze.h"
#include "wbox_test_init.h"

#define MAX_DBINIT_RETRY	4
#endif

#ifdef	GTM_FD_TRACE
#include "gtm_dbjnl_dupfd_check.h"
#endif

GBLREF	gd_region		*gv_cur_region, *db_init_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_addrs		*cs_addrs_list;
GBLREF	boolean_t		gtcm_connection;
GBLREF	bool			licensed;
GBLREF	int4			lkid;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	uint4			update_array_size, cumul_update_array_size;
GBLREF	ua_list			*first_ua, *curr_ua;
GBLREF	short			crash_count;
GBLREF	uint4			dollar_tlevel;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	boolean_t		mupip_jnl_recover;
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
GBLREF	gvt_container		*gvt_pending_list;
GBLREF	boolean_t		dse_running;
GBLREF	jnl_gbls_t		jgbl;
#ifdef UNIX
GBLREF	boolean_t		pool_init;
GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	jnlpool_addrs		jnlpool;
#endif

LITREF char			gtm_release_name[];
LITREF int4			gtm_release_name_len;

error_def(ERR_BADDBVER);
error_def(ERR_DBCREINCOMP);
error_def(ERR_DBFLCORRP);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBVERPERFWARN1);
error_def(ERR_DBVERPERFWARN2);
error_def(ERR_MMNODYNUPGRD);
error_def(ERR_REGOPENFAIL);

void	assert_jrec_member_offsets(void)
{
	assert(REAL_JNL_HDR_LEN % DISK_BLOCK_SIZE == 0);
	assert(JNL_HDR_LEN % DISK_BLOCK_SIZE == 0);
	/* We currently assume that the journal file header size is aligned relative to the filesystem block size.
	 * which is currently assumed to be a 2-power (e.g. 512 bytes, 1K, 2K, 4K etc.) but never more than 64K
	 * (MAX_IO_BLOCK_SIZE). Given this, we keep the journal file header size at 64K for Unix and 512-byte aligned
	 * for VMS. This way any process updating the file header will hold crit and do aligned writes. Any process
	 * writing the journal file data (journal records) on disk will hold the qio lock and can safely do so without
	 * ever touching the journal file header area. If ever MAX_IO_BLOCK_SIZE changes (say because some filesystem
	 * block size changes to 128K) such that JNL_HDR_LEN is no longer aligned to that, we want to know hence this assert.
	 */
	assert(JNL_HDR_LEN % MAX_IO_BLOCK_SIZE == 0);
	assert(REAL_JNL_HDR_LEN == SIZEOF(jnl_file_header));
	UNIX_ONLY(assert(REAL_JNL_HDR_LEN <= JNL_HDR_LEN);)
	VMS_ONLY(assert(REAL_JNL_HDR_LEN == JNL_HDR_LEN);)
	assert(JNL_HDR_LEN == JNL_FILE_FIRST_RECORD);
	assert(DISK_BLOCK_SIZE >= PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN);
	assert((JNL_ALLOC_MIN * DISK_BLOCK_SIZE) > JNL_HDR_LEN);
	/* Following assert is for JNL_FILE_TAIL_PRESERVE macro in tp.h */
	assert(PINI_RECLEN >= EPOCH_RECLEN && PINI_RECLEN >= PFIN_RECLEN && PINI_RECLEN >= EOF_RECLEN);
	/* jnl_string structure has a 8-bit nodeflags field and a 24-bit length field. In some cases, this is
	 * used as a 32-bit length field (e.g. in the value part of the SET record or ZTWORMHOLE record). These
	 * usages treat the 32-bits as a jnl_str_len_t type and access it directly. Hence the requirement that
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

	/* Make sure all strm_seqno fields start at same offset. Lot of modules rely on this */
	assert(offsetof(struct_jrec_null, strm_seqno) == offsetof(struct_jrec_upd, strm_seqno));
	assert(offsetof(struct_jrec_null, strm_seqno) == offsetof(struct_jrec_tcom, strm_seqno));
	assert(offsetof(struct_jrec_null, strm_seqno) == offsetof(struct_jrec_ztworm, strm_seqno));
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
	/* Assert following comment which is relied upon in JNL_FILE_TAIL_PRESERVE macro.
	 * 	"We know PINI_RECLEN is maximum of EPOCH_RECLEN, PFIN_RECLEN, EOF_RECLEN"
	 */
	assert(PINI_RECLEN > EPOCH_RECLEN);
	assert(PINI_RECLEN > PFIN_RECLEN);
	assert(PINI_RECLEN > EOF_RECLEN);
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

void gvcst_init(gd_region *greg)
{
	sgmnt_addrs		*csa, *prevcsa, *regcsa;
	sgmnt_data_ptr_t	csd;
#	ifdef VMS
	char			cs_data_buff[ROUND_UP(SGMNT_HDR_LEN, DISK_BLOCK_SIZE)];
	sgmnt_data_ptr_t	temp_cs_data;
#	endif
	uint4			segment_update_array_size;
	int4			bsize;
	boolean_t		realloc_alt_buff;
	file_control		*fc;
	gd_region		*prev_reg, *reg_top;
#	ifdef DEBUG
	cache_rec_ptr_t		cr;
	bt_rec_ptr_t		bt;
	blk_ident		tmp_blk;
#	endif
	mstr			log_nam, trans_log_nam;
	char			trans_buff[MAX_FN_LEN + 1];
	unique_file_id		*greg_fid, *reg_fid;
	gd_addr			*addr_ptr;
	tp_region		*tr;
	ua_list			*tmp_ua;
	time_t			curr_time;
	uint4			curr_time_uint4, next_warn_uint4;
	unsigned int            minus1 = (unsigned)-1;
	enum db_acc_method	greg_acc_meth;
	ht_ent_mname		*tabent, *topent, *stayent;
	gv_namehead		*gvt, *gvt_stay;
	gvnh_reg_t		*gvnh_reg;
	hash_table_mname	*table;
	boolean_t		added, first_wasopen, onln_rlbk_cycle_mismatch = FALSE;
	intrpt_state_t		save_intrpt_ok_state;
#	ifdef UNIX
	replpool_identifier	replpool_id;
	unsigned int		full_len;
	int4			db_init_retry;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	UNSUPPORTED_PLATFORM_CHECK;
	assert(!jgbl.forw_phase_recovery);
	CWS_INIT;	/* initialize the cw_stagnate hash-table */
	/* check the header design assumptions */
	assert(SIZEOF(th_rec) == (SIZEOF(bt_rec) - SIZEOF(bt->blkque)));
	assert(SIZEOF(cache_rec) == (SIZEOF(cache_state_rec) + SIZEOF(cr->blkque)));
	DEBUG_ONLY(assert_jrec_member_offsets();)
	assert(MAX_DB_BLK_SIZE < (1 << NEXT_OFF_MAX_BITS));	/* Ensure a off_chain record's next_off member
								 * can work with all possible block sizes */
        set_num_additional_processors();
	DEBUG_ONLY(
		/* Note that the "block" member in the blk_ident structure in gdskill.h has 30 bits.
		 * Currently, the maximum number of blocks is 2**30. If ever this increases, something
		 * has to be correspondingly done to the "block" member to increase its capacity.
		 * The following assert checks that we always have space in the "block" member
		 * to represent a GDS block number.
		 */
		tmp_blk.block = minus1;
		assert(MAXTOTALBLKS_MAX - 1 <= tmp_blk.block);
	)
	/* TH_BLOCK is currently a hardcoded constant as basing it on the offsetof macro does not work with the VMS compiler.
	 * Therefore assert that TH_BLOCK points to the 512-byte block where the "trans_hist" member lies in the fileheader.
	 */
	assert(DIVIDE_ROUND_UP(offsetof(sgmnt_data, trans_hist), DISK_BLOCK_SIZE) == TH_BLOCK);
	if ((prev_reg = dbfilopn(greg)) != greg)
	{
		if (NULL == prev_reg || (gd_region *)-1L == prev_reg) /* (gd_region *)-1 == prev_reg => cm region open attempted */
			return;
		/* Found same database already open - prev_reg contains addr of originally openned region */
		greg->dyn.addr->file_cntl = prev_reg->dyn.addr->file_cntl;
		memcpy(greg->dyn.addr->fname, prev_reg->dyn.addr->fname, prev_reg->dyn.addr->fname_len);
		greg->dyn.addr->fname_len = prev_reg->dyn.addr->fname_len;
		csa = (sgmnt_addrs *)&FILE_INFO(greg)->s_addrs;
		PROCESS_GVT_PENDING_LIST(greg, csa, gvt_pending_list);
		csd = csa->hdr;
		if (NULL == csa->gvt_hashtab)
		{	/* Already have another region that points to the same physical database file as this one.
			 * Since two regions point to the same physical file, start maintaining a list of all global variable
			 * names whose gv_targets have already been allocated on behalf of the current database file.
			 * Future targ_allocs will check this list before they allocate (to avoid duplicate allocations).
			 */
			csa->gvt_hashtab = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
			init_hashtab_mname(csa->gvt_hashtab, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
			assert(1 == csa->regcnt);
			first_wasopen = TRUE;
		} else
			first_wasopen = FALSE;
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			table = addr_ptr->tab_ptr;
			for (tabent = table->base, topent = tabent + table->size; tabent < topent; tabent++)
			{
				if (HTENT_VALID_MNAME(tabent, gvnh_reg_t, gvnh_reg))
				{	/* Check if the gvt's region is the current region.
					 * If so add gvt's variable name into the csa hashtable.
					 */
					gvt = gvnh_reg->gvt;
					assert((gvnh_reg->gd_reg != greg) || (csa == gvt->gd_csa));
					/* If this is the first time a was_open region is happening for this csa, then
					 * we want to merge gv_targets from both the regions into csa->gvt_hashtab. For
					 * all future was_open cases, we want only to add gv_targets from the was_open region.
					 */
					if (first_wasopen && (csa == gvt->gd_csa) || !first_wasopen && (gvnh_reg->gd_reg == greg))
					{	/* Add gv_target into csa->gvt_hashtab */
						added = add_hashtab_mname(csa->gvt_hashtab, &gvt->gvname, gvt, &stayent);
						assert(!added || (1 <= gvt->regcnt));
						if (!added)
						{	/* Entry already present. Increment gvt->regcnt.
							 * If NOISOLATION status differs between the two,
							 * choose the more pessimistic one.
							 */
							gvt_stay = (gv_namehead *)stayent->value;
							assert(gvt_stay != gvt);
							if (FALSE == gvt->noisolation)
								gvt_stay->noisolation = FALSE;
							assert(1 <= gvt_stay->regcnt);
							/* Now make gvnh_reg->gvt point to gvt_stay (instead of gvt) */
							gvt_stay->regcnt++;
							gvt->regcnt--;
							gvnh_reg->gvt = gvt_stay;
							targ_free(gvt);
						}
					}
				}
			}
		}
		greg->max_rec_size = csd->max_rec_size;
		greg->max_key_size = csd->max_key_size;
	 	greg->null_subs = csd->null_subs;
		greg->std_null_coll = csd->std_null_coll;
		greg->jnl_state = csd->jnl_state;
		greg->jnl_file_len = csd->jnl_file_len;		/* journal file name length */
		memcpy(greg->jnl_file_name, csd->jnl_file_name, greg->jnl_file_len);	/* journal file name */
		greg->jnl_alq = csd->jnl_alq;
		greg->jnl_deq = csd->jnl_deq;
		greg->jnl_buffer_size = csd->jnl_buffer_size;
		greg->jnl_before_image = csd->jnl_before_image;
		SET_REGION_OPEN_TRUE(greg, WAS_OPEN_TRUE);
		assert(1 <= csa->regcnt);
		csa->regcnt++;	/* Increment # of regions that point to this csa */
		return;
	}
	GTM_FD_TRACE_ONLY(gtm_dbjnl_dupfd_check();)	/* check if any of db or jnl fds collide (D9I11-002714) */
	greg->was_open = FALSE;
	/* We shouldn't have crit on any region unless we are in TP and in the final retry or we are in mupip_set_journal trying to
	 * switch journals across all regions. WBTEST_HOLD_CRIT_ENABLED is an exception because it exercises a deadlock situation so
	 * it needs to hold multiple crits at the same time. Currently, there is no fine-granular checking for mupip_set_journal,
	 * hence a coarse MUPIP_IMAGE check for image_type.
	 */
	assert(dollar_tlevel && (CDB_STAGNATE <= t_tries) || IS_MUPIP_IMAGE || (0 == have_crit(CRIT_HAVE_ANY_REG))
	       || WBTEST_ENABLED(WBTEST_HOLD_CRIT_ENABLED));
	if (dollar_tlevel && (0 != have_crit(CRIT_HAVE_ANY_REG)))
	{	/* To avoid deadlocks with currently holding crits and the DLM lock request to be done in db_init(),
		 * we should insert this region in the tp_reg_list and tp_restart should do the gvcst_init after
		 * having released crit on all regions. Note that this check should be done AFTER checking if the
		 * region has already been opened (i.e. greg->was_open = TRUE logic above) since in that case we dont
		 * do any heavyweight processing (like db_init which involves crit/DLM locks) and so dont need to restart.
		 */
		insert_region(greg, &tp_reg_list, &tp_reg_free_list, SIZEOF(tp_region));
		t_retry(cdb_sc_needcrit);
		assert(FALSE);	/* we should never reach here since t_retry should have unwound the M-stack and restarted the TP */
	}
	csa = (sgmnt_addrs *)&FILE_INFO(greg)->s_addrs;

#ifdef	NOLICENSE
	licensed = TRUE;
#else
	CRYPT_CHKSYSTEM;
#endif
	db_init_region = greg;	/* initialized for dbinit_ch */
	csa->hdr = NULL;
        csa->nl = NULL;
        csa->jnl = NULL;
	csa->persistent_freeze = FALSE;	/* want secshr_db_clnup() to clear an incomplete freeze/unfreeze codepath */
	csa->regcnt = 1;	/* At this point, only one region points to this csa */
	csa->db_addrs[0] = csa->db_addrs[1] = NULL;
	csa->lock_addrs[0] = csa->lock_addrs[1] = NULL;
#	ifdef VMS
	greg_acc_meth = greg->dyn.addr->acc_meth;
	assert(dba_cm != greg_acc_meth);
	temp_cs_data = (sgmnt_data_ptr_t)cs_data_buff;
	fc = greg->dyn.addr->file_cntl;
	fc->file_type = greg_acc_meth;
	fc->op = FC_READ;
	fc->op_buff = (sm_uc_ptr_t)temp_cs_data;
	fc->op_len = SIZEOF(*temp_cs_data);
	fc->op_pos = 1;
	dbfilop(fc);
	DO_BADDBVER_CHK(greg, temp_cs_data);
	DO_DB_HDR_CHECK(greg, temp_cs_data); /* Basic sanity check on the file header fields */
	if (greg_acc_meth != temp_cs_data->acc_meth)
	{
		greg_acc_meth = temp_cs_data->acc_meth;
		greg->dyn.addr->acc_meth = greg_acc_meth;
	}
#	endif
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
#	ifdef UNIX
	START_HEARTBEAT_IF_NEEDED;
	if (!pool_init && jnlpool_init_needed && ANTICIPATORY_FREEZE_AVAILABLE && REPL_INST_AVAILABLE)
		jnlpool_init(GTMRELAXED, (boolean_t)FALSE, (boolean_t *)NULL);
	/* Any LSEEKWRITEs hence forth will wait if the instance is frozen. To aid in printing the region information before
	 * and after the wait, csa->region is referenced. Since it is NULL at this point, set it to greg. This is a safe
	 * thing to do since csa->region is anyways set in db_common_init (few lines below).
	 */
	csa->region = greg;
#	endif
	/* Protect the db_init and the code below until we set greg->open to TRUE. This is needed as otherwise,
	 * if a MUPIP STOP is issued to this process at a time-window when db_init is completed but greg->open
	 * is NOT set to TRUE, will cause gds_rundown NOT to clean up the shared memory created by db_init and
	 * thus would be left over in the system.
	 */
	DEFER_INTERRUPTS(INTRPT_IN_GVCST_INIT);
	VMS_ONLY(db_init(greg, temp_cs_data));
#	ifdef UNIX
	db_init_retry = 0;
	GTM_WHITE_BOX_TEST(WBTEST_HOLD_FTOK_UNTIL_BYPASS, db_init_retry, 3);
	for (; db_init_retry < MAX_DBINIT_RETRY; db_init_retry++)
	{
		if (0 == db_init(greg))
			break;
		db_init_err_cleanup(MAX_DBINIT_RETRY >  (db_init_retry + 1));
	}
	if (MAX_DBINIT_RETRY == db_init_retry) /* We retried enough. Error out. */
	{
		assert(IS_LKE_IMAGE || IS_DSE_IMAGE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REGOPENFAIL, 4, REG_LEN_STR(greg), DB_LEN_STR(greg));
	}

#	endif
	/* At this point, we have initialized the database, but haven't yet set reg->open to TRUE. If any rts_errors happen in
	 * the meantime, there are no condition handlers established to handle the rts_error. More importantly, it is non-trivial
	 * to add logic to such a condition handler to undo the effects of db_init. Also, in some cases, the rts_error can can
	 * confuse future calls of db_init. By invoking DBG_MARK_RTS_ERROR_UNUSABLE, we can catch any rts_errors in future and
	 * eliminate it on a case by case basis.
	 */
	UNIX_ONLY(DBG_MARK_RTS_ERROR_UNUSABLE);
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
	assert(greg->dyn.addr->acc_meth == csd->acc_meth); /* db_init should have made sure this assert holds good */
	greg_acc_meth = csd->acc_meth;
	/* It is necessary that we do the pending gv_target list reallocation BEFORE db_common_init as the latter resets
	 * greg->max_key_size to be equal to the csd->max_key_size and hence process_gvt_pending_list might wrongly conclude
	 * that NO reallocation (since it checks greg->max_key_size with csd->max_key_size) is needed when in fact a
	 * reallocation might be necessary (if the user changed max_key_size AFTER database creation)
	 */
	PROCESS_GVT_PENDING_LIST(greg, csa, gvt_pending_list);
	db_common_init(greg, csa, csd);	/* do initialization common to db_init() and mu_rndwn_file() */

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
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBVERPERFWARN1, 2, DB_LEN_STR(greg));
		else
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBVERPERFWARN2, 2, DB_LEN_STR(greg));
	}

	/* Compute the maximum journal space requirements for a PBLK (including possible ALIGN record).
	 * Use this variable in the TOTAL_TPJNL_REC_SIZE and TOTAL_NONTP_JNL_REC_SIZE macros instead of recomputing.
	 */
	csa->pblk_align_jrecsize = (int4)MIN_PBLK_RECLEN + csd->blk_size + (int4)MIN_ALIGN_RECLEN;
	segment_update_array_size = UA_SIZE(csd);

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
			GTMCRYPT_ONLY(non_tp_jfb_ptr->alt_buff = NULL);
		} else if (bsize > non_tp_jfb_ptr->hi_water_bsize)
		{	/* Need a larger buffer to accommodate larger non-TP journal records */
			non_tp_jfb_ptr->hi_water_bsize = bsize;
			free(non_tp_jfb_ptr->buff);
			non_tp_jfb_ptr->buff = (char *)malloc(MAX_NONTP_JNL_REC_SIZE(bsize));
#			ifdef GTM_CRYPT
			if (NULL != non_tp_jfb_ptr->alt_buff)
			{
				free(non_tp_jfb_ptr->alt_buff);
				realloc_alt_buff = TRUE;
			}
#			endif
		}
		/* If the journal records need to be encrypted in the journal file and if replication is in use,
		 * we will need access to both the encrypted (for the journal file) and unencrypted (for the
		 * journal pool) journal record contents. Allocate an alternative buffer if any open journaled region
		 * is encrypted.
		 */
#		ifdef GTM_CRYPT
		if (realloc_alt_buff || (csd->is_encrypted && (NULL == non_tp_jfb_ptr->alt_buff)))
			non_tp_jfb_ptr->alt_buff = (char *)malloc(MAX_NONTP_JNL_REC_SIZE(non_tp_jfb_ptr->hi_water_bsize));
#		endif
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
		gvcst_tp_init(greg);	/* Initialize TP structures, else postpone till TP is used (only if GTM) */
	if (!global_tlvl_info_list)
	{
		global_tlvl_info_list = (buddy_list *)malloc(SIZEOF(buddy_list));
		initialize_list(global_tlvl_info_list, SIZEOF(global_tlvl_info), GBL_TLVL_INFO_LIST_INIT_ALLOC);
	}
	assert(!greg->was_open);
	SET_REGION_OPEN_TRUE(greg, WAS_OPEN_FALSE);
	csa = (sgmnt_addrs*)&FILE_INFO(greg)->s_addrs;
	if (NULL != csa->dir_tree)
	{	/* It is possible that dir_tree has already been targ_alloc'ed. This is because GT.CM or VMS DAL
		 * calls can run down regions without the process halting out. We don't want to double malloc.
		 */
		csa->dir_tree->clue.end = 0;
	}
	SET_CSA_DIR_TREE(csa, greg->max_key_size, greg);
	/* Now that reg->open is set to TRUE and directory tree is initialized, go ahead and set rts_error back to being usable */
	UNIX_ONLY(DBG_MARK_RTS_ERROR_USABLE);
	/* gds_rundown if invoked from now on will take care of cleaning up the shared memory segment */
	ENABLE_INTERRUPTS(INTRPT_IN_GVCST_INIT);
	if (dba_bg == greg_acc_meth)
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
			DEBUG_ONLY(reformat_buffer_in_use++;)
			if (reformat_buffer)
				free(reformat_buffer);	/* Different blksized databases in use .. keep only largest one */
			reformat_buffer = malloc(csd->blk_size);
			reformat_buffer_len = csd->blk_size;
			DEBUG_ONLY(reformat_buffer_in_use--;)
			assert(0 == reformat_buffer_in_use);
			--fast_lock_count;
		}

	}
	if ((dba_bg == greg_acc_meth) || (dba_mm == greg_acc_meth))
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
		greg_fid = &(csa->nl->unique_id);
		for (regcsa = cs_addrs_list; NULL != regcsa; regcsa = regcsa->next_csa)
		{
			UNIX_ONLY(onln_rlbk_cycle_mismatch |= (regcsa->db_onln_rlbkd_cycle != regcsa->nl->db_onln_rlbkd_cycle));
			if ((NULL != prevcsa) && (regcsa->fid_index < prevcsa->fid_index))
				continue;
			reg_fid = &((regcsa)->nl->unique_id);
			VMS_ONLY(if (0 < memcmp(&(greg_fid->file_id), (char *)&(reg_fid->file_id), SIZEOF(gd_id))))
			UNIX_ONLY(if (0 < gdid_cmp(&(greg_fid->uid), &(reg_fid->uid))))
			{
				if ((NULL == prevcsa) || (regcsa->fid_index > prevcsa->fid_index))
					prevcsa = regcsa;
			} else
				regcsa->fid_index++;
		}
		if (NULL == prevcsa)
			csa->fid_index = 1;
		else
			csa->fid_index = prevcsa->fid_index + 1;
		UNIX_ONLY(
			if (onln_rlbk_cycle_mismatch)
			{
				csa->root_search_cycle--;
				csa->onln_rlbk_cycle--;
				csa->db_onln_rlbkd_cycle--;
			}
		)
		/* Add current csa into list of open csas */
		csa->next_csa = cs_addrs_list;
		cs_addrs_list = csa;
		/* Also update tp_reg_list fid_index's as insert_region relies on it */
		for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
			tr->file.fid_index = (&FILE_INFO(tr->reg)->s_addrs)->fid_index;
		DBG_CHECK_TP_REG_LIST_SORTING(tp_reg_list);
	}
#	ifdef UNIX
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
		VALIDATE_INITIALIZED_JNLPOOL(csa, csa->nl, greg, GTMRELAXED, SCNDDBNOUPD_CHECK_FALSE);
	}
#	endif
	return;
}
