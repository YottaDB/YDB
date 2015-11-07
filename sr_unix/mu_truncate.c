/****************************************************************
 *								*
 *	Copyright 2012, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/***************************************************************************************************************
 * mu_truncate.c:
 * 	This program truncates the db file corresponding to a given region, if there is enough free space.
 * 	It operates in two phases:
 * 	Phase 1:
 * 		Working from the end of the file towards its beginning, mu_truncate sets recycled blocks
 * 		free, which allows t_end to write PBLKS if needed. If it sees a busy block, skip ahead
 * 		to Phase 2.
 * 	Phase 2:
 * 		Grab crit, write JRT_TRUNC and INCTN journal records, reduce csa->ti->total_blks and
 * 		finally truncate the file. If mu_truncate crashes here, recover_truncate.c finishes the job.
 * 	Meanwhile:
 * 		Before completing Phase 2, mu_truncate has to detect other processes marking blocks
 * 		busy through gvcst_map_build. Likewise, other processes have to detect concurrent
 * 		truncates that occur between allocating blocks outside crit and transaction completion.
 * 	mu_truncate returns TRUE if and only if it completes successfully or halts for a benign reason.
 *
 **************************************************************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_string.h"
#include "gtm_time.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "min_max.h"
#include "t_qread.h"
#include "dse.h"
#include "gtmmsg.h"
#include "t_begin.h"
#include "t_write_map.h"
#include "t_abort.h"
#include "t_retry.h"
#include "t_end.h"
#include "wbox_test_init.h"
#include "error.h"
#include "t_recycled2free.h"
#include "cdb_sc.h"
#include "eintr_wrappers.h"
#include "gtmimagename.h"
#include "mu_truncate.h"
#include "gtmio.h"
#include "util.h"
#include "anticipatory_freeze.h"

#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "interlock.h"
#include "gdsbgtr.h"
#include "copy.h"
#include "shmpool.h"
#include "clear_cache_array.h"
#include "wcs_flu.h"
#include "repl_msg.h"
#include "gtmsource.h"

error_def(ERR_BUFFLUFAILED);
error_def(ERR_DBFILERR);
error_def(ERR_DBFSYNCERR);
error_def(ERR_IOERROR);
error_def(ERR_JNLFLUSH);
error_def(ERR_MUTRUNCFAIL);
error_def(ERR_MUTRUNCNOSPACE);
error_def(ERR_MUTRUNCERROR);
error_def(ERR_MUTRUNCNOV4);
error_def(ERR_MUTRUNCNOTBG);
error_def(ERR_MUTRUNCSSINPROG);
error_def(ERR_MUTRUNCSUCCESS);
error_def(ERR_MUTRUNCBACKINPROG);
error_def(ERR_TEXT);
error_def(ERR_TRUNCBACKUPPROG);
error_def(ERR_TRUNCNOTRUN);
error_def(ERR_TRUNCSSINPROG);

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	uint4			update_trans;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF  uint4                   update_array_size;
GBLREF	unsigned char		rdfail_detail;
GBLREF	uint4			process_id;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	volatile int4		db_fsync_in_prog;	/* for DB_FSYNC macro usage */
GBLREF	jnl_gbls_t		jgbl;
GBLREF	int			num_additional_processors;
GBLREF	jnlpool_addrs		jnlpool;

