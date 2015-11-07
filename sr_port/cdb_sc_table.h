/****************************************************************
 *                                                              *
 *      Copyright 2003, 2013 Fidelity Information Services, Inc        *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

/* New entries should be added at the end to maintain backward compatibility */
/* Note: This is an exception where we have 132+ characters in a line */

/*
 * CDB_SC_NUM_ENTRY(code, value)
 * CDB_SC_UCHAR_ENTRY(code, is_wcs_code, value)
 * CDB_SC_LCHAR_ENTRY(code, is_wcs_code, value)
 *
 * is_wcs_code is TRUE if the cdb_sc code is a cache-related failure code.
 *
 * cdb_sc codes with numeric values are internally generated codes which is never displayed to the user and hence
 * can never imply a database cache related problem. hence the macro CDB_SC_NUM_ENTRY has no is_wcs_code parameter.
 *
 * Currently the failure codes considered as cache failure codes are as follows.
 *      cdb_sc_losthist,
 *      cdb_sc_lostcr,
 *      cdb_sc_cacheprob,
 *      cdb_sc_lostbefor
 *      cdb_sc_cyclefail,
 *      cdb_sc_lostbmlhist,
 *      cdb_sc_lostbmlcr,
 *      cdb_sc_crbtmismatch,
 * cdb_sc_bmlmod and cdb_sc_blkmod need to be added here, but they present an interesting problem. This is because
 * if the database has an integrity error, we will get a cdb_sc_blkmod/bmlmod error for every transaction that reads
 * the block with the BLKTNTOOLG integrity error in which case we do not want to set wc_blocked and cause indefinite
 * cache-recoveries. But to do that we need to do a wcs_verify() after setting wc_blocked and if no problems are
 * detected, we should unset wc_blocked. that is a little tricky and is deferred until it is considered worthy.
 */

CDB_SC_NUM_ENTRY(  cdb_sc_normal,                    0) /*  0   success */
CDB_SC_NUM_ENTRY(  cdb_sc_endtree,                   1) /*  1   gvcst_lftsib or gvcst_rtsib searched past end of tree */
CDB_SC_NUM_ENTRY(  cdb_sc_delete_parent,             2) /*  2   gvcst_kill_blk succeeded, but signals gvcst_kill
							 *		that block was completely deleted */
CDB_SC_NUM_ENTRY(  cdb_sc_nolock,                    3) /*  3   mutex_lockwim was unable to obtain a lock */
CDB_SC_NUM_ENTRY(  cdb_sc_needcrit,                  4) /*  4   on 4th attempt and need crit for this region -- restart transaction
							 *		no penalty */
CDB_SC_NUM_ENTRY(  cdb_sc_helpedout,                 5) /*  5   wcs_blocked when t_tries >= CDB_STAGNATE */
CDB_SC_NUM_ENTRY(  cdb_sc_gbloflow,                  6) /*  6   t_end or tp_tend found database full and could not be extended */
CDB_SC_NUM_ENTRY(  cdb_sc_oprnotneeded,              7) /*  7   reorg operation was not required */
CDB_SC_NUM_ENTRY(  cdb_sc_starrecord,                8) /*  8   star record was found while reading the block */
CDB_SC_NUM_ENTRY(  cdb_sc_extend,                    9) /*  9   extend requested when none seemed needed - from gdsfilext */
CDB_SC_NUM_ENTRY(  cdb_sc_jnlclose,                 10) /* 10   journal file has been closed */

