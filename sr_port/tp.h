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

#ifndef TP_H
#define TP_H

#include <sys/types.h>

error_def(ERR_TPNOTACID);

/* HEADER-FILE-DEPENDENCIES : hashtab_int4.h */

#define JNL_LIST_INIT_ALLOC		16		/* initial allocation for si->jnl_list */
#define	CW_SET_LIST_INIT_ALLOC		64		/* initial allocation for si->cw_set_list */
#define	TLVL_CW_SET_LIST_INIT_ALLOC     64		/* initial allocation for si->tlvl_cw_set_list */
#define	NEW_BUFF_LIST_INIT_ALLOC       	 8		/* initial allocation for si->new_buff_list */
#define	RECOMPUTE_LIST_INIT_ALLOC	 8		/* initial allocation for si->recompute_list */
#define	BLKS_IN_USE_INIT_ELEMS		64		/* initial no. of elements in hash table si->blks_in_use */
#define	TLVL_INFO_LIST_INIT_ALLOC	 4		/* initial allocation for si->tlvl_info_head */
#define	GBL_TLVL_INFO_LIST_INIT_ALLOC	 8		/* initial allocation for global_tlvl_info_head (one per region) */

#define	INIT_CUR_TP_HIST_SIZE		64 		/* initial value of si->cur_tp_hist_size */
#define TP_MAX_MM_TRANSIZE		64*1024
#define JNL_FORMAT_BUFF_INIT_ALLOC	16*1024
#define	JFB_ELE_SIZE			8		/* Same as JNL_REC_START_BNDRY */
#define	JFB_ELE_SIZE_IN_BITS		3		/* log2 of JFB_ELE_SIZE */

#define TP_BATCH_ID		"BATCH"
#define TP_BATCH_LEN		(SIZEOF(TP_BATCH_ID) - 1)
#define TP_BATCH_SHRT		2	/* permit abbreviation to two characters */
#define TP_DEADLOCK_FACTOR	5	/* multiplied by dollar_trestart to produce an argument for wcs_backoff */
#define MAX_VISIBLE_TRESTART	4	/* Per Bhaskar on 10/20/98: dollar_trestart is not allowed to visibly exceed 4
					 * because of errors this causes in legacy Profile versions (any < 6.1)
					 */
#define	MAX_TP_FINAL_RETRY_TRESTART_CNT		 2
#define	MAX_TP_FINAL_RETRY_MLOCKRESTART_CNT	16
#define	MAX_TRESTARTS		(2 * MAX_TP_FINAL_RETRY_MLOCKRESTART_CNT)
#define	FAIL_HIST_ARRAY_SIZE	MAX_TRESTARTS

/* structure to hold transaction level specific info per segment.
 * Aids in incremental rollback, to identify the state of various elements at the BEGINNING of a new transaction level. */

typedef struct tlevel_info_struct
{
	que_ent		free_que;
	struct tlevel_info_struct
			*next_tlevel_info;
	kill_set	*tlvl_kill_set;		/* these two fields hold the kill_set state before this t_level started */
	int4		tlvl_kill_used;
	jnl_format_buffer
			*tlvl_jfb_info;		/* state of the tp_jnl_format_buff_info list before this t_level started */
	srch_blk_status	*tlvl_tp_hist_info;	/* state of the tp_hist array (tail) before this t_level started */
	uint4		t_level;
	uint4		update_trans;		/* a copy of sgm_info_ptr->update_trans before this t_level started */
	uint4		jnl_list_elems;		/* # of si->jnl_list elements consumed before this transaction started */
	uint4		jfb_list_elems;	/* # of si->format_buff_list elements consumed before this transaction started */
} tlevel_info;

/* structure to hold the global (across all segments) dollar_tlevel specific information.
 * To identify the state of various elements at the BEGINNING of a given transaction level.
 */

typedef struct global_tlvl_info_struct
{
	struct global_tlvl_info_struct
			*next_global_tlvl_info;
	sgmnt_addrs	*global_tlvl_fence_info;
	uint4		t_level;
	uint4		tlvl_cumul_jrec_len;
#	ifdef DEBUG
	uint4		tlvl_cumul_index;
#	endif
	uint4		tlvl_tp_ztp_jnl_upd_num;
#	ifdef GTM_TRIGGER
	unsigned char	*tlvl_prev_ztworm_ptr;
#	endif
	struct ua_list	*curr_ua;		/* points to global variable curr_ua at start of this transaction */
	char		*upd_array_ptr;		/* points to global variable update_array_ptr at start of this transaction */
#	ifdef GTM_TRIGGER
	int		ztrigbuffLen;		/* copy of TREF(ztrigbuffLen) at start of this transaction */
#	endif
} global_tlvl_info;

/* A note on the buddy lists used in sgm_info structure,
 *	cw_set_list		->	uses get_new_element      and free_last_n_elements
 *	tlvl_cw_set_list	-> 	uses get_new_free_element and free_element
 *	jnl_list		->	uses get_new_element      and free_last_n_elements
 *	tlvl_info_list		->	uses get_new_element      and free_last_n_elements
 *	gbl_tlvl_info_list	->	uses get_new_element      and free_last_n_elements
 *	new_buff_list		->	uses get_new_free_element and free_element
 *	recompute_list		->	uses get_new_element
 */

/* A small comment about the tp_hist_size and cur_tp_hist_size members of the sgm_info structure.
 * Currently, we allow a maximum of 64k (TP_MAX_MM_TRANSIZE) blocks in the read-set of a TP transaction and n_bts/2 blocks
 * in the write-set. Each block in the read-set has a corresponding srch_blk_status structure used in final validation in tp_tend.
 * The current code requires the read-set to be a contiguous array of srch_blk_status structures for performance considerations.
 * A process may need anywhere from 1 to 64K elements of this array depending on the TP transaction although the average need
 * 	is most likely to be less than a hundred. But mallocing 64K srch_blk_status structures at one stretch (in order to allow
 * 	the maximum) is too costly in terms of virtual memory (approx. 3 MB memory per region per process).
 * Therefore, we malloc only cur_tp_hist_size structures initially. As the need for more structures arises, we malloc an array
 *	double the size of the original	and reset pointers appropriately.
 * cur_tp_hist_size always represents the current allocation while tp_hist_size represents the maximum possible allocation.
 */
