/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
															\
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
	boolean_t		fresh_start;
	int4			crash_count;
	boolean_t		backup_block_saved;
	sgmnt_addrs		*kip_csa;
	int			tmp_cw_set_depth;	/* used only #ifdef DEBUG. see comments for tmp_cw_set_depth in "tp_tend" */
	uint4			tot_jrec_size;		/* maximum journal space needs for this transaction */
} sgm_info;

typedef struct
{
#ifdef	BIGENDIAN
	unsigned	flag	 : 1;
	unsigned	cw_index : 15;
	unsigned	next_off : 16;
#else
	unsigned	next_off : 16;
	unsigned	cw_index : 15;
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

GBLREF	int4		tprestart_syslog_delta;
GBLREF	block_id	t_fail_hist_blk[];
GBLREF	gd_region	*tp_fail_hist_reg[];
GBLREF	gv_namehead	*tp_fail_hist[];
GBLREF	int4		tp_fail_n;
GBLREF	int4		tp_fail_level;
GBLREF	trans_num	tp_fail_histtn[], tp_fail_bttn[];

#define TP_TRACE_HIST(X, Y) 										\
{													\
	if (tprestart_syslog_delta)									\
	{												\
		tp_fail_hist_reg[t_tries] = gv_cur_region;						\
		t_fail_hist_blk[t_tries] = ((block_id)X); 						\
		tp_fail_hist[t_tries] = (gv_namehead *)(((int)X & ~(-BLKS_PER_LMAP)) ? Y : NULL);	\
	}												\
}

#define TP_TRACE_HIST_MOD(X, Y, n, csd, histtn, bttn, level)						\
{													\
	if (tprestart_syslog_delta)									\
	{												\
		tp_fail_hist_reg[t_tries] = gv_cur_region;						\
		t_fail_hist_blk[t_tries] = ((block_id)X);						\
		tp_fail_hist[t_tries] = (gv_namehead *)(((int)X & ~(-BLKS_PER_LMAP)) ? Y : NULL); 	\
		(csd)->tp_cdb_sc_blkmod[(n)]++;								\
		tp_fail_n = (n);									\
		tp_fail_level = (level);								\
		tp_fail_histtn[t_tries] = (histtn);							\
		tp_fail_bttn[t_tries] = (bttn);								\
	}												\
}

#define	ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(first_tp_srch_status, sgm_info_ptr)	\
{											\
	assert(NULL == (first_tp_srch_status) 						\
		|| ((first_tp_srch_status) >= (sgm_info_ptr)->first_tp_hist		\
			&& (first_tp_srch_status) < (sgm_info_ptr)->last_tp_hist));	\
}

#define	SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status)				\
{	/* set wc_blocked if final retry and cache related failure status */		\
	if (CDB_STAGNATE <= t_tries)							\
	{										\
		GBLREF	boolean_t	is_uchar_wcs_code[];				\
		GBLREF	boolean_t	is_lchar_wcs_code[];				\
			boolean_t	is_wcs_code = FALSE;				\
											\
		if (ISALPHA_ASCII(status))						\
		{									\
			if (status > 'Z')						\
				is_wcs_code = is_lchar_wcs_code[status - 'a'];		\
			else								\
				is_wcs_code = is_uchar_wcs_code[status - 'A'];		\
		}									\
		if (is_wcs_code)							\
		{									\
			SET_TRACEABLE_VAR(csa->hdr->wc_blocked, TRUE);			\
			BG_TRACE_PRO_ANY(csa, wc_blocked_wcs_cdb_sc_final_retry);	\
		}									\
	}										\
}

#define	TP_RETRY_ACCOUNTING(csa, cnl, status)						\
{											\
	GBLREF	uint4		dollar_trestart;					\
											\
	switch (dollar_trestart)							\
	{										\
		case 0:									\
			INCR_GVSTATS_COUNTER(csa, cnl, n_tp_cnflct_retries_0, 1);	\
			break;								\
		case 1:									\
			INCR_GVSTATS_COUNTER(csa, cnl, n_tp_cnflct_retries_1, 1);	\
			break;								\
		case 2:									\
			INCR_GVSTATS_COUNTER(csa, cnl, n_tp_cnflct_retries_2, 1);	\
			break;								\
		case 3:									\
			INCR_GVSTATS_COUNTER(csa, cnl, n_tp_cnflct_retries_3, 1);	\
			break;								\
		default:								\
			INCR_GVSTATS_COUNTER(csa, cnl, n_tp_cnflct_retries_4, 1);	\
			break;								\
	}										\
}

#define PREV_OFF_INVALID -1

/* JNL_FILE_TAIL_PRESERVE macro indicates maximum number of bytes to ensure allocated at the end of the journal file
 * 	 to store the journal records that will be written whenever the journal file gets closed.
 * (i)	 Any process closing the journal file needs to write at most one PINI, one EPOCH, one PFIN and one EOF record
 *	 In case of wcs_recover extra INCTN will be written
 * (ii)	 We may need to give room for twice the above space to accommodate the EOF writing by a process that closes the journal
 *	 and the EOF writing by the first process that reopens it and finds no space left and switches to a new journal.
 * (iii) We may need to write one ALIGN record at the most since the total calculated from (i) and (ii) above is
 * 	   less than the minimum alignsize that we support (asserted before using JNL_FILE_TAIL_PRESERVE in macros below)
 * 	   The variable portion of this ALIGN record can get at the most equal to the maximum of the sizes of the
 * 	   PINI/EPOCH/PFIN/EOF record. (We know PINI_RECLEN is maximum of EPOCH_RECLEN, PFIN_RECLEN, EOF_RECLEN)
 */
#define	JNL_FILE_TAIL_PRESERVE	(MIN_ALIGN_RECLEN + (PINI_RECLEN + EPOCH_RECLEN + INCTN_RECLEN + 		\
								PFIN_RECLEN + EOF_RECLEN) * 2 + PINI_RECLEN)

