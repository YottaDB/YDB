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

#ifndef MUPIP_REORG_DEFINED

/* prototypes */

boolean_t mu_reorg(mval *gn, glist *exclude_glist_ptr, boolean_t *resume,
			int index_fill_factor, int data_fill_factor, int reorg_op);
# ifdef UNIX
boolean_t mu_swap_root(mval *gn, int *root_swap_statistic_ptr);
block_id swap_root_or_directory_block(int parent_blk_lvl, int level, srch_hist *dir_hist_ptr, block_id child_blk_id,
		sm_uc_ptr_t child_blk_ptr, kill_set *kill_set_list, trans_num curr_tn);
# endif
enum cdb_sc mu_clsce(int level, int i_max_fill, int d_max_fill, kill_set *kill_set_ptr, boolean_t *remove_rtsib);
enum cdb_sc mu_split(int cur_level, int i_max_fill, int d_max_fill, int *blks_created, int *lvls_increased);
enum cdb_sc mu_swap_blk(int level, block_id *pdest_blk_id, kill_set *kill_set_ptr, glist *exclude_glist_ptr);
enum cdb_sc mu_reduce_level(kill_set *kill_set_ptr);
enum cdb_sc gvcst_expand_any_key (sm_uc_ptr_t blk_base, sm_uc_ptr_t rec_top, sm_uc_ptr_t expanded_key,
					int *rec_size, int *keylen, int *keycmpc, srch_hist *hist_ptr);
boolean_t in_exclude_list(unsigned char *curr_key_ptr, int key_len, glist *exclude_glist_ptr);
void mupip_reorg(void);

#define  MUPIP_REORG_DEFINED
#endif