typedef struct sgm_info_struct
{
	struct sgm_info_struct	*next_sgm_info,
				*next_tp_si_by_ftok;	 /* List of ALL     regions in the TP transaction sorted on ftok order */
	srch_blk_status		*first_tp_hist,
				*last_tp_hist;
	hash_table_int4		*blks_in_use;
	trans_num		start_tn;
	gd_region		*gv_cur_region;	/* Backpointer to the region; Note that it is not necessarily unique since
						 * multiple regions could point to the same csa (and hence same sgm_info
						 * structure) with all but one of them having reg->was_open set to TRUE.
						 */
	uint4			update_trans;	/* bitmask indicating among other things whether this region was updated;
						 * Bit-0 is 1 if cw_set_depth is non-zero or if it is a duplicate set
						 * (cw_set_depth is zero in that case).
						 * Bit-1 is 1 if there was a journaled logical update in this region.
						 * Bit-2 is 1 if transaction commit in this region is beyond point of rollback
						 */
	cw_set_element		*first_cw_set,
				*last_cw_set,
				*first_cw_bitmap;
	buddy_list		*cw_set_list;		/* list(buddy) of cw_set_elements for this region */
	buddy_list		*tlvl_cw_set_list;	/* list(buddy) of horizontal cw_set_elements for this region */
							/* first link in each of these horizontal lists is maintained in
							 * cw_set_list buddy list */
	buddy_list		*new_buff_list;		/* to hold the new_buff for the cw_set elements */
	buddy_list		*recompute_list;	/* to hold the list of to-be-recomputed keys and values */
	buddy_list		*tlvl_info_list;	/* to hold the list of tlvl_info structures */
	cache_rec_ptr_ptr_t	cr_array;
	sgmnt_data_ptr_t	tp_csd;
	sgmnt_addrs		*tp_csa;
	kill_set		*kill_set_head,
				*kill_set_tail;
	tlevel_info		*tlvl_info_head;
	jnl_format_buffer	*jnl_head,
				**jnl_tail;
	buddy_list		*format_buff_list;
	buddy_list		*jnl_list;
	int			cw_set_depth,
				cr_array_index,
				num_of_blks,
				tp_hist_size,
				cur_tp_hist_size,
				total_jnl_rec_size,
				cr_array_size;
	boolean_t		tp_set_sgm_done;
	int4			crash_count;
	boolean_t		backup_block_saved;
	sgmnt_addrs		*kip_csa;
	int			tmp_cw_set_depth;	/* used only #ifdef DEBUG. see comments for tmp_cw_set_depth in "tp_tend" */
	uint4			tot_jrec_size;		/* maximum journal space needs for this transaction */
	jbuf_rsrv_struct_t	*jbuf_rsrv_ptr;		/* Pointer to structure corresponding to reservations on the journal
							 * buffer for this region in current TP transaction.
							 */
} sgm_info;

/* Define macros to reflect the size of cw_index and next_off in the off_chain structure.
 * If the structure layout changes, these formulas might need corresponding adjustment.
 * Note that ideally we should be using SIZEOF(off_chain) * 8 instead of 32 in the NEXT_OFF_MAX_BITS definition
 * (currently commented due to a gengtmdeftypes issue). But that introduces a cyclic dependency causing a compiler error.
 */
#define	CW_INDEX_MAX_BITS	15
/* Remove the next line and uncomment the following line once gengtmdeftypes is fixed to allow expressions in bitfield members */
#define	NEXT_OFF_MAX_BITS	16
/* #define	NEXT_OFF_MAX_BITS	(32 - CW_INDEX_MAX_BITS - 1) */

typedef struct
{
#ifdef	BIGENDIAN
	unsigned	flag	 : 1;
	unsigned	cw_index : CW_INDEX_MAX_BITS;
	unsigned	next_off : NEXT_OFF_MAX_BITS;
#else
	unsigned	next_off : NEXT_OFF_MAX_BITS;
	unsigned	cw_index : CW_INDEX_MAX_BITS;
	unsigned	flag	 : 1;
#endif
} off_chain;

/* The following struct is built into a separate list for each transaction
   because it is not thrown away if a transaction restarts. The list keeps
   growing so we can lock down all the necessary regions in the correct order
   in case one attempt doesn't get very far while later attempts get further.
   Items will be put on the list sorted in unique_id order so that they will always
   be grab-crit'd in the same order thus avoiding deadlocks. */

/* The structure backup_region_list defined in mupipbckup.h needs to have its first four fields
   identical to the first three fields in this structure */
typedef struct tp_region_struct
{
	struct	tp_region_struct *fPtr;		/* Next in list */
	gd_region	*reg;			/* Region pointer. Note that it is not necessarily unique since multiple
						 * regions could point to the same physical file (with all but one of them
						 * having reg->was_open set to TRUE.and hence have the same tp_region structure.
						 */
	union					/* we will either use file_id or index */
	{
		gd_id		file_id;
		int4		fid_index;	/* copy of csa->fid_index for this region */
	} file;
} tp_region;

#ifdef	DEBUG
/* Macro to check that the tp_reg_list linked list is sorted properly on the file-id */
#define	DBG_CHECK_TP_REG_LIST_SORTING(REGLIST)						\
{											\
	int4		prev_index;							\
	tp_region	*tr;								\
											\
	prev_index = 0;									\
	for (tr = REGLIST; NULL != tr; tr = tr->fPtr)					\
	{										\
		if (tr->reg->open)							\
		{									\
			assert(prev_index < tr->file.fid_index);			\
			DEBUG_ONLY(prev_index = tr->file.fid_index);			\
		}									\
	}										\
}

#define	DBG_CHECK_SI_BUDDY_LIST_IS_REINITIALIZED(si)					\
{											\
	sgmnt_addrs	*csa;								\
											\
	csa = si->tp_csa;								\
	assert(NULL == si->kill_set_head);						\
	assert(NULL == si->kill_set_tail);						\
	assert(NULL == si->jnl_head);							\
	assert(NULL == csa->next_fenced);						\
	if (JNL_ALLOWED(csa))								\
	{										\
		assert(si->total_jnl_rec_size == csa->min_total_tpjnl_rec_size);	\
		VERIFY_LIST_IS_REINITIALIZED(si->jnl_list);				\
		VERIFY_LIST_IS_REINITIALIZED(si->format_buff_list);			\
		assert(si->jnl_tail == &si->jnl_head);					\
		assert(NULL != si->jbuf_rsrv_ptr);					\
	} else										\
	{										\
		assert(NULL == si->jnl_list);						\
		assert(NULL == si->format_buff_list);					\
		assert(NULL == si->jnl_tail);						\
		assert(NULL == si->jbuf_rsrv_ptr);					\
	}										\
	VERIFY_LIST_IS_REINITIALIZED(si->recompute_list);				\
	VERIFY_LIST_IS_REINITIALIZED(si->cw_set_list);					\
	VERIFY_LIST_IS_REINITIALIZED(si->new_buff_list);				\
	VERIFY_LIST_IS_REINITIALIZED(si->tlvl_cw_set_list);				\
	assert(NULL == si->first_cw_set);						\
	assert(NULL == si->last_cw_set);						\
	assert(NULL == si->first_cw_bitmap);						\
	assert(0 == si->cw_set_depth);							\
	assert(0 == si->update_trans);							\
}