#define TOTAL_TPJNL_REC_SIZE(total_jnl_rec_size, si, csa)							\
{														\
	DEBUG_ONLY(si->tmp_cw_set_depth = si->cw_set_depth;)	/* save a copy to check later in "tp_tend" */	\
	total_jnl_rec_size = si->total_jnl_rec_size;								\
	if (csa->jnl_before_image)										\
		total_jnl_rec_size += (si->cw_set_depth * csa->pblk_align_jrecsize);				\
	/* Since we have already taken into account an align record per journal record and since the size of	\
	 * an align record will be < (size of the journal record written + fixed-size of align record)		\
	 * we can be sure we won't need more than twice the computed space.					\
	 * JNL_FILE_TAIL_PRESERVE is to give allowance for space needed at end of journal file 			\
	 * in case journal file close is needed 								\
	 */													\
	assert(JNL_FILE_TAIL_PRESERVE < (JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE));					\
	si->total_jnl_rec_size = total_jnl_rec_size = (total_jnl_rec_size * 2) + (uint4)JNL_FILE_TAIL_PRESERVE;	\
}

#define MIN_TOTAL_NONTPJNL_REC_SIZE 	(PINI_RECLEN + MIN_ALIGN_RECLEN + INCTN_RECLEN + MIN_ALIGN_RECLEN)

/* This macro gives a pessimistic estimate on the total journal record size needed.
 * The side effect is that we might end up with a journal file extension when it was actually not needed.
 */
