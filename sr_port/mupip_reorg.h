/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUPIP_REORG_DEFINED

/* MUPIP REORG uses this macro to avoid KILLABANDONED errors in case of a MUPIP STOP (GTM-9400). */
#define	DEFERRED_EXIT_REORG_CHECK									\
	deferred_exit_reorg_check()

/* prototypes */

boolean_t mu_reorg(glist *gl_ptr, glist *exclude_glist_ptr, boolean_t *resume,
			int index_fill_factor, int data_fill_factor, int reorg_op);
# ifdef UNIX
void		mu_swap_root(glist *gl_ptr, int *root_swap_statistic_ptr, block_id upg_mv_block);
block_id	mu_swap_root_blk(glist *gl_ptr, srch_hist *gvt_hist_ptr, srch_hist  *dir_hist_ptr, kill_set *kill_set_list,
			trans_num curr_tn, block_id upg_mv_block);
block_id	swap_root_or_directory_block(int parent_blk_lvl, int level, srch_hist *dir_hist_ptr, block_id child_blk_id,
		sm_uc_ptr_t child_blk_ptr, kill_set *kill_set_list, trans_num curr_tn, block_id upg_mv_block);
# endif
enum cdb_sc	mu_clsce(int level, int i_max_fill, int d_max_fill, kill_set *kill_set_ptr, boolean_t *remove_rtsib);
enum cdb_sc	mu_split(int cur_level, int i_max_fill, int d_max_fill, int *blks_created, int *lvls_increased);
enum cdb_sc	mu_swap_blk(int level, block_id *pdest_blk_id, kill_set *kill_set_ptr, glist *exclude_glist_ptr,
			block_id upg_mv_block);
enum cdb_sc	mu_reduce_level(kill_set *kill_set_ptr);
enum cdb_sc	gvcst_expand_any_key (srch_blk_status *blk_stat, sm_uc_ptr_t rec_top, sm_uc_ptr_t expanded_key,
					int *rec_size, int *keylen, int *keycmpc, srch_hist *hist_ptr);
boolean_t in_exclude_list(unsigned char *curr_key_ptr, int key_len, glist *exclude_glist_ptr);
void mupip_reorg(void);

/* Inline functions */

/* MUPIP REORG uses this inline function to avoid KILLABANDONED errors in case of a MUPIP STOP (GTM-9400).
 * This macro checks if there was a MUPIP STOP that was deferred and if "cs_data->kill_in_prog" is 0.
 * If so, it should be safe to handle the deferred signal without any KILLABANDONED error likelihood.
 */
static inline void deferred_exit_reorg_check(void)
{
	GBLREF	sgmnt_data_ptr_t	cs_data;
	GBLREF	VSIG_ATOMIC_T		forced_exit;

	/* Since the DEFERRED_EXIT_REORG_CHECK macro invokes this function and is called from mu_reorg.c and mu_swap_root.c
	 * at various points in the process life time, we want to have the "forced_exit" check done first as it would be
	 * a quick check for the fast path. In case "forced_exit" is non-zero, then we have the potential of a MUPIP STOP
	 * so in that case, do the more heavyweight DEFERRED_SIGNAL_HANDLING_CHECK macro invocation.
	 */
	if (forced_exit && !cs_data->kill_in_prog)
	{
		if (INTRPT_IN_KILL_CLEANUP == intrpt_ok_state)
		{	/* no KIP & only set INTRPT_IN_KILL_CLEANUP if OK_TO_INTERRUPT */
			ENABLE_INTERRUPTS(INTRPT_IN_KILL_CLEANUP, INTRPT_OK_TO_INTERRUPT);
		}
		DEFERRED_SIGNAL_HANDLING_CHECK;
	}
}

#define  MUPIP_REORG_DEFINED
#endif