#define	DBG_CHECK_IN_FIRST_SGM_INFO_LIST(SI)						\
{											\
	sgm_info		*tmpsi;							\
											\
	GBLREF sgm_info		*first_sgm_info;					\
	GBLREF  uint4           dollar_tlevel;						\
											\
	assert(dollar_tlevel);								\
	assert(NULL != first_sgm_info);							\
	assert(NULL != SI);								\
	for (tmpsi = first_sgm_info; NULL != tmpsi; tmpsi = tmpsi->next_sgm_info)	\
	{										\
		if (tmpsi == SI)							\
			break;								\
	}										\
	assert(NULL != tmpsi);								\
}

#else
#define	DBG_CHECK_TP_REG_LIST_SORTING(REGLIST)
#define	DBG_CHECK_IN_FIRST_SGM_INFO_LIST(SI)
#endif

/* The below macro is used to check if any block split info (heuristic used by gvcst_put) is no longer relevant
 * (due to an incremental rollback or rollback or restart) and if so reset it to a safe value of 0.
 */
#define	TP_CLEANUP_GVNH_SPLIT_IF_NEEDED(GVNH, CW_DEPTH)								\
{														\
	int		cw_set_depth;										\
	int		level;											\
	off_chain	chain1;											\
														\
	cw_set_depth = CW_DEPTH;										\
	if (GVNH->split_cleanup_needed)										\
	{	/* Created block numbers stored in the blk split array are no longer relevant after the		\
		 * restart. So reset them not to confuse the next call to gvcst_put for this gv_target.		\
		 */												\
		for (level = 0; level < ARRAYSIZE(GVNH->last_split_blk_num); level++)				\
		{												\
			chain1 = *(off_chain *)&GVNH->last_split_blk_num[level];				\
			if (chain1.flag && ((unsigned)cw_set_depth <= (unsigned)chain1.cw_index))		\
				GVNH->last_split_blk_num[level] = 0;						\
		}												\
	}													\
}

/* Following macro resets clues and root block number of all the gv_targets accessed in the life-time of this process. This is
 * needed whenever a process during its transaction (TP or Non-TP) validation phase detects that a concurrent Online Rollback
 * started and completed in the meantime, the transaction should NOT go for commit, but should restart to ensure the clues and
 * root block numbers are reset so that the restarted transaction uses the up-to-date information.
 */
#  define RESET_ALL_GVT_CLUES											\
{														\
	GBLREF	gv_namehead	*gv_target_list;								\
	GBLREF	gv_namehead	*reorg_gv_target;								\
														\
	gv_namehead		*gvnh;										\
														\
	for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)					\
	{													\
		gvnh->clue.end = 0;										\
		if (gvnh->gd_csa && (gvnh != gvnh->gd_csa->dir_tree))						\
		{												\
			assert ((DIR_ROOT != gvnh->root) || (gvnh == reorg_gv_target));				\
			gvnh->root = 0;										\
		}												\
		/* Cleanup any block-split info (of created block #) in gvtarget histories */			\
		TP_CLEANUP_GVNH_SPLIT_IF_NEEDED(gvnh, 0);							\
	}													\
}
#  define RESET_ALL_GVT_CLUES_REG(CSA)										\
{														\
	GBLREF	gv_namehead	*gv_target_list;								\
	GBLREF	gv_namehead	*reorg_gv_target;								\
														\
	gv_namehead		*gvnh;										\
														\
	assert(NULL != CSA);											\
	for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)					\
	{													\
		if (CSA == gvnh->gd_csa)									\
		{	/* Only reset info for globals in CSA. */						\
			gvnh->clue.end = 0;									\
			if (gvnh != gvnh->gd_csa->dir_tree)							\
				gvnh->root = 0;									\
			TP_CLEANUP_GVNH_SPLIT_IF_NEEDED(gvnh, 0);						\
		}												\
	}													\
}

typedef struct ua_list_struct
{
	struct ua_list_struct
			*next_ua;
	char		*update_array;
	uint4		update_array_size;
} ua_list;

#define TP_MAX_NEST	127

/* Note gv_orig_key is assigned to tp_pointer->orig_key which then tries to dereference the "begin", "end", "prev", "top"
 * 	fields like it were a gv_currkey pointer. Since these members are 2-byte fields, we need atleast 2 byte alignment.
 * We want to be safer and hence give 4-byte alignment by declaring it as an array of integers.
 * Note that the array stores only ONE key since we need to store gv_currkey at the outermost TSTART only since this needs
 *	to be restored only in case of a TRESTART or TROLLBACK where the objective is to restore it to what the state was
 *	at the outermost TSTART.
 */
typedef struct gv_orig_key_struct
{
	gv_key	gv_orig_key[DBKEYALLOC(MAX_KEY_SZ)];
}gv_orig_key_array;

#define TRANS_RESTART_HIST_ARRAY_SZ	512 /* See comment (6) in gtm_threadgbl_defs.h as to why this is not inside #ifdef DEBUG */
#ifdef DEBUG
/* The following structure stores information pertaining to the most recent invocation of t_retry OR tp_restart. Maintain a 512
 * element array per-process to track the recent t_retry and tp_restart invocations.  Instead of keeping track of dollar_tlevel,
 * keep track of the current csa which is always Non-NULL for Non-TP. For TP, csa can be NULL but store it as NULL in the history
 * as there are multiple regions involved anyways.
 */
typedef struct trans_restart_hist_struct
{
	uint4			t_tries;
	uint4			dollar_tlevel;
	enum cdb_sc		retry_code;
	caddr_t			call_from;
	union
	{
		trans_num	curr_tn;	/* used if replication disabled */
		seq_num		jnl_seqno;	/* used if replication enabled */
	} seq_or_tn;
	sgmnt_addrs		*csa;		/* NULL if TP, Non-NULL if Non-TP */
} trans_restart_hist_t;