#define TOTAL_NONTPJNL_REC_SIZE(total_jnl_rec_size, non_tp_jfb_ptr, csa, tmp_cw_set_depth)			\
{														\
	total_jnl_rec_size = (non_tp_jfb_ptr->record_size + (uint4)MIN_TOTAL_NONTPJNL_REC_SIZE);		\
	if (csa->jnl_before_image)										\
		/* One PBLK record for each gds block changed by the transaction */				\
		total_jnl_rec_size += (tmp_cw_set_depth * csa->pblk_align_jrecsize);			\
	if (write_after_image)											\
		total_jnl_rec_size += (uint4)MIN_AIMG_RECLEN + csa->hdr->blk_size + (uint4)MIN_ALIGN_RECLEN;	\
	/* Since we have already taken into account an align record per journal record and since the size of	\
	 * an align record will be < (size of the journal record written + fixed-size of align record)		\
	 * we can be sure we won't need more than twice the computed space.					\
	 * JNL_FILE_TAIL_PRESERVE is to give allowance for space needed at end of journal file 			\
	 * in case journal file close is needed 								\
	 */													\
	assert(JNL_FILE_TAIL_PRESERVE < (JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE));					\
	total_jnl_rec_size = total_jnl_rec_size * 2 + (uint4)JNL_FILE_TAIL_PRESERVE;				\
}

#define INVALIDATE_CLUE(cse) 					\
{								\
	off_chain	macro_chain;				\
								\
	assert(cse->blk_target);				\
	cse->blk_target->clue.end = 0;				\
	macro_chain = *(off_chain *)&cse->blk_target->root;	\
	if (macro_chain.flag)					\
		cse->blk_target->root = 0;			\
}

/* freeup killset starting from the link 'ks' */

