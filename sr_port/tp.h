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

#ifndef __TP_H__
#define __TP_H__

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
		gd_id		file_id; 	/* both for VMS and UNIX */
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
	} else										\
	{										\
		assert(NULL == si->jnl_list);						\
		assert(NULL == si->format_buff_list);					\
		assert(NULL == si->jnl_tail);						\
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

#ifdef UNIX
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
#endif

typedef struct ua_list_struct
{
	struct ua_list_struct
			*next_ua;
	char		*update_array;
	uint4		update_array_size;
} ua_list;

#define TP_MAX_NEST	127

/* Note gv_orig_key[i] is assigned to tp_pointer->orig_key which then tries to dereference the "begin", "end", "prev", "top"
 * 	fields like it were a gv_currkey pointer. Since these members are 2-byte fields, we need atleast 2 byte alignment.
 * We want to be safer and hence give 4-byte alignment by declaring the array as an array of integers.
 */
typedef struct gv_orig_key_struct
{
	int4	gv_orig_key[TP_MAX_NEST + 1][DIVIDE_ROUND_UP((SIZEOF(gv_key) + MAX_KEY_SZ + 1), SIZEOF(int4))];
}gv_orig_key_array;

GBLREF	block_id	t_fail_hist_blk[];
GBLREF	gd_region	*tp_fail_hist_reg[];
GBLREF	gv_namehead	*tp_fail_hist[];
GBLREF	int4		tp_fail_n;
GBLREF	int4		tp_fail_level;
GBLREF	trans_num	tp_fail_histtn[], tp_fail_bttn[];

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
	GBLREF	jnlpool_addrs	jnlpool;								\
	GBLREF	unsigned int	t_tries;								\
	GBLREF	unsigned int	dollar_tlevel;								\
	GBLREF	sgmnt_addrs	*cs_addrs;								\
													\
	uint4			curidx;									\
	trans_restart_hist_t	*this_restart_hist;							\
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
	if (NULL != jnlpool.jnlpool_ctl)								\
		this_restart_hist->seq_or_tn.jnl_seqno = jnlpool.jnlpool_ctl->jnl_seqno;		\
	else												\
		this_restart_hist->seq_or_tn.curr_tn = (NULL != cs_addrs) ? cs_addrs->ti->curr_tn : 0;	\
	this_restart_hist->csa = dollar_tlevel ? NULL : cs_addrs;					\
}
#else
# define TRACE_TRANS_RESTART(RETRY_CODE)
#endif	/* DEBUG */

