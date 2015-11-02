/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note that each TAB_DB_CSH_ACCT_REC entry corresponds to a field in the file-header
 * Therefore, any operation that changes the offset of any entry in the file-header shouldn't be attempted.
 * Additions are to be done at the END of the file.
 * Replacing existing fields with new fields is allowed (provided their implications are thoroughly analyzed).
 */

TAB_DB_CSH_ACCT_REC(n_gvcst_srches,     "  count_gvcst_srches    ", "  cumul_gvcst_srches    ") /* number of calls to gvcst_search */
TAB_DB_CSH_ACCT_REC(n_gvcst_srch_clues, "  count_gvcst_srch_clues", "  cumul_gvcst_srch_clues") /* number of times clue was non-zero in gvcst_search */
TAB_DB_CSH_ACCT_REC(n_clue_used_head,   "  count_clue_used_head  ", "  cumul_clue_used_head  ") /* number of times the search-key was between first_rec and clue */
TAB_DB_CSH_ACCT_REC(n_clue_used_same,   "  count_clue_used_same  ", "  cumul_clue_used_same  ") /* number of times the search-key was the clue itself */
TAB_DB_CSH_ACCT_REC(n_clue_used_tail,   "  count_clue_used_tail  ", "  cumul_clue_used_tail  ") /* number of times the search-key was between the clue and last_rec */
TAB_DB_CSH_ACCT_REC(n_t_qreads,         "  count_t_qreads        ", "  cumul_t_qreads        ") /* number of calls to t_qread */
TAB_DB_CSH_ACCT_REC(unused_dsk_reads,   "  obsolete_dsk_reads    ", "  obsolete_dsk_reads    ") /* number of calls to dsk_read through t_qread() */
TAB_DB_CSH_ACCT_REC(n_bgmm_updates,     "  count_bgmm_updates    ", "  cumul_bgmm_updates    ") /* number of calls to bg_update (for MM no. of calls to mm_update) */
TAB_DB_CSH_ACCT_REC(unused_dsk_writes,  "  obsolete_dsk_writes   ", "  obsolete_dsk_writes   ") /* number of calls to dsk_write through wcs_wtstart() */
TAB_DB_CSH_ACCT_REC(n_bg_update_creates,"  count_bg_update_create", "  cumul_bg_update_create") /* number of calls to db_csh_getn() from bg_update() (no meaning in MM) */
TAB_DB_CSH_ACCT_REC(n_db_csh_getns,     "  count_db_csh_getns    ", "  cumul_db_csh_getns    ") /* number of calls to db_csh_getn */
TAB_DB_CSH_ACCT_REC(n_db_csh_getn_lcnt, "  count_db_csh_getn_lcnt", "  cumul_db_csh_getn_lcnt") /* total number of cache-records that were skipped in db_csh_getn */