#define FREE_KILL_SET(si, ks)					\
{								\
	kill_set	*macro_next_ks;				\
	for (; ks; ks = macro_next_ks)				\
	{							\
		macro_next_ks = ks->next_kill_set;		\
		free(ks);					\
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

#define FREE_GBL_TLVL_INFO(gti)							\
{										\
	int	macro_cnt;							\
	for (macro_cnt = 0; gti; gti = gti->next_global_tlvl_info)		\
		macro_cnt++;							\
	if (macro_cnt)								\
		free_last_n_elements(global_tlvl_info_list, macro_cnt);		\
}

#ifdef GTM_TRIGGER
#define INVALIDATE_TRIGGER_CYCLES_IF_NEEDED(INCREMENTAL, COMMIT)								\
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
																\
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
			 * reset db_trigger_cycle (to -1) for non-incremental rollbacks/restarts and increment db_dztrigger_cycle\
			 * for incremental rollbacks we still need to reset gv_trigger_cycle as otherwise gvtr_init will find 	\
			 * that gv_trigger_cycle has NOT changed since it was updated last and will NOT do any trigger reads	\
			 */													\
			for (gvnh = gvt_tp_list; NULL != gvnh; gvnh = gvnh->next_tp_gvnh)					\
			{													\
				assert(gvnh->read_local_tn == local_tn);							\
				if (NULL != gvnh->gvt_trigger)									\
					((gvt_trigger_t *)(gvnh->gvt_trigger))->gv_trigger_cycle = 0;				\
				if (!INCREMENTAL)										\
				{	/* TROLLBACK(0) or TRESTART. Reset db_dztrigger_cycle to 0 since we are going to start 	\
					 * a new transaction. But, we want to ensure that the new transaction re-read triggers	\
					 * since any gvt which updated its gvt_trigger in this transaction will be stale as	\
					 * they never got committed								\
					 */											\
					gvnh->db_dztrigger_cycle = 0;								\
					gvnh->db_trigger_cycle = (uint4)-1;							\
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
				{												\
					assert(!csa->incr_db_trigger_cycle);							\
				}												\
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
}
# ifdef DEBUG
#  define ASSERT_ZTRIGGER_CYCLE_RESET											\
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
#  define ASSERT_ZTRIGGER_CYCLE_RESET
# endif
#endif

#ifdef VMS
/* The error below has special handling in a few condition handlers because it not so much signals an error
   as it does drive the necessary mechanisms to invoke a restart. Consequently this error can be
   overridden by a "real" error. For VMS, the extra parameters are specified to provide "placeholders" on
   the stack in the signal array if a real error needs to be overlayed in place of this one (see example
   code in mdb_condition_handler). The number of extra parameters need to be 2 more than the largest
   number of parameters for an rts_error in tp_restart().
*/
#define INVOKE_RESTART	rts_error(VARLSTCNT(6) ERR_TPRETRY, 4, 0, 0, 0, 0, 0, 0, 0, 0);
#else
#define INVOKE_RESTART	rts_error(VARLSTCNT(1) ERR_TPRETRY);
#endif

/* the following macros T_BEGIN_READ_NONTP_OR_TP and T_BEGIN_SETORKILL_NONTP_OR_TP are similar except for one difference
 * which is that for the SETORKILL case, sgm_info_ptr->update_trans needs to be set. They need to be maintained
 * in parallel always. The reason for choosing this duplication is because it saves us an if check which would have
 * otherwise been had the two macros been merged and this is used in database code where performance is a concern.
 */
/* the macro below uses "dollar_tlevel", "t_err" and "sgm_info_ptr" */
#define T_BEGIN_SETORKILL_NONTP_OR_TP(err_code)										\
{															\
	GBLREF	sgm_info	*sgm_info_ptr;										\
	GBLREF	sgmnt_addrs	*cs_addrs;										\
	GBLREF	uint4		t_err;											\
	error_def(err_code);												\
															\
	if (!dollar_tlevel)												\
		t_begin(err_code, UPDTRNS_DB_UPDATED_MASK);								\
	else														\
	{														\
		t_err = err_code;											\
		assert((NULL != sgm_info_ptr) && (cs_addrs->sgm_info_ptr == sgm_info_ptr));				\
		sgm_info_ptr->update_trans |= UPDTRNS_DB_UPDATED_MASK;							\
	}														\
}

/* the macro below uses "dollar_tlevel", "t_err" */
#define T_BEGIN_READ_NONTP_OR_TP(err_code)										\
{															\
	GBLREF	uint4		t_err;											\
	GBLREF	sgm_info	*sgm_info_ptr;										\
	GBLREF	sgmnt_addrs	*cs_addrs;										\
															\
	error_def(err_code);												\
															\
	if (!dollar_tlevel)												\
		t_begin(err_code, 0);											\
	else														\
	{														\
		assert((NULL != sgm_info_ptr) && (cs_addrs->sgm_info_ptr == sgm_info_ptr));				\
		t_err = err_code;											\
	}														\
}

/* The following GBLREFs are needed by the IS_TP_AND_FINAL_RETRY macro */
GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned int	t_tries;

#define	IS_TP_AND_FINAL_RETRY 	(dollar_tlevel && (CDB_STAGNATE <= t_tries))

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
	/* In case of journal recovery, it operates at t_tries=CDB_STAGNATE but we should not adjust t_tries	\
	 * in that case and it is ok to not print the TPNOTACID message in that case. Since we have standalone	\
	 * access, we dont expect anyone else to interfere with us and cause a restart anyways.			\
	 */													\
	if (!mupip_jnl_recover)											\
		t_tries = CDB_STAGNATE - 1;									\
}

#define	TPNOTACID_CHECK(CALLER_STR)											\
{															\
	mval		zpos;												\
															\
	if (IS_TP_AND_FINAL_RETRY)											\
	{														\
		TP_REL_CRIT_ALL_REG;											\
		TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK;									\
		getzposition(&zpos);											\
		gtm_putmsg(VARLSTCNT(6) ERR_TPNOTACID, 4, LEN_AND_LIT(CALLER_STR), zpos.str.len, zpos.str.addr);	\
		send_msg(VARLSTCNT(6) ERR_TPNOTACID, 4, LEN_AND_LIT(CALLER_STR), zpos.str.len, zpos.str.addr);		\
	}														\
}

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