# define TRACE_TRANS_RESTART(RETRY_CODE)								\
{													\
	GBLREF	jnlpool_addrs_ptr_t	jnlpool;							\
	GBLREF	unsigned int		t_tries;							\
	GBLREF	unsigned int		dollar_tlevel;							\
	GBLREF	sgmnt_addrs		*cs_addrs;							\
													\
	uint4				curidx;								\
	trans_restart_hist_t		*this_restart_hist;						\
													\
	assert(dollar_tlevel || (NULL != cs_addrs));							\
	curidx = ++(TREF(trans_restart_hist_index));							\
	if (TRANS_RESTART_HIST_ARRAY_SZ <= curidx)							\
		curidx = TREF(trans_restart_hist_index) = 0;						\
	this_restart_hist = (TADR(trans_restart_hist_array) + curidx);					\
	this_restart_hist->t_tries = t_tries;								\
	this_restart_hist->dollar_tlevel = dollar_tlevel;						\
	this_restart_hist->retry_code = RETRY_CODE;							\
	this_restart_hist->call_from = (caddr_t)caller_id();						\
	if ((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl))						\
		this_restart_hist->seq_or_tn.jnl_seqno = jnlpool->jnlpool_ctl->jnl_seqno;		\
	else												\
		this_restart_hist->seq_or_tn.curr_tn = (NULL != cs_addrs) ? cs_addrs->ti->curr_tn : 0;	\
	this_restart_hist->csa = dollar_tlevel ? NULL : cs_addrs;					\
}
#else
# define TRACE_TRANS_RESTART(RETRY_CODE)
#endif	/* DEBUG */

#define TP_TRACE_HIST(X, Y) 										\
{													\
	GBLREF	gd_region	*tp_fail_hist_reg[];							\
	GBLREF	gv_namehead	*tp_fail_hist[];							\
	GBLREF	block_id	t_fail_hist_blk[];							\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	if (TREF(tprestart_syslog_delta))								\
	{												\
		tp_fail_hist_reg[t_tries] = gv_cur_region;						\
		t_fail_hist_blk[t_tries] = ((block_id)X); 						\
		tp_fail_hist[t_tries] = (gv_namehead *)(((int)X & ~(-BLKS_PER_LMAP)) ? Y : NULL);	\
	}												\
}

#define	ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(FIRST_TP_SRCH_STATUS, SGM_INFO_PTR)	\
{											\
	assert(NULL == (FIRST_TP_SRCH_STATUS) 						\
		|| ((FIRST_TP_SRCH_STATUS) >= (SGM_INFO_PTR)->first_tp_hist		\
			&& (FIRST_TP_SRCH_STATUS) < (SGM_INFO_PTR)->last_tp_hist));	\
}

#define	SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(CSA, CNL, STATUS)				\
{	/* set wc_blocked if final retry and cache related failure status */		\
	if (CDB_STAGNATE <= t_tries)							\
	{										\
		GBLREF	boolean_t	is_uchar_wcs_code[];				\
		GBLREF	boolean_t	is_lchar_wcs_code[];				\
			boolean_t	is_wcs_code = FALSE;				\
											\
		if (ISALPHA_ASCII(STATUS))						\
			is_wcs_code = (STATUS > 'Z') 					\
				? is_lchar_wcs_code[STATUS - 'a'] 			\
				: is_uchar_wcs_code[STATUS - 'A'];			\
		if (is_wcs_code)							\
		{									\
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);			\
			BG_TRACE_PRO_ANY(CSA, wc_blocked_wcs_cdb_sc_final_retry);	\
		}									\
	}										\
}

#define	TP_RETRY_ACCOUNTING(CSA, CNL)							\
{											\
	GBLREF	uint4		dollar_trestart;					\
											\
	switch (dollar_trestart)							\
	{										\
		case 0:									\
			INCR_GVSTATS_COUNTER(CSA, CNL, n_tp_cnflct_retries_0, 1);	\
			break;								\
		case 1:									\
			INCR_GVSTATS_COUNTER(CSA, CNL, n_tp_cnflct_retries_1, 1);	\
			break;								\
		case 2:									\
			INCR_GVSTATS_COUNTER(CSA, CNL, n_tp_cnflct_retries_2, 1);	\
			break;								\
		case 3:									\
			INCR_GVSTATS_COUNTER(CSA, CNL, n_tp_cnflct_retries_3, 1);	\
			break;								\
		default:								\
			INCR_GVSTATS_COUNTER(CSA, CNL, n_tp_cnflct_retries_4, 1);	\
			break;								\
	}										\
}

#define PREV_OFF_INVALID -1

#define TOTAL_TPJNL_REC_SIZE(TOTAL_JNL_REC_SIZE, SI, CSA)							\
{														\
	DEBUG_ONLY(SI->tmp_cw_set_depth = SI->cw_set_depth;)	/* save a copy to check later in "tp_tend" */	\
	TOTAL_JNL_REC_SIZE = SI->total_jnl_rec_size;								\
	if (CSA->jnl_before_image)										\
		TOTAL_JNL_REC_SIZE += (SI->cw_set_depth * CSA->pblk_align_jrecsize);				\
	/* Since we have already taken into account an align record per journal record and since the size of	\
	 * an align record will be < (size of the journal record written + fixed-size of align record)		\
	 * we can be sure we won't need more than twice the computed space.					\
	 * JNL_FILE_TAIL_PRESERVE is to give allowance for space needed at end of journal file 			\
	 * in case journal file close is needed 								\
	 */													\
	assert(JNL_FILE_TAIL_PRESERVE < (JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE));					\
	SI->total_jnl_rec_size = TOTAL_JNL_REC_SIZE = (TOTAL_JNL_REC_SIZE * 2) + (uint4)JNL_FILE_TAIL_PRESERVE;	\
}

#define MIN_TOTAL_NONTPJNL_REC_SIZE 	(PINI_RECLEN + MIN_ALIGN_RECLEN + INCTN_RECLEN + MIN_ALIGN_RECLEN)

/* This macro gives a pessimistic estimate on the total journal record size needed.
 * The side effect is that we might end up with a journal file extension when it was actually not needed.
 */
#define TOTAL_NONTPJNL_REC_SIZE(TOTAL_JNL_REC_SIZE, NON_TP_JFB_PTR, CSA, TMP_CW_SET_DEPTH)			\
{														\
	TOTAL_JNL_REC_SIZE = (NON_TP_JFB_PTR->record_size + (uint4)MIN_TOTAL_NONTPJNL_REC_SIZE);		\
	if (CSA->jnl_before_image)										\
		/* One PBLK record for each gds block changed by the transaction */				\
		TOTAL_JNL_REC_SIZE += (TMP_CW_SET_DEPTH * CSA->pblk_align_jrecsize);			\
	if (write_after_image)											\
		TOTAL_JNL_REC_SIZE += (uint4)MIN_AIMG_RECLEN + CSA->hdr->blk_size + (uint4)MIN_ALIGN_RECLEN;	\
	/* Since we have already taken into account an align record per journal record and since the size of	\
	 * an align record will be < (size of the journal record written + fixed-size of align record)		\
	 * we can be sure we won't need more than twice the computed space.					\
	 * JNL_FILE_TAIL_PRESERVE is to give allowance for space needed at end of journal file 			\
	 * in case journal file close is needed 								\
	 */													\
	assert(JNL_FILE_TAIL_PRESERVE < (JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE));					\
	TOTAL_JNL_REC_SIZE = TOTAL_JNL_REC_SIZE * 2 + (uint4)JNL_FILE_TAIL_PRESERVE;				\
}