#define TP_TRACE_HIST(X, Y) 										\
{													\
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

#define TP_TRACE_HIST_MOD(X, Y, N, CSD, HISTTN, BTTN, LEVEL)						\
{													\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	if (TREF(tprestart_syslog_delta))								\
	{												\
		tp_fail_hist_reg[t_tries] = gv_cur_region;						\
		t_fail_hist_blk[t_tries] = ((block_id)X);						\
		tp_fail_hist[t_tries] = (gv_namehead *)(((int)X & ~(-BLKS_PER_LMAP)) ? Y : NULL); 	\
		(CSD)->tp_cdb_sc_blkmod[(N)]++;								\
		tp_fail_n = (N);									\
		tp_fail_level = (LEVEL);								\
		tp_fail_histtn[t_tries] = (HISTTN);							\
		tp_fail_bttn[t_tries] = (BTTN);								\
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
#define TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED(INCREMENTAL, COMMIT)								\
{																\
	GBLREF	boolean_t	dollar_ztrigger_invoked;									\
	GBLREF	trans_num	local_tn;											\
	GBLREF	gv_namehead	*gvt_tp_list;											\
	GBLREF	sgm_info	*first_sgm_info;										\
																\
	gv_namehead		*gvnh, *lcl_hasht_tree;										\
	cw_set_element		*cse;												\
	sgmnt_addrs		*csa;												\
	sgm_info		*si;												\
	gvt_trigger_t		*gvt_trigger;											\
	DEBUG_ONLY(boolean_t	matched_one_gvnh;)										\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	if (dollar_ztrigger_invoked)												\
	{	/* There was at least one region where a $ZTRIGGER() was invoked. */						\
		dollar_ztrigger_invoked = FALSE;										\
		/* Phase 1: Adjust trigger/$ztrigger related fields for all gv_target read/updated in this transaction */	\
		if (COMMIT)													\
		{	/* Reset csa->db_dztrigger_cycle and gvt->db_dztrigger_cycle to zero. This is needed so that globals 	\
			 * updated after the TCOMMIT don't re-read triggers when NOT necessary. Such a case is possible if	\
			 * for instance a $ZTRIGGER() happened in a sub-transaction that got rolled back. Though no actual	\
			 * trigger change happened, csa->db_dztrigger_cycle will be incremented and hence gvt(s) whose 		\
			 * db_dztrigger_cycle does not match will now re-read triggers. We don't expect $ZTRIGGER() to be	\
			 * frequent. So, it's okay to go through the list of gvt 						\
			 */													\
			for (gvnh = gvt_tp_list; NULL != gvnh; gvnh = gvnh->next_tp_gvnh)					\
			{													\
				gvnh->db_dztrigger_cycle = 0;									\
				gvnh->gd_csa->db_dztrigger_cycle = 0;								\
			}													\
		} else														\
		{														\
			/* Now that the transaction is rolled back/restarted, invalidate gvt->gvt_trigger->gv_trigger_cycle	\
			 * (by resetting it to zero) for all gvt read/updated in this transaction. Note that even though we	\
			 * reset db_trigger_cycle for non-incremental rollbacks/restarts and increment db_dztrigger_cycle	\
			 * for incremental rollbacks we still need to reset gv_trigger_cycle as otherwise gvtr_init will find 	\
			 * that gv_trigger_cycle has NOT changed since it was updated last and will NOT do any trigger reads.	\
			 */													\
			for (gvnh = gvt_tp_list; NULL != gvnh; gvnh = gvnh->next_tp_gvnh)					\
			{													\
				assert(gvnh->read_local_tn == local_tn);							\
				if (NULL != (gvt_trigger = gvnh->gvt_trigger))							\
					gvt_trigger->gv_trigger_cycle = 0;							\
				if (!INCREMENTAL)										\
				{	/* TROLLBACK(0) or TRESTART. Reset db_dztrigger_cycle to 0 since we are going to start 	\
					 * a new transaction. But, we want to ensure that the new transaction re-reads triggers	\
					 * since any gvt which updated its gvt_trigger in this transaction will be stale as	\
					 * they never got committed.								\
					 */											\
					gvnh->db_dztrigger_cycle = 0;								\
					gvnh->db_trigger_cycle = 0;								\
				}												\
			}													\
		}														\
		/* Phase 2: Adjust trigger/$ztrigger related fields for all csa accessed in this transaction */			\
		if (INCREMENTAL)												\
		{	/* An incremental rollback. Find out if there are still any cse->blk_target containing ^#t updates. 	\
			 * If not, set csa->incr_db_trigger_cycle to FALSE (if already set to TRUE) so that we don't 		\
			 * increment csd->db_trigger_cycle during commit time 							\
			 */													\
			for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)						\
			{													\
				cse = si->first_cw_set;										\
				csa = si->tp_csa;										\
				lcl_hasht_tree = csa->hasht_tree;								\
				if (NULL != lcl_hasht_tree)									\
				{												\
					/* Walk through the cw_set_elements remaining after the incremental rollback to 	\
					 * see if any of them has a ^#t update 							\
					 */											\
					while (NULL != cse)									\
					{											\
						if (lcl_hasht_tree == cse->blk_target)						\
							break;									\
						cse = cse->next_cw_set;								\
					}											\
					if (NULL != cse)									\
						csa->incr_db_trigger_cycle = FALSE;						\
				} else												\
					assert(!csa->incr_db_trigger_cycle);							\
				if (csa->db_dztrigger_cycle)									\
					csa->db_dztrigger_cycle++; /* so that future updates in this TN re-read triggers */	\
			}													\
			/* Keep dollar_ztrigger_invoked as TRUE as the transaction is not yet complete and we need this 	\
			 * variable being TRUE to reset csa->db_dztrigger_cycle and gvt->db_dztrigger_cycle to 0 during 	\
			 * tp_clean_up of this transaction.									\
			 */													\
			dollar_ztrigger_invoked = TRUE;										\
		} else if (!COMMIT)												\
		{	/* This is either a complete rollback or a restart. In either case, set csa->incr_db_trigger_cycle 	\
			 * to FALSE for all csa referenced in this transaction as they are anyways not going to be committed.	\
			 */													\
			for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)						\
			{													\
				si->tp_csa->db_dztrigger_cycle = 0;								\
				si->tp_csa->incr_db_trigger_cycle = FALSE;							\
			}													\
		}														\
	}															\
	/* If gvt_triggers_read_this_tn is TRUE and INCREMENTAL is TRUE, we are in an incremental commit or rollback.		\
	 * In either case, we dont need to worry about resetting gv_target's trigger cycles as we will do this when the		\
	 * outermost commit or rollback (or a restart) occurs. The key is that even if a gv_target gets used in a nested	\
	 * tstart/tcommit that gets incrementally rolled back, it is still part of the gvt_tp_list and hence we can wait	\
	 * to clean the cycle fields at restart/commit/non-incremental-rollback time. In case of a commit, no need to reset	\
	 * the gv_trigger_cycle, db_dztrigger_cycle and db_trigger_cycle fields of gvnh as those are valid going forward.	\
	 */															\
	if (TREF(gvt_triggers_read_this_tn) && !INCREMENTAL)									\
	{															\
		if (!COMMIT)													\
		{	/* There was at least one GVT where ^#t records were read and this is a complete			\
			 * (i.e. non-incremental) transaction restart or rollback. In this case, reset cycle fields		\
			 * in corresponding gv_target to force re-reads of ^#t records of this gvt (if any) in next retry.	\
			 */													\
			DEBUG_ONLY(matched_one_gvnh = TRUE;)									\
			for (gvnh = gvt_tp_list; NULL != gvnh; gvnh = gvnh->next_tp_gvnh)					\
			{													\
				assert(gvnh->read_local_tn == local_tn);							\
				if (gvnh->trig_read_tn != local_tn)								\
					continue;										\
				DEBUG_ONLY(matched_one_gvnh = FALSE;)								\
				if (NULL != (gvt_trigger = gvnh->gvt_trigger))							\
					gvt_trigger->gv_trigger_cycle = 0;							\
				gvnh->db_dztrigger_cycle = 0;									\
				gvnh->db_trigger_cycle = 0;									\
			}													\
			assert(!matched_one_gvnh);	/* we expect at least one gvnh with trig_read_tn == local_tn */		\
		}														\
		TREF(gvt_triggers_read_this_tn) = FALSE;									\
	}															\
}
# ifdef DEBUG
#  define TP_ASSERT_ZTRIGGER_CYCLE_RESET										\
{	/* At the end of a transaction (either because of trestart, complete trollback or tcommit) ensure that 		\
	 * csa->db_dztrigger_cycle is reset to zero. It's okay not to check if all gvt updated in this transaction	\
	 * also has gvt->db_dztrigger_cycle set back to zero because if they don't there are other asserts that 	\
	 * will trip in the subsequent transactions									\
	 */														\
	GBLREF	sgm_info	*first_sgm_info;									\
															\
	sgm_info		*si;											\
															\
	for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)							\
		assert(0 == si->tp_csa->db_dztrigger_cycle);								\
}
# else
#  define TP_ASSERT_ZTRIGGER_CYCLE_RESET
# endif	/* #ifdef DEBUG */
#endif	/* #ifdef GTM_TRIGGER */

