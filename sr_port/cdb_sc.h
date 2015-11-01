/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CDB_SC
#define CDB_SC

/*********************************  WARNING:  ***********************************
*   Several of these codes are concurrently defined in GVCST_BLK_SEARCH.MAR,	*
*   GVCST_SEARCH.MAR, MUTEX.MAR, and MUTEX_STOPREL.MAR.  If their positions	*
*   are changed here, their definitions must be modified there as well!		*
********************************************************************************/

enum cdb_sc
{
	cdb_sc_normal = 0,	/*  0   success */
	cdb_sc_endtree,		/*  1   gvcst_lftsib or gvcst_rtsib searched past end of tree */
	cdb_sc_delete_parent,	/*  2   gvcst_kill_blk succeeded, but signals gvcst_kill that block was completely deleted */
	cdb_sc_nolock,		/*  3   mutex_lockwim or mutex_lockwnoh was unable to obtain a lock, or grab_read_crit failed */
	cdb_sc_needcrit,	/*  4	on 4th attempt and need crit for this region -- restart transaction no penalty */
	cdb_sc_helpedout,	/*  5	wcs_blocked when t_tries >= CDB_STAGNATE */
	cdb_sc_gbloflow,	/*  6   t_end or tp_tend found the database full and could not be extended */
	cdb_sc_oprnotneeded,	/*  7   reorg operation was not required */
	cdb_sc_starrecord,	/*  8   star record was found while reading the block */
	cdb_sc_extend,		/*  9   extend requested when none seemed needed - from gdsfilext */
	cdb_sc_jnlclose,	/* 10	journal file has been closed */
	cdb_sc_rmisalign1 = 'A',/* 'A'  gvcst_get found record misaligned */
	cdb_sc_keyoflow,	/* 'B'  gvcst_expand_key or gvcst_search (3) found key overflow */
	cdb_sc_rmisalign,	/* 'C'  Record misaligned from nearly everyone */
	cdb_sc_r2small,		/* 'D'  gvcst_expand_key found record too small */
	cdb_sc_losthist,	/* 'E'  t_end or tp_tend (both mm or bg) - tn could not be verified from history */
	cdb_sc_mapfail,		/* 'F'  t_end or op_tcommit (from bm_getfree) failed to acquire new block */
	cdb_sc_lostcr,		/* 'G'  gvcst_...sib, t_end, tp_tend, tp_check_hist - found cache buffer modified */
	cdb_sc_mkblk,		/* 'H'  Composing a local block failed, from gvcst_kill (5) gvcst_put (10), mu_reorg (5) */
	cdb_sc_rdfail,		/* 'I'  t_qread found block number requested is outside size of file as described by fileheader */
	cdb_sc_badlvl,		/* 'J'  gvcst_search found a child block didn't have the next block level below its parent */
	cdb_sc_cacheprob,	/* 'K'  db_csh_get, ... found a cache control problem */
	cdb_sc_blkmod,		/* 'L'  t_end, or tp_tend found block modified */
	cdb_sc_uperr,		/* 'M'  t_ch received an unpredicatable error */
	cdb_sc_comfail,		/* 'N'  Commit failed used in t_end_sysops (8) by (?) */
	cdb_sc_lostbefor,	/* 'O'  t_end or tp_tend found the before image needed for journaling was removed from the cache */
	cdb_sc_committfail,	/* 'P'  t_commit_cleanup found a partially committed block split */
	cdb_sc_dbccerr,		/* 'Q'  mutex found (in 1 of 3 places) an interlock instruction failure in critical mechanism */
	cdb_sc_critreset,	/* 'R'  mutex found (in 1 of 6 places) that the segment crit crash count has been incremented */
	cdb_sc_maxlvl,		/* 'S'  t_write_root or gvcst_search found maximum legal block level for database exceeded */
	cdb_sc_blockflush,	/* 'T'  t_end (hist, or bitmap) found an to update a buffer that is being flushed (GT.CX) */
	cdb_sc_cyclefail,	/* 'U'  t_end or tp_tend found a buffer in read(only) set was overwritten though tn static */
	cdb_sc_readblocked,	/* 'V'  t_qread found db_csh_getn couldn't get a buffer for a read_only process */
	cdb_sc_future_read,	/* 'W'  dsk_read return to t_qread indicated block transaction exceeds curr_tn (GT.CX) */
	cdb_sc_badbitmap,	/* 'X'  bm_getfree found bitmap had bad size or level */
	cdb_sc_badoffset,	/* 'Y'  gvcst_blk_search (in gvcst_search_blk or gvcst_search_tail) found a bad record offset */
	cdb_sc_blklenerr,	/* 'Z'  gvcst_blk_search (in gvcst_search_blk or gvcst_search_tail) reached the end with no match */
	cdb_sc_bmlmod = 'a',	/* 'a'  t_end or tp_tend (mm or bg) found bit_map modified */
	cdb_sc_lostbmlhist,	/* 'b'  t_end or tp_tend (bg) - tn could not be verified from history */
	cdb_sc_lostbmlcr,	/* 'c'  t_end or tp_tend (bg) - found cache buffer modified */
	cdb_sc_lostoldblk,	/* 'd'  t_qread or op_tcommit (tp and before image) - old_block of a used block is NULL */
	cdb_sc_blknumerr,	/* 'e'  t_qread or op_tcommit - block number is impossible */
	cdb_sc_blksplit,	/* 'f'  recompute_upd_array recognized that the block needs to be split */
	cdb_sc_toomanyrecompute,/* 'g'	more than 25% of the blocks in read-set need to be recomputed */
	cdb_sc_jnlstatemod,	/* 'h'  journal state is modified during an operation */
	cdb_sc_unfreeze_getcrit	/* 'i'	gdsfilext found region frozen. Wait for unfreeze and reattempt crit */
};

#define FUTURE_READ 0		/* used by dsk_read and t_qread */

#endif