#define INVALIDATE_CLUE(CSE) 					\
{								\
	off_chain	macro_chain;				\
								\
	assert(CSE->blk_target);				\
	CSE->blk_target->clue.end = 0;				\
	macro_chain = *(off_chain *)&CSE->blk_target->root;	\
	if (macro_chain.flag)					\
		CSE->blk_target->root = 0;			\
}

/* freeup killset starting from the link 'ks' */

#define FREE_KILL_SET(KS)					\
{								\
	kill_set	*macro_next_ks;				\
	for (; KS; KS = macro_next_ks)				\
	{							\
		macro_next_ks = KS->next_kill_set;		\
		free(KS);					\
	}							\
}

/* Determine previous jfb, if any */
#define SET_PREV_JFB(SI, PREV_JFB)											\
{															\
	if (SI->jnl_tail != &SI->jnl_head)										\
		PREV_JFB = (jnl_format_buffer *)((uchar_ptr_t)SI->jnl_tail - OFFSETOF(jnl_format_buffer, next));	\
	else														\
		PREV_JFB = NULL;											\
}

/* freeup si->jnl_list and si->format_buff_list elements which are no more needed
 * as the sub-transactions in which they have been created are now being rolled back
 */

#define FREE_JFB_INFO(SI, TLI, FREE_ALL_ELEMS)								\
{													\
	int	jnl_list_cnt, format_buf_list_cnt;							\
													\
	jnl_list_cnt = SI->jnl_list->nElems;								\
	format_buf_list_cnt = SI->format_buff_list->nElems;						\
	if (!FREE_ALL_ELEMS)										\
	{												\
		assert(TLI);										\
		assert(jnl_list_cnt >= TLI->jnl_list_elems);						\
		assert(format_buf_list_cnt >= TLI->jfb_list_elems);					\
		jnl_list_cnt -= TLI->jnl_list_elems;							\
		format_buf_list_cnt -= TLI->jfb_list_elems;						\
	}												\
	/* else all elements from si->jnl_head can be free'ed up since the sub-transactions		\
	 * in which journal activity started for this region is been currently rolled			\
	 * back												\
	 */												\
	if (format_buf_list_cnt && !free_last_n_elements(SI->format_buff_list, format_buf_list_cnt))	\
		assert(FALSE);										\
	if (jnl_list_cnt && !free_last_n_elements(SI->jnl_list, jnl_list_cnt))				\
		assert(FALSE);										\
}

/* Calls FREE_JFB_INFO based on whether journal activity existed in the prior, non-rolled back levels.
 * FREE_ALL_ELEMS is TRUE if either (a) No TLI exists for this region OR (b) All such TLI are now
 * being actively rolled back. In either case, the entire SI->jnl_list and SI->format_buff_list can be
 * freed up.
 */
#define FREE_JFB_INFO_IF_NEEDED(CSA, SI, TLI, FREE_ALL_ELEMS)									\
{																\
	jnl_format_buffer		*jfb;											\
																\
	if (JNL_WRITE_LOGICAL_RECS(CSA))											\
	{															\
		if (!FREE_ALL_ELEMS && (TLI->tlvl_jfb_info))									\
		{	/* Journal activity existed in $TLEVEL <= newlevel which should not be free'ed up */			\
			assert(0 < TLI->jnl_list_elems);									\
			assert(0 < TLI->jfb_list_elems);									\
			jfb = TLI->tlvl_jfb_info; /* last journal format buffer in the non-rolled back levels */		\
			FREE_JFB_INFO(SI, TLI, FALSE);										\
			jfb->next = NULL;											\
			SI->jnl_tail = &jfb->next;										\
			/* Ensure that size of si->jnl_list and si->format_buff_list matches the one noted down at op_tstart	\
			 * of $TLEVEL = (newlevel + 1)										\
			 */													\
			assert(SI->jnl_list->nElems == TLI->jnl_list_elems);							\
			assert(SI->format_buff_list->nElems == TLI->jfb_list_elems);						\
		} else														\
		{	/* Either journal activity did not exist on $TLEVEL <= newlevel OR the sub-transactions in which 	\
			 * they existed are now being rolled back. So, free all the jnl_list and format_buff_list buddy lists */\
			FREE_JFB_INFO(SI, TLI, TRUE);										\
			SI->jnl_head = NULL;											\
			SI->jnl_tail = &SI->jnl_head;										\
			assert(0 == SI->jnl_list->nElems);									\
			assert(0 == SI->format_buff_list->nElems);								\
		}														\
	}															\
}																\

/* freeup gbl_tlvl_info_list starting from the link 'gti' */

#define FREE_GBL_TLVL_INFO(GTI)							\
{										\
	int	macro_cnt;							\
										\
	for (macro_cnt = 0; GTI; GTI = GTI->next_global_tlvl_info)		\
		macro_cnt++;							\
	if (macro_cnt)								\
		free_last_n_elements(global_tlvl_info_list, macro_cnt);		\
}

#ifdef GTM_TRIGGER
/* State information needed by TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED(). Currently needed for trigger cycle handling */
typedef enum
{
	TP_COMMIT,
	TP_ROLLBACK,
	TP_INCR_ROLLBACK,
	TP_RESTART

} tp_cleanup_state;

/* TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED uses the logic below to manage CSA's and GVT's trigger cycle information
 *
 * Manage cached trigger state for regions and gvts. If either $ZTRIGGER() was invoked (dollar_ztrigger_invoked == TRUE) or
 * triggers were read from the database in the current (TREF(gvt_triggers_read_this_tn) == TRUE) transaction, certain state
 * information needs to be cleaned up at transaction commit, complete/incremental rollback or restart.
 * Rules for gvt:
 * - For all cases, commit, incr/complete rollback and restart, each gvt's db_dztrigger_cycle must be reset to zero. We do
 *   this to ensure that any $ZTRIGGER() causes a cycle mismatch which will result in a call to gvtr_db_read_hasht(). The end
 *   of this macro ASSERTs that cleanup will always reset gvt->db_dztrigger_cycle to zero.
 * - Any gvt that read trigger info from the DB since the transaction start will have gvt->trig_read_tn >= tstart_local_tn
 *   and gvt->trig_read_tn == local_tn if read in the current (possibly sub) transaction. For commits, there is nothing to
 *   do. Complete rollbacks and restarts, if gvt->gvt_trigger is non-NULL, reset gvt_trigger->gv_trigger_cycle to zero.
 *   Incremental rollbacks only reset gvt_trigger->gv_trigger_cycle to zero when the trigger was read in the current
 *   transaction. Alone, resetting gvt_trigger->gv_trigger_cycle to zero will not force a cycle mismatch.
 *   The above reset of the gvt's db_dztrigger_cycle to zero only causes a mismatch when $ZTRIGGER() is involved. To cover
 *   restarts and complete rollbacks without $ZTRIGGER() set gvt->db_trigger_cycle to gvt->gd_csa->db_trigger_cycle - 1 to force
 *   a cycle mismatch. While resetting gvt->db_dztrigger_cycle and gvt->db_trigger_cycle is redundant doing both simplifies
 *   the logic.
 * - For commits and complete rollbacks cleanup asserts that gvt->db_dztrigger_cycle == gvt->gd_csa->db_dztrigger_cycle
 * Rules for csa:
 * - For commits and complete rollbacks, reset csa->db_dztrigger_cycle to zero as the transaction is over. Also reset
 *   csa->incr_db_trigger_cycle to FALSE (this is redundant for commits as tp_tend will already have reset
 *   csa->incr_db_trigger_cycle when it increment csd->db_trigger_cycle. Iterate over tp_reg_list in case a region that was
 *   affected by a $ZTRIGGER() in a prior try, but not by the current try.
 * - For restarts and incremental rollbacks, if $ZTRIGGER() was invoked (dollar_ztrigger_invoked == TRUE), iterate over
 *   sgminfo to set each non-zero csa->db_dztrigger_cycle to 1. While any non-zero value will cause a cycle mismatch -
 *   because each gvt will have its gvt->db_dztrigger_cycle reset to 0 - one is enough to preserve the knowledge that
 *   $ZTRIGGER() affected loaded triggers. Assert that csa->db_dztrigger_cycle is non-zero when csa->incr_db_trigger_cycle is
 *   TRUE.
 * At clean up end:
 * - Reset TREF(gvt_triggers_read_this_tn) to FALSE
 * - Reset dollar_ztrigger_invoked to FALSE only for commits and complete rollbacks. Done last since it impacts both gvts and
 *   CSAs. Restarts and incremental rollbacks must retain the knowledge that $ZTRIGGER() was invoked and leaving
 *   dollar_ztrigger_invoked is one part of that.
 */