#ifdef VMS
/* The error below has special handling in a few condition handlers because it not so much signals an error
   as it does drive the necessary mechanisms to invoke a restart. Consequently this error can be
   overridden by a "real" error. For VMS, the extra parameters are specified to provide "placeholders" on
   the stack in the signal array if a real error needs to be overlayed in place of this one (see example
   code in mdb_condition_handler). The number of extra parameters need to be 2 more than the largest
   number of parameters for an rts_error in tp_restart().
*/
#define INVOKE_RESTART	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TPRETRY, 4, 0, 0, 0, 0, 0, 0, 0, 0);
#else
#define INVOKE_RESTART	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TPRETRY);
#endif

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
	 * In that case, because we have standalone access, we dont expect anyone else to interfere with us 	\
	 * and cause a restart, but if they do, TPNOTACID_CHECK (below) gives a TPNOTACID message.		\
	 */													\
	if (!mupip_jnl_recover)											\
	{													\
		assert(CDB_STAGNATE <= dollar_trestart);							\
		assert(dollar_trestart >= TREF(tp_restart_dont_counts));					\
		t_tries = CDB_STAGNATE - 1;									\
		DEBUG_ONLY(if (0 == TREF(tp_restart_dont_counts)))						\
			DEBUG_ONLY((TREF(tp_restart_dont_counts))++);	/* can live with one too many */	\
		DEBUG_ONLY(if (0 < TREF(tp_restart_dont_counts)))						\
			DEBUG_ONLY((TREF(tp_restart_dont_counts)) = -(TREF(tp_restart_dont_counts)));		\
	}													\
}

#define TPNOTACID_DEFAULT_TIME	2	/* default (in seconds)for tpnotacidtime */
#define TPNOTACID_MAX_TIME	30	/* maximum (in seconds)for tpnotacidtime */
#define TPTIMEOUT_MAX_TIME	60	/* maximum (inseconds) for dollar_zmaxtptime - enforced in gtm_env_init, not in op_svput */

#define	TPNOTACID_CHECK(CALLER_STR)											\
{															\
	GBLREF	boolean_t	mupip_jnl_recover;									\
	mval		zpos;												\
															\
	if (IS_TP_AND_FINAL_RETRY)											\
	{														\
		TP_REL_CRIT_ALL_REG;											\
		assert(!mupip_jnl_recover);										\
		TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK;									\
		getzposition(&zpos);											\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TPNOTACID, 4, LEN_AND_LIT(CALLER_STR), zpos.str.len,	\
				zpos.str.addr);										\
	}														\
}

/* Any retry transition where the destination state is the 3rd retry, we don't want to release crit, i.e. for 2nd to 3rd retry
 * transition or 3rd to 3rd retry transition. Therefore we need to release crit only if (CDB_STAGNATE - 1) > t_tries.
 */
#define NEED_TO_RELEASE_CRIT(T_TRIES, STATUS)		(((CDB_STAGNATE - 1) > T_TRIES)					\
							 UNIX_ONLY(|| cdb_sc_instancefreeze == STATUS))

void tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1);
void tp_clean_up(boolean_t rollback_flag);
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