CDB_SC_UCHAR_ENTRY(cdb_sc_rmisalign1,       FALSE, 'A') /* 'A'  gvcst_get found record misaligned */
CDB_SC_UCHAR_ENTRY(cdb_sc_keyoflow,         FALSE, 'B') /* 'B'  gvcst_expand_key or gvcst_search (3) found key overflow */
CDB_SC_UCHAR_ENTRY(cdb_sc_rmisalign,        FALSE, 'C') /* 'C'  Record misaligned from nearly everyone */
CDB_SC_UCHAR_ENTRY(cdb_sc_r2small,          FALSE, 'D') /* 'D'  gvcst_expand_key found record too small */
CDB_SC_UCHAR_ENTRY(cdb_sc_losthist,         TRUE,  'E') /* 'E'  t_end/tp_tend (mm/bg) - tn could not be verified from history */
CDB_SC_UCHAR_ENTRY(cdb_sc_mapfail,          FALSE, 'F') /* 'F'  t_end or op_tcommit (from bm_getfree) failed to acquire new block */
CDB_SC_UCHAR_ENTRY(cdb_sc_lostcr,           TRUE,  'G') /* 'G'  gvcst_...sib, t_end/tp_tend/tp_hist - found cache buffer modified */
CDB_SC_UCHAR_ENTRY(cdb_sc_mkblk,            FALSE, 'H') /* 'H'  Composing a local block failed, from gvcst_kill(3) gvcst_put(14) */
CDB_SC_UCHAR_ENTRY(cdb_sc_rdfail,           FALSE, 'I') /* 'I'  t_qread found block number requested is outside size of file
							 *		as described by fileheader */
CDB_SC_UCHAR_ENTRY(cdb_sc_badlvl,           FALSE, 'J') /* 'J'  gvcst_search found a child block didn't have the next block level
							 *		below its parent */
CDB_SC_UCHAR_ENTRY(cdb_sc_cacheprob,        TRUE,  'K') /* 'K'  db_csh_get, ... found a cache control problem */
CDB_SC_UCHAR_ENTRY(cdb_sc_blkmod,           FALSE, 'L') /* 'L'  t_end, or tp_tend found block modified */
CDB_SC_UCHAR_ENTRY(cdb_sc_uperr,            FALSE, 'M') /* 'M'  t_ch received an unpredicatable error */
CDB_SC_UCHAR_ENTRY(cdb_sc_comfail,          FALSE, 'N') /* 'N'  Commit failed used in t_end_sysops (8) by (?) */
CDB_SC_UCHAR_ENTRY(cdb_sc_lostbefor,        TRUE,  'O') /* 'O'  t_end or tp_tend found the before image needed for journaling was
							 *		removed from the cache */
CDB_SC_UCHAR_ENTRY(cdb_sc_committfail,      FALSE, 'P') /* 'P'  t_commit_cleanup found a partially committed block split */
CDB_SC_UCHAR_ENTRY(cdb_sc_dbccerr,          FALSE, 'Q') /* 'Q'  mutex found (in 1 of 3 places) an interlock instruction failure
							 *		in critical mechanism */
CDB_SC_UCHAR_ENTRY(cdb_sc_critreset,        FALSE, 'R') /* 'R'  mutex found (in 1 of 6 places) that the segment crit crash count
							 *		has been incremented */
CDB_SC_UCHAR_ENTRY(cdb_sc_maxlvl,           FALSE, 'S') /* 'S'  t_write_root or gvcst_search found maximum legal block level for
							 *		database exceeded */
CDB_SC_UCHAR_ENTRY(cdb_sc_blockflush,       FALSE, 'T') /* 'T'  t_end (hist, or bitmap) found an to update a buffer that is being
							 *		flushed (GT.CX) */
CDB_SC_UCHAR_ENTRY(cdb_sc_cyclefail,        TRUE,  'U') /* 'U'  t_end or tp_tend found a buffer in read(only) set was overwritten
							 *		though tn static */
CDB_SC_UCHAR_ENTRY(cdb_sc_optrestart,       FALSE, 'V') /* 'V'  TP restart explicitly signaled by the TRESTART command */
CDB_SC_UCHAR_ENTRY(cdb_sc_future_read,      FALSE, 'W') /* 'W'  dsk_read return to t_qread indicated block transaction exceeds
							 *		curr_tn (GT.CX) */
CDB_SC_UCHAR_ENTRY(cdb_sc_badbitmap,        FALSE, 'X') /* 'X'  bm_getfree found bitmap had bad size or level */
CDB_SC_UCHAR_ENTRY(cdb_sc_badoffset,        FALSE, 'Y') /* 'Y'  gvcst_blk_search (in gvcst_search_blk or gvcst_search_tail) found
							 *		a bad record offset */
CDB_SC_UCHAR_ENTRY(cdb_sc_blklenerr,        FALSE, 'Z') /* 'Z'  gvcst_blk_search (in gvcst_search_blk or gvcst_search_tail) reached
							 *		the end with no match */