#define TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED(STATE)										\
{																\
	GBLREF	boolean_t	dollar_ztrigger_invoked;									\
	GBLREF	trans_num	local_tn;											\
	GBLREF	gv_namehead	*gvt_tp_list;											\
	GBLREF	sgm_info	*first_sgm_info;										\
	GBLREF	tp_region	*tp_reg_list;											\
	GBLREF	trans_num	tstart_local_tn;	/* copy of global variable "local_tn" at op_tstart time */		\
	DEBUG_ONLY(GBLDEF	boolean_t	was_dollar_ztrigger_invoked);							\
	DEBUG_ONLY(GBLDEF	boolean_t	were_gvt_triggers_read_this_tn);						\
	DBGTRIGR_ONLY(char	gvnamestr[MAX_MIDENT_LEN + 1];)									\
	DBGTRIGR_ONLY(uint4	lcl_csadztrig;)											\
	DBGTRIGR_ONLY(uint4	lcl_dztrig;)											\
																\
	gv_namehead		*gvnh, *lcl_hasht_tree;										\
	cw_set_element		*cse;												\
	sgmnt_addrs		*csa;												\
	sgm_info		*si;												\
	gvt_trigger_t		*gvt_trigger;											\
	tp_region               *tr;												\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	DEBUG_ONLY(was_dollar_ztrigger_invoked = dollar_ztrigger_invoked;)							\
	DEBUG_ONLY(were_gvt_triggers_read_this_tn = TREF(gvt_triggers_read_this_tn);)						\
	/* $ZTRIGGER() was invoked. Clean up csa->db_dztrigger_cycle as necessary. */						\
	DBGTRIGR((stderr, "INVALIDATE_TRIG_CYCLE(): %s:%d t_tries %d, $tlevel=%d state=%d wasDZ=%d wasTREAD=%d\n",		\
				__FILE__, __LINE__, t_tries, dollar_tlevel, STATE,						\
				was_dollar_ztrigger_invoked, were_gvt_triggers_read_this_tn));					\
	if (dollar_ztrigger_invoked)												\
	{															\
		if ((STATE == TP_COMMIT) || (STATE == TP_ROLLBACK))								\
		{	/* Reset csa->db_dztrigger_cycle to zero. This is needed so that globals updated after the commit or	\
			 * rollback do not re-read triggers when NOT necessary. Such a case is possible if for instance a	\
			 * $ZTRIGGER() happened in a sub-transaction that got rolled back. Though no actual trigger change	\
			 * happened, csa->db_dztrigger_cycle will be incremented and hence gvt(s) whose db_dztrigger_cycle does	\
			 * not match will now re-read triggers. We don't expect $ZTRIGGER() to be frequent. So, it's okay to go	\
			 * through tp_reg_list											\
			 */													\
			for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)							\
			{													\
				csa = &FILE_INFO(tr->reg)->s_addrs;								\
				csa->db_dztrigger_cycle = 0;									\
				/* This is safe to do here for commit because tp_tend() has already been done. This is safe	\
				 * for a complete rollback as well because the transaction is now complete.			\
				 */												\
				csa->incr_db_trigger_cycle = FALSE;								\
			}													\
		} else														\
		{	/* In case of a restart or incremental rollback, reset the csa's db_dztrigger_cycle to force a		\
			 * cycle mismatch in the next try or update (rollback)							\
			 */													\
			for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)						\
			{													\
				csa = si->tp_csa;										\
				assert(!csa->db_dztrigger_cycle || ((csa->db_dztrigger_cycle) && (csa->incr_db_trigger_cycle)));\
				DBGTRIGR_ONLY(lcl_csadztrig = csa->db_dztrigger_cycle;)						\
				if (csa->db_dztrigger_cycle)									\
					csa->db_dztrigger_cycle = 1;								\
				DBGTRIGR((stderr, "INVALIDATE_TRIG_CYCLE(): CSA is %s at %d, was %d\n",				\
						csa->region->rname, csa->db_dztrigger_cycle, lcl_csadztrig));			\
			}													\
		}														\
	}															\
	/* Either $ZTRIGGER() was invoked or triggers were read for the first time in this transaction. In order for the next	\
	 * try or update (in case of a rollback) to re-read triggers, force a cycle mismatch as necessary.			\
	 */															\
	if (TREF(gvt_triggers_read_this_tn) || dollar_ztrigger_invoked)								\
	{															\
		for (gvnh = gvt_tp_list; NULL != gvnh; gvnh = gvnh->next_tp_gvnh)						\
		{														\
			assert(gvnh->read_local_tn == local_tn);								\
			/* Always reset db_dztrigger_cycle, but capture current state */					\
			DBGTRIGR_ONLY(lcl_dztrig = gvnh->db_dztrigger_cycle;)							\
			gvnh->db_dztrigger_cycle = 0;										\
			/* For commit and complete rollback the csa and gvt must share the same db_dztrigger_cycle value */	\
			assert(((STATE != TP_COMMIT) && (STATE != TP_ROLLBACK))							\
					|| (gvnh->db_dztrigger_cycle == gvnh->gd_csa->db_dztrigger_cycle));			\
			DBGTRIGR_ONLY(memcpy(gvnamestr, gvnh->gvname.var_name.addr, gvnh->gvname.var_name.len);)		\
			DBGTRIGR_ONLY(gvnamestr[gvnh->gvname.var_name.len]='\0';)						\
			DBGTRIGR((stderr, "INVALIDATE_TRIG_CYCLE(): %s:%d %s dztrig=%d(%d) cycle=%d CSA\n", 			\
					__FILE__, __LINE__, gvnamestr, gvnh->db_dztrigger_cycle, lcl_dztrig, 			\
						gvnh->db_trigger_cycle));							\
			if (STATE == TP_COMMIT)											\
				continue;											\
			/* If in a restart/complete rollback and gvhn's trigger was read since the transaction start, clear its	\
			 * gv_trigger_cycle to force a complete re-read of trigger content in the next try or update (rollback).\
			 * NOTE: While reseting gv_trigger_cycle forces a complete re-read, that only occurs with a cycle	\
			 * mismatch by reseting some combination of gvnh->db_trigger_cycle and gvnh->db_dztrigger_cycle.	\
			 */													\
			if (((STATE == TP_ROLLBACK) || (STATE == TP_RESTART)) && (gvnh->trig_read_tn >= tstart_local_tn) 	\
				&& (NULL != (gvt_trigger = gvnh->gvt_trigger)))	/* in-line assigment */				\
					gvt_trigger->gv_trigger_cycle = 0;							\
			/* If in an incremental rollback, only clear gv_trigger_cycle if the trigger was read in the current 	\
			 * transaction. Do not force a cycle mismatch. A $ZTRIGGER() operation in the current transaction will	\
			 * cause a cycle mismatch. Without any external change - another process doing a $ZTRIGGER() - there is	\
			 * no reason to reload the trigger. Inducing a mismatch would cause GVTR_INIT_AND_TPWRAP_IF_NEEDED	\
			 * to restart when local_tn == GVT->trig_local_tn							\
			 */													\
			if (STATE == TP_INCR_ROLLBACK)										\
			{													\
				if ((gvnh->trig_read_tn == local_tn)		 						\
					&& (NULL != (gvt_trigger = gvnh->gvt_trigger)))	/* in-line assigment */			\
						gvt_trigger->gv_trigger_cycle = 0;						\
				continue;											\
			}													\
			/* For complete rollback and restart always force a cycle mismatch */					\
			gvnh->db_trigger_cycle = gvnh->gd_csa->db_trigger_cycle - 1;						\
		}														\
	}															\
	/* At this point, assert that each gvt's db_dztrigger_cycle is zero */							\
	DEBUG_ONLY(for (gvnh = gvt_tp_list; NULL != gvnh; gvnh = gvnh->next_tp_gvnh))						\
		assert(!gvnh->db_dztrigger_cycle);										\
	/* Now it is safe to clear dollar_ztrigger_invoked and gvt_triggers_read_this_tn */					\
	if (TREF(gvt_triggers_read_this_tn))											\
		TREF(gvt_triggers_read_this_tn) = FALSE;									\
	if (dollar_ztrigger_invoked && ((STATE == TP_COMMIT) || (STATE == TP_ROLLBACK)))					\
		dollar_ztrigger_invoked = FALSE;										\
	assert(!TREF(gvt_triggers_read_this_tn));										\
	assert((FALSE == dollar_ztrigger_invoked) || (STATE == TP_RESTART) || (STATE == TP_INCR_ROLLBACK));			\
	TP_ASSERT_ZTRIGGER_CYCLE_STATUS;											\
}
# ifdef DEBUG
#  define TP_ASSERT_ZTRIGGER_CYCLE_STATUS										\
{	/* At the end of a transaction commit ensure that csa->db_dztrigger_cycle is reset to zero.			\
	 * In the case of trestart and/or rollback, this value could be 1 if $ZTRIGGER() activity happened in		\
	 * the failed try. Because of this it is also possible the value could be 1 after a commit (that is preceded	\
	 * by a restart). Assert this.											\
	 * It's okay not to check if all gvt updated in this transaction also has gvt->db_dztrigger_cycle set		\
	 * back to zero because if they don't there are other asserts that will trip in the subsequent transactions.	\
	 */														\
	GBLREF	sgm_info	*first_sgm_info;									\
															\
	sgm_info		*si;											\
															\
	for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)							\
		assert((0 == si->tp_csa->db_dztrigger_cycle) || (1 == si->tp_csa->db_dztrigger_cycle));			\
}
# else
#  define TP_ASSERT_ZTRIGGER_CYCLE_STATUS
# endif	/* #ifdef DEBUG */
#endif	/* #ifdef GTM_TRIGGER */

#define INVOKE_RESTART	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TPRETRY);

/* the following macros T_BEGIN_READ_NONTP_OR_TP and T_BEGIN_SETORKILL_NONTP_OR_TP are similar except for one difference
 * which is that for the SETORKILL case, sgm_info_ptr->update_trans needs to be set. They need to be maintained
 * in parallel always. The reason for choosing this duplication is because it saves us an if check which would have
 * otherwise been had the two macros been merged and this is used in database code where performance is a concern.
 */
/* the macro below uses "dollar_tlevel", "t_err" and "sgm_info_ptr" */
#define T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_CODE)										\
{															\
	GBLREF	sgm_info	*sgm_info_ptr;										\
	GBLREF	sgmnt_addrs	*cs_addrs;										\
	GBLREF	uint4		t_err;											\
															\
	if (!dollar_tlevel)												\
		t_begin(ERR_CODE, UPDTRNS_DB_UPDATED_MASK);								\
	else														\
	{														\
		t_err = ERR_CODE;											\
		assert((NULL != sgm_info_ptr) && (cs_addrs->sgm_info_ptr == sgm_info_ptr));				\
		sgm_info_ptr->update_trans |= UPDTRNS_DB_UPDATED_MASK;							\
	}														\
}

/* the macro below uses "dollar_tlevel", "t_err" */
#define T_BEGIN_READ_NONTP_OR_TP(ERR_CODE)										\
{															\
	GBLREF	uint4		t_err;											\
	GBLREF	sgm_info	*sgm_info_ptr;										\
	GBLREF	sgmnt_addrs	*cs_addrs;										\
															\
	if (!dollar_tlevel)												\
		t_begin(ERR_CODE, 0);											\
	else														\
	{														\
		assert((NULL != sgm_info_ptr) && (cs_addrs->sgm_info_ptr == sgm_info_ptr));				\
		t_err = ERR_CODE;											\
	}														\
}

