/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUPIP_REORG_DEFINED

#define	DEFERRED_EXIT_REORG_CHECK									\
	deferred_exit_reorg_check()
/* prototypes */

boolean_t mu_reorg(glist *gl_ptr, glist *exclude_glist_ptr, boolean_t *resume,
			int index_fill_factor, int data_fill_factor, int reorg_op, int min_level);
# ifdef UNIX
void		mu_swap_root(glist *gl_ptr, int *root_swap_statistic_ptr, block_id upg_mv_block);
block_id	mu_swap_root_blk(glist *gl_ptr, srch_hist *gvt_hist_ptr, srch_hist  *dir_hist_ptr, kill_set *kill_set_list,
			trans_num curr_tn, block_id upg_mv_block);
block_id	swap_root_or_directory_block(int parent_blk_lvl, int level, srch_hist *dir_hist_ptr, block_id child_blk_id,
		sm_uc_ptr_t child_blk_ptr, kill_set *kill_set_list, trans_num curr_tn, block_id upg_mv_block);
# endif
enum cdb_sc	mu_clsce(int level, int i_max_fill, int d_max_fill, kill_set *kill_set_ptr, int *pending_levels);
enum cdb_sc	mu_split(int cur_level, int i_max_fill, int d_max_fill, int *blks_created, int *lvls_increased,
			int *max_rightblk_lvl);
enum cdb_sc	mu_swap_blk(int level, block_id *pdest_blk_id, kill_set *kill_set_ptr, glist *exclude_glist_ptr,
			block_id upg_mv_block);
enum cdb_sc	mu_reduce_level(kill_set *kill_set_ptr);
enum cdb_sc	gvcst_expand_any_key(srch_blk_status *blk_stat, sm_uc_ptr_t rec_top, sm_uc_ptr_t expanded_key,
					int *rec_size, int *keylen, int *keycmpc, srch_hist *hist_ptr);
boolean_t in_exclude_list(unsigned char *curr_key_ptr, int key_len, glist *exclude_glist_ptr);
void mupip_reorg(void);

/* Inline functions */

static inline void deferred_exit_reorg_check(void)
{
	char			*rname;

	GBLREF	int			process_exiting;
	GBLREF	sgmnt_data_ptr_t	cs_data;
	GBLREF	VSIG_ATOMIC_T		forced_exit;
	GBLREF	volatile int4		gtmMallocDepth;
	GBLREF	volatile int		suspend_status;

	/* This is a variation on the deferred_exit handling_check in have_crit.h
	 * The forced_exit state of 2 indicates that the exit is already in progress, so we need not process any deferred events.
	 */
	assert(!INSIDE_THREADED_CODE(rname));				/* need to change if mupip reorg goes multi-threaded */
	if (forced_exit && (2 > forced_exit) && !process_exiting  && !cs_data->kill_in_prog)
	{	/* If forced_exit was set while in a deferred state, disregard any deferred timers and
		* invoke deferred_signal_handler directly.
		*/
		if (INTRPT_IN_KILL_CLEANUP == intrpt_ok_state)	/* no KIP & only set INTRPT_IN_KILL_CLEANUP if OK_TO_INTERRUPT */
			ENABLE_INTERRUPTS(INTRPT_IN_KILL_CLEANUP, INTRPT_OK_TO_INTERRUPT);
		if ((2 > forced_exit) && OK_TO_INTERRUPT)
		{
			deferred_signal_handler();
		} else
		{
			if ((DEFER_SUSPEND == suspend_status) && OK_TO_INTERRUPT)
				suspend(SIGSTOP);
			if (deferred_timers_check_needed && OK_TO_INTERRUPT)
				check_for_deferred_timers();
		}
	}
}

#define  MUPIP_REORG_DEFINED
#endif