CDB_SC_LCHAR_ENTRY(cdb_sc_bmlmod,           FALSE, 'a') /* 'a'  t_end or tp_tend (mm or bg) found bit_map modified */
CDB_SC_LCHAR_ENTRY(cdb_sc_lostbmlhist,      TRUE,  'b') /* 'b'  t_end or tp_tend (bg) - tn could not be verified from history */
CDB_SC_LCHAR_ENTRY(cdb_sc_lostbmlcr,        TRUE,  'c') /* 'c'  t_end or tp_tend (bg) - found cache buffer modified */
CDB_SC_LCHAR_ENTRY(cdb_sc_lostoldblk,       FALSE, 'd') /* 'd'  t_qread or op_tcommit (tp and before image) - old_block of a used
							 *		block is NULL */
CDB_SC_LCHAR_ENTRY(cdb_sc_blknumerr,        FALSE, 'e') /* 'e'  t_qread or op_tcommit - block number is impossible */
CDB_SC_LCHAR_ENTRY(cdb_sc_blksplit,         FALSE, 'f') /* 'f'  recompute_upd_array recognized that the block needs to be split */
CDB_SC_LCHAR_ENTRY(cdb_sc_toomanyrecompute, FALSE, 'g') /* 'g'  more than 25% of the blocks in read-set need to be recomputed */
CDB_SC_LCHAR_ENTRY(cdb_sc_jnlstatemod,      FALSE, 'h') /* 'h'  csd->jnl_state changed or csd->jnl_before_image changed since start
							 *		of the transaction */
CDB_SC_LCHAR_ENTRY(cdb_sc_needlock,         FALSE, 'i') /* 'i'  on final retry and need to wait for M-lock - restart transaction
							 *		- allow for max of 16 such restarts */
CDB_SC_LCHAR_ENTRY(cdb_sc_bkupss_statemod,  FALSE, 'j') /* 'j'  t_end/tp_tend found that either online-backup-in-progress or
							 *		snapshot state changed since start of transaction */
CDB_SC_LCHAR_ENTRY(cdb_sc_crbtmismatch,     TRUE,  'k') /* 'k'  cr->blk and bt->blk does not match */
CDB_SC_LCHAR_ENTRY(cdb_sc_phase2waitfail,   TRUE,  'l') /* 'l'  wcs_phase2_commit_wait timed out when called from t_qread */
CDB_SC_LCHAR_ENTRY(cdb_sc_inhibitkills,     FALSE, 'm') /* 'm'  t_end/tp_tend found inhibit_kills counter greater than zero */
CDB_SC_LCHAR_ENTRY(cdb_sc_triggermod,       FALSE, 'n') /* 'n'  csd->db_trigger_cycle changed since start of of transaction */
CDB_SC_LCHAR_ENTRY(cdb_sc_onln_rlbk1,       FALSE, 'o') /* 'o'  csa->onln_rlbk_cycle changed since start of transaction */
CDB_SC_LCHAR_ENTRY(cdb_sc_onln_rlbk2,       FALSE, 'p') /* 'p'  csa->db_onln_rlbkd_cycle changed since start of transaction */
CDB_SC_LCHAR_ENTRY(cdb_sc_truncate,         FALSE, 'q') /* 'q'  t_qread tried to read a block beyond the end of a database
							 *		that has been concurrently truncated */
CDB_SC_LCHAR_ENTRY(cdb_sc_gvtrootmod,       FALSE, 'r') /* 'r'  gvcst_kill found a need to redo the gvcst_root_search */
CDB_SC_LCHAR_ENTRY(cdb_sc_instancefreeze,   FALSE, 's') /* 's'  instance freeze detected in t_end/tp_tend, requires retry */
CDB_SC_LCHAR_ENTRY(cdb_sc_gvtrootmod2,      FALSE, 't') /* 't'  t_end/tp_tend detected root blocks moved by reorg */
CDB_SC_LCHAR_ENTRY(cdb_sc_spansize,         FALSE, 'u') /* 'u'  chunks of spanning node don't add up */
CDB_SC_LCHAR_ENTRY(cdb_sc_restarted,        FALSE, 'v') /* 'v'  return value indicating t_retry has already happened */
CDB_SC_LCHAR_ENTRY(cdb_sc_tqreadnowait,     FALSE, 'w') /* 'w'  update helper returning from t_qread instead of sleeping */