/* The following GBLREFs are needed by the IS_TP_AND_FINAL_RETRY macro */
GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned int	t_tries;

#define	IS_TP_AND_FINAL_RETRY 		(dollar_tlevel && (CDB_STAGNATE <= t_tries))

#define	TP_REL_CRIT_ALL_REG							\
{										\
	sgmnt_addrs		*csa;						\
	tp_region		*tr;						\
										\
	GBLREF	tp_region	*tp_reg_list;					\
										\
	for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)			\
	{									\
		assert(tr->reg->open);						\
		if (!tr->reg->open)						\
			continue;						\
		csa = (sgmnt_addrs *)&FILE_INFO(tr->reg)->s_addrs;		\
		assert(!csa->hold_onto_crit);					\
		if (csa->now_crit)						\
			rel_crit(tr->reg);					\
	}									\
	assert(!have_crit(CRIT_HAVE_ANY_REG));					\
}

#define	TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK									\
{														\
	GBLREF	boolean_t	mupip_jnl_recover;								\
														\
	assert(dollar_tlevel);											\
	assert(CDB_STAGNATE == t_tries);									\
	/* mupip_jnl_recovery operates with t_tries=CDB_STAGNATE so we should not adjust t_tries		\
	 * In that case, because we have standalone access, we don't expect anyone else to interfere with us 	\
	 * and cause a restart, but if they do, TPNOTACID_CHECK (below) gives a TPNOTACID message.		\
	 */													\
	if (!mupip_jnl_recover)											\
	{													\
		assert(dollar_trestart >= TREF(tp_restart_dont_counts));					\
		t_tries = CDB_STAGNATE - 1;									\
		DEBUG_ONLY(if (0 == TREF(tp_restart_dont_counts)))						\
			DEBUG_ONLY((TREF(tp_restart_dont_counts))++);	/* can live with one too many */	\
		DEBUG_ONLY(if (0 < TREF(tp_restart_dont_counts)))						\
			DEBUG_ONLY((TREF(tp_restart_dont_counts)) = -(TREF(tp_restart_dont_counts)));		\
	}													\
}

#define TPNOTACID_DEFAULT_TIME	2 * MILLISECS_IN_SEC	/* default for tpnotacidtime */
#define TPNOTACID_MAX_TIME	30	/* maximum (in seconds)for tpnotacidtime */
#define TPTIMEOUT_MAX_TIME	60	/* maximum (in seconds) for dollar_zmaxtptime - enforced in gtm_env_init, not in op_svput */

#define	TPNOTACID_CHECK(CALLER_STR)												\
{																\
	GBLREF	boolean_t	mupip_jnl_recover;										\
	mval		zpos;													\
																\
	if (IS_TP_AND_FINAL_RETRY)												\
	{															\
		TP_REL_CRIT_ALL_REG;												\
		assert(!mupip_jnl_recover);											\
		TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK;										\
		getzposition(&zpos);												\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_TPNOTACID, 7, LEN_AND_LIT(CALLER_STR), zpos.str.len, zpos.str.addr, \
			 (TREF(tpnotacidtime)).str.len, (TREF(tpnotacidtime)).str.addr, dollar_trestart);			\
	}															\
}

#define SAVE_REGION_INFO(SAVE_KEY, SAVE_TARGET, SAVE_CUR_REG, SAVE_SI_PTR, SAVE_JNLPOOL)	\
MBSTART {											\
	SAVE_TARGET = gv_target;								\
	SAVE_CUR_REG = gv_cur_region;								\
	SAVE_SI_PTR = sgm_info_ptr;								\
	SAVE_JNLPOOL = jnlpool;									\
	assert(NULL != gv_currkey);								\
	assert((SIZEOF(gv_key) + gv_currkey->end) <= SIZEOF(SAVE_KEY));				\
	memcpy(&SAVE_KEY[0], gv_currkey, SIZEOF(gv_key) + gv_currkey->end);			\
} MBEND
#define RESTORE_REGION_INFO(SAVE_KEY, SAVE_TARGET, SAVE_CUR_REG, SAVE_SI_PTR, SAVE_JNLPOOL)	\
MBSTART {											\
	gv_target = SAVE_TARGET;								\
	sgm_info_ptr = SAVE_SI_PTR;								\
	/* check no keysize expansion occurred inside gvcst_root_search */			\
	assert(gv_currkey->top == SAVE_KEY[0].top);						\
	memcpy(gv_currkey, &SAVE_KEY[0], SIZEOF(gv_key) + SAVE_KEY[0].end);			\
	if (NULL != SAVE_CUR_REG)								\
	{											\
		TP_CHANGE_REG_IF_NEEDED(SAVE_CUR_REG);						\
	} else											\
	{											\
		gv_cur_region = NULL;								\
		cs_data = NULL;									\
		cs_addrs = NULL;								\
	}											\
	jnlpool = SAVE_JNLPOOL;									\
} MBEND

/* Any retry transition where the destination state is the 3rd retry, we don't want to release crit, i.e. for 2nd to 3rd retry
 * transition or 3rd to 3rd retry transition. Therefore we need to release crit only if (CDB_STAGNATE - 1) > t_tries.
 */
#define NEED_TO_RELEASE_CRIT(T_TRIES, STATUS)		(((CDB_STAGNATE - 1) > T_TRIES)	|| (cdb_sc_instancefreeze == STATUS))

void tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1);
void tp_clean_up(tp_cleanup_state clnup_state);
void tp_cw_list(cw_set_element **cs);
void tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1);
void tp_incr_clean_up(uint4 newlevel);
void tp_set_sgm(void);
void tp_start_timer_dummy(int4 timeout_seconds);
void tp_clear_timeout_dummy(void);
void tp_timeout_action_dummy(void);

tp_region	*insert_region(gd_region *reg, tp_region **reg_list, tp_region **reg_free_list, int4 size);
boolean_t	tp_tend(void);
boolean_t	tp_crit_all_regions(void);

#endif

#define	GVNAME_UNKNOWN		"*BITMAP"
#define GVNAME_DIRTREE		"*DIR"

static readonly char		gvname_unknown[] = GVNAME_UNKNOWN;
static readonly int4		gvname_unknown_len = STR_LIT_LEN(GVNAME_UNKNOWN);
static readonly char		gvname_dirtree[] = GVNAME_DIRTREE;
static readonly int4		gvname_dirtree_len = STR_LIT_LEN(GVNAME_DIRTREE);