boolean_t mu_truncate(int4 truncate_percent)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t 	csd;
	int			num_local_maps;
	int 			lmap_num, lmap_blk_num;
	int			bml_status, sigkill;
	int			save_errno;
	int			ftrunc_status;
	uint4			jnl_status;
	uint4			old_total, new_total;
	uint4			old_free, new_free;
	uint4			end_blocks;
	int4			blks_in_lmap, blk;
	gtm_uint64_t		before_trunc_file_size;
	off_t			trunc_file_size;
	uchar_ptr_t		lmap_addr;
	boolean_t		was_crit;
	uint4			found_busy_blk;
	srch_blk_status		bmphist;
	srch_blk_status 	*blkhist;
	srch_hist		alt_hist;
	trans_num		curr_tn;
	blk_hdr_ptr_t		lmap_blk_hdr;
	block_id		*blkid_ptr;
	unix_db_info    	*udi;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	char			*err_msg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	csd = cs_data;
	if (dba_mm == csd->acc_meth)
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_MUTRUNCNOTBG, 2, REG_LEN_STR(gv_cur_region));
		return TRUE;
	}
	if ((GDSVCURR != csd->desired_db_format) || (csd->blks_to_upgrd != 0))
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_MUTRUNCNOV4, 2, REG_LEN_STR(gv_cur_region));
		return TRUE;
	}
	if (csa->ti->free_blocks < (truncate_percent * csa->ti->total_blks / 100))
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_MUTRUNCNOSPACE, 3, REG_LEN_STR(gv_cur_region), truncate_percent);
		return TRUE;
	}
	/* already checked for parallel truncates on this region --- see mupip_reorg.c */
	gv_target = NULL;
	assert(csa->nl->trunc_pid == process_id);
	assert(dba_mm != csd->acc_meth);
	old_total = csa->ti->total_blks;
	old_free = csa->ti->free_blocks;
	sigkill = 0;
	found_busy_blk = 0;
	memset(&alt_hist, 0, SIZEOF(alt_hist)); /* null-initialize history */
	assert(csd->bplmap == BLKS_PER_LMAP);
	end_blocks = old_total % BLKS_PER_LMAP; /* blocks in the last lmap (first one we start scanning) */
	if (0 == end_blocks)
		end_blocks = BLKS_PER_LMAP;
	num_local_maps = DIVIDE_ROUND_UP(old_total, BLKS_PER_LMAP);
	/* ======================================== PHASE 1 ======================================== */
	for (lmap_num = num_local_maps - 1; (lmap_num > 0 && !found_busy_blk); lmap_num--)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			return TRUE;
		assert(csa->ti->total_blks >= old_total); /* otherwise, a concurrent truncate happened... */
		if (csa->ti->total_blks != old_total) /* Extend (likely called by mupip extend) -- don't truncate */
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_MUTRUNCNOSPACE, 3, REG_LEN_STR(gv_cur_region),
					truncate_percent);
			return TRUE;
		}
		lmap_blk_num = lmap_num * BLKS_PER_LMAP;
		if (csa->nl->highest_lbm_with_busy_blk >= lmap_blk_num)
		{
			found_busy_blk = lmap_blk_num;
			break;
		}
		blks_in_lmap = (lmap_num == num_local_maps - 1) ? end_blocks : BLKS_PER_LMAP;
		/* Loop through non-bitmap blocks of this lmap, do recycled2free */
		DBGEHND((stdout, "DBG:: lmap_num = [%lu], lmap_blk_num = [%lu], blks_in_lmap = [%lu]\n",
			lmap_num, lmap_blk_num, blks_in_lmap));
		for (blk = 1; blk < blks_in_lmap && blk != -1 && !found_busy_blk;)
		{
			t_begin(ERR_MUTRUNCFAIL, UPDTRNS_DB_UPDATED_MASK);
			for (;;) /* retry loop for recycled to free transactions */
			{
				curr_tn = csd->trans_hist.curr_tn;
				/* Read the nth local bitmap into memory */
				bmphist.blk_num = lmap_blk_num;
				bmphist.buffaddr = t_qread(bmphist.blk_num, &bmphist.cycle, &bmphist.cr);
				lmap_blk_hdr = (blk_hdr_ptr_t)bmphist.buffaddr;
				if (!(bmphist.buffaddr) || (BM_SIZE(BLKS_PER_LMAP) != lmap_blk_hdr->bsiz))
				{ /* Could not read the block successfully. Retry. */
					t_retry((enum cdb_sc)rdfail_detail);
					continue;
				}
				lmap_addr = bmphist.buffaddr + SIZEOF(blk_hdr);
				/* starting from the hint (blk itself), find the first busy or recycled block */
				blk = bml_find_busy_recycled(blk, lmap_addr, blks_in_lmap, &bml_status);
				assert(blk < BLKS_PER_LMAP);
				if (blk == -1 || blk >= blks_in_lmap)
				{ /* done with this lmap, continue to next */
					t_abort(gv_cur_region, csa);
					break;
				}
				else if (BLK_BUSY == bml_status || csa->nl->highest_lbm_with_busy_blk >= lmap_blk_num)
				{ /* stop processing blocks... skip ahead to phase 2 */
					found_busy_blk = lmap_blk_num;
					t_abort(gv_cur_region, csa);
					break;
				}
				else if (BLK_RECYCLED == bml_status)
				{ /* Write PBLK records for recycled blocks only if before_image journaling is
				   * enabled. t_end() takes care of checking if journaling is enabled and
				   * writing PBLK record. We have to at least mark the recycled block as free.
				   */
					RESET_UPDATE_ARRAY;
					update_trans = UPDTRNS_DB_UPDATED_MASK;
					*((block_id *)update_array_ptr) = blk;
					update_array_ptr += SIZEOF(block_id);
					*(int *)update_array_ptr = 0;
					alt_hist.h[1].blk_num = 0;
					alt_hist.h[0].level = 0;
					alt_hist.h[0].cse = NULL;
					alt_hist.h[0].tn = curr_tn;
					alt_hist.h[0].blk_num = lmap_blk_num + blk;
					alt_hist.h[0].buffaddr = t_qread(alt_hist.h[0].blk_num,
							&alt_hist.h[0].cycle, &alt_hist.h[0].cr);
					if (!alt_hist.h[0].buffaddr)
					{
						t_retry((enum cdb_sc)rdfail_detail);
						continue;
					}
					if (!t_recycled2free(&alt_hist.h[0]))
					{
						t_retry(cdb_sc_lostbmlcr);
						continue;
					}
					t_write_map(&bmphist, (unsigned char *)update_array, curr_tn, 0);
					/* Set the opcode for INCTN record written by t_end() */
					inctn_opcode = inctn_blkmarkfree;
					if ((trans_num)0 == t_end(&alt_hist, NULL, TN_NOT_SPECIFIED))
						continue;
					/* block processed, scan from the next one */
					blk++;
					break;
				} else
				{
					assert(t_tries < CDB_STAGNATE);
					t_retry(cdb_sc_badbitmap);
					continue;
				}
			} /* END recycled2free retry loop */
		} /* END scanning blocks of this particular lmap */
		/* Write PBLK for the bitmap block, in case it hasn't been written i.e. t_end() was never called above */
		/* Do a transaction that just increments the bitmap block's tn so that t_end() can do its thing */
		DBGEHND((stdout, "DBG:: bitmap block inctn -- lmap_blk_num = [%lu]\n", lmap_blk_num));
		t_begin(ERR_MUTRUNCFAIL, UPDTRNS_DB_UPDATED_MASK);
		for (;;)
		{
			RESET_UPDATE_ARRAY;
			BLK_ADDR(blkid_ptr, SIZEOF(block_id), block_id);
			*blkid_ptr = 0;
			update_trans = UPDTRNS_DB_UPDATED_MASK;
			inctn_opcode = inctn_mu_reorg; /* inctn_mu_truncate */
			curr_tn = csd->trans_hist.curr_tn;
			blkhist = &alt_hist.h[0];
			blkhist->blk_num = lmap_blk_num;
			blkhist->tn = curr_tn;
			blkhist->cse = NULL; /* start afresh (do not use value from previous retry) */
			/* Read the nth local bitmap into memory */
			blkhist->buffaddr = t_qread(lmap_blk_num, (sm_int_ptr_t)&blkhist->cycle, &blkhist->cr);
			lmap_blk_hdr = (blk_hdr_ptr_t)blkhist->buffaddr;
			if (!(blkhist->buffaddr) || (BM_SIZE(BLKS_PER_LMAP) != lmap_blk_hdr->bsiz))
			{ /* Could not read the block successfully. Retry. */
				t_retry((enum cdb_sc)rdfail_detail);
				continue;
			}
			t_write_map(blkhist, (unsigned char *)blkid_ptr, curr_tn, 0);
			blkhist->blk_num = 0; /* create empty history for bitmap block */
			if ((trans_num)0 == t_end(&alt_hist, NULL, TN_NOT_SPECIFIED))
				continue;
			break;
		}
	} /* END scanning lmaps */
	/* ======================================== PHASE 2 ======================================== */
	assert(!csa->now_crit);
	for (;;)
	{ /* wait for FREEZE, we don't want to truncate a frozen database */
		grab_crit(gv_cur_region);
		if (!cs_data->freeze && !IS_REPL_INST_FROZEN)
			break;
		rel_crit(gv_cur_region);
		while (cs_data->freeze || IS_REPL_INST_FROZEN)
			hiber_start(1000);
	}
	assert(csa->nl->trunc_pid == process_id);
	/* Flush pending updates to disk. If this is not done, old updates can be flushed AFTER ftruncate, extending the file. */
	if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_MSYNC_DB))
	{
		assert(FALSE);
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("MUPIP REORG TRUNCATE"),
				DB_LEN_STR(gv_cur_region));
		rel_crit(gv_cur_region);
		return FALSE;
	}
	csa->nl->highest_lbm_with_busy_blk = MAX(found_busy_blk, csa->nl->highest_lbm_with_busy_blk);
	assert(IS_BITMAP_BLK(csa->nl->highest_lbm_with_busy_blk));
	new_total = MIN(old_total, csa->nl->highest_lbm_with_busy_blk + BLKS_PER_LMAP);
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		rel_crit(gv_cur_region);
		return TRUE;
	} else if (csa->ti->total_blks != old_total || new_total == old_total)
	{
		assert(csa->ti->total_blks >= old_total); /* Better have been an extend, not a truncate... */
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_MUTRUNCNOSPACE, 3, REG_LEN_STR(gv_cur_region), truncate_percent);
		rel_crit(gv_cur_region);
		return TRUE;
	} else if (GDSVCURR != csd->desired_db_format || csd->blks_to_upgrd != 0 || !csd->fully_upgraded)
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_MUTRUNCNOV4, 2, REG_LEN_STR(gv_cur_region));
		rel_crit(gv_cur_region);
		return TRUE;
	} else if (SNAPSHOTS_IN_PROG(csa->nl))
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_MUTRUNCSSINPROG, 2, REG_LEN_STR(gv_cur_region));
		rel_crit(gv_cur_region);
		return TRUE;
	} else if (BACKUP_NOT_IN_PROGRESS != cs_addrs->nl->nbb)
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_MUTRUNCBACKINPROG, 2, REG_LEN_STR(gv_cur_region));
		rel_crit(gv_cur_region);
		return TRUE;
	}
	DEFER_INTERRUPTS(INTRPT_IN_TRUNC);
	if (JNL_ENABLED(csa))
	{ /* Write JRT_TRUNC and INCTN records */
		if (!jgbl.dont_reset_gbl_jrec_time)
		SET_GBL_JREC_TIME;	/* needed before jnl_ensure_open as that can write jnl records */
		jpc = csa->jnl;
		jbp = jpc->jnl_buff;
		/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order
		 * of jnl records. This needs to be done BEFORE the jnl_ensure_open as that could write
		 * journal records (if it decides to switch to a new journal file).
		 */
		ADJUST_GBL_JREC_TIME(jgbl, jbp);
		jnl_status = jnl_ensure_open();
		if (SS_NORMAL != jnl_status)
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
		else
		{
			if (0 == jpc->pini_addr)
				jnl_put_jrt_pini(csa);
			jnl_write_trunc_rec(csa, old_total, csa->ti->free_blocks, new_total);
			inctn_opcode = inctn_mu_reorg;
			jnl_write_inctn_rec(csa);
			jnl_status = jnl_flush(gv_cur_region);
			if (SS_NORMAL != jnl_status)
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
					ERR_TEXT, 2, RTS_ERROR_TEXT("Error with journal flush during mu_truncate"),
					jnl_status);
				assert(NOJNL == jpc->channel); /* jnl file lost has been triggered */
			}
		}
	}
	/* Good to go ahead and REALLY truncate (reduce total_blks, clear cache_array, FTRUNCATE) */
	curr_tn = csa->ti->curr_tn;
	CHECK_TN(csa, csd, curr_tn);
	udi = FILE_INFO(gv_cur_region);
	/* Information used by recover_truncate to check if the file size and csa->ti->total_blks are INCONSISTENT */
	before_trunc_file_size = gds_file_size(gv_cur_region->dyn.addr->file_cntl); /* in DISK_BLOCKs */
	assert((off_t)before_trunc_file_size * DISK_BLOCK_SIZE > (off_t)(old_total - new_total) * csd->blk_size);
	trunc_file_size = (off_t)before_trunc_file_size * DISK_BLOCK_SIZE
		- (off_t)(old_total - new_total) * csd->blk_size; /* in bytes */
	csd->after_trunc_total_blks = new_total;
	csd->before_trunc_free_blocks = csa->ti->free_blocks;
	csd->before_trunc_total_blks = old_total; /* Flags interrupted truncate for recover_truncate */
	/* file size and total blocks: INCONSISTENT */
	csa->ti->total_blks = new_total;
	/* past the point of no return -- shared memory intact */
	assert(csa->ti->free_blocks >= DELTA_FREE_BLOCKS(old_total, new_total));
	csa->ti->free_blocks -= DELTA_FREE_BLOCKS(old_total, new_total);
	new_free = csa->ti->free_blocks;
	KILL_TRUNC_TEST(WBTEST_CRASH_TRUNCATE_1); /* 55 : Issue a kill -9 before 1st fsync */
	fileheader_sync(gv_cur_region);
	DB_FSYNC(gv_cur_region, udi, csa, db_fsync_in_prog, save_errno);
	CHECK_DBSYNC(gv_cur_region, save_errno);
	/* past the point of no return -- shared memory deleted */
	KILL_TRUNC_TEST(WBTEST_CRASH_TRUNCATE_2); /* 56 : Issue a kill -9 after 1st fsync */
	clear_cache_array(csa, csd, gv_cur_region, new_total, old_total);
	WRITE_EOF_BLOCK(gv_cur_region, csd, new_total, save_errno);
	if (0 != save_errno)
	{
		err_msg = (char *)STRERROR(errno);
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_MUTRUNCERROR, 4, REG_LEN_STR(gv_cur_region), LEN_AND_STR(err_msg));
		return FALSE;
	}
	KILL_TRUNC_TEST(WBTEST_CRASH_TRUNCATE_3); /* 57 : Issue a kill -9 after reducing csa->ti->total_blks, before FTRUNCATE */
	/* Execute an ftruncate() and truncate the DB file
	 * ftruncate() is a SYSTEM CALL on almost all platforms (except SunOS)
	 * It ignores kill -9 signal till its operation is completed.
	 * So we can safely assume that the result of ftruncate() will be complete.
	 */
	FTRUNCATE(FILE_INFO(gv_cur_region)->fd, trunc_file_size, ftrunc_status);
	if (0 != ftrunc_status)
	{
		err_msg = (char *)STRERROR(errno);
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_MUTRUNCERROR, 4, REG_LEN_STR(gv_cur_region), LEN_AND_STR(err_msg));
		/* should go through recover_truncate now, which will again try to FTRUNCATE */
		return FALSE;
	}
	/* file size and total blocks: CONSISTENT (shrunk) */
	KILL_TRUNC_TEST(WBTEST_CRASH_TRUNCATE_4); /* 58 : Issue a kill -9 after FTRUNCATE, before 2nd fsync */
	csa->nl->root_search_cycle++;	/* Force concurrent processes to restart in t_end/tp_tend to make sure no one
					 * tries to commit updates past the end of the file. Bitmap validations together
					 * with highest_lbm_with_busy_blk should actually be sufficient, so this is
					 * just to be safe.
					 */
	csd->before_trunc_total_blks = 0; /* indicate CONSISTENT */
	/* Increment TN */
	assert(csa->ti->early_tn == csa->ti->curr_tn);
	csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
	INCREMENT_CURR_TN(csd);
	fileheader_sync(gv_cur_region);
	DB_FSYNC(gv_cur_region, udi, csa, db_fsync_in_prog, save_errno);
	KILL_TRUNC_TEST(WBTEST_CRASH_TRUNCATE_5); /* 58 : Issue a kill -9 after after 2nd fsync */
	CHECK_DBSYNC(gv_cur_region, save_errno);
	ENABLE_INTERRUPTS(INTRPT_IN_TRUNC);
	curr_tn = csa->ti->curr_tn;
	rel_crit(gv_cur_region);
	send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_MUTRUNCSUCCESS, 5, DB_LEN_STR(gv_cur_region), old_total, new_total, &curr_tn);
	util_out_print("Truncated region: !AD. Reduced total blocks from [!UL] to [!UL]. Reduced free blocks from [!UL] to [!UL].",
					FLUSH, REG_LEN_STR(gv_cur_region), old_total, new_total, old_free, new_free);
	return TRUE;
} /* END of mu_truncate() */

STATICFNDEF int4 bml_find_busy_recycled(int4 hint, uchar_ptr_t base_addr, int4 blks_in_lmap, int *bml_status_ptr)
{
	uchar_ptr_t	ptr, top;
	int		status;
	int4		base_blk, blknum, i;

	top = base_addr + DIVIDE_ROUND_UP(blks_in_lmap, BML_BLKS_PER_UCHAR);
	for (ptr = base_addr + DIVIDE_ROUND_DOWN(hint, BML_BLKS_PER_UCHAR); ptr < top; ptr++)
	{
		if (FOUR_BLKS_FREE == *ptr)
			continue;
		base_blk = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK));
		/* loop through 4 blocks corresponding to this byte */
		for (i = 0; i < BML_BLKS_PER_UCHAR; i++)
		{
			blknum = i + base_blk;
			if (blknum < hint || blks_in_lmap <= blknum)
				continue;
			GET_STATUS(*ptr, i, status);
			if (status != BLK_FREE)
			{
				assert((t_tries < CDB_STAGNATE) || (status == BLK_BUSY) || (status == BLK_RECYCLED));
				*bml_status_ptr = status;
				return blknum;
			}
		}
	}
	return -1;
}

