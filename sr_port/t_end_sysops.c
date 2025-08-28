/****************************************************************
 *								*
 * Copyright (c) 2007-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"		/*  for strlen() in RTS_ERROR_TEXT macro */

#include <sys/mman.h>
#include <errno.h>

#include "gtm_facility.h"
#include "gdsroot.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbml.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "db_header_conversion.h"
#include "gdskill.h"
#include "gdscc.h"
#include "gdsblkops.h"
#include "jnl_get_checksum.h"
#include "cdb_sc.h"
#include "copy.h"
#include "filestruct.h"
#include "interlock.h"
#include "jnl.h"
#include "iosp.h"
#include "buddy_list.h"
#include "tp.h"
#include "gdsbgtr.h"
#include "min_max.h"
#include "relqueopi.h"
#include "gtmsecshr.h"
#include "sleep_cnt.h"
#include "wbox_test_init.h"
#include "cache.h"
#include "memcoherency.h"
#include "repl_sp.h"		/* for F_CLOSE (used by JNL_FD_CLOSE) */
#include "have_crit.h"
#include "gt_timer.h"
#include "anticipatory_freeze.h"
#include "aswp.h"
#include "gtmio.h"
#include "io.h"			/* for gtmsecshr.h */
#include "performcaslatchcheck.h"
#include "gtmmsg.h"
#include "error.h"		/* for gtm_fork_n_core() prototype */
#include "util.h"
#include "caller_id.h"
#include "add_inter.h"
#include "wcs_write_in_progress_wait.h"
#include "gvt_inline.h"
#include "mu_updwn_ver_inline.h"

/* Include prototypes */
#include "send_msg.h"
#include "bit_set.h"
#include "bit_clear.h"
#include "mupipbckup.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk prototypes */
#include "gvcst_blk_build.h"
#include "gvcst_map_build.h"
#include "relqop.h"
#include "is_proc_alive.h"
#include "bm_update.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_get_space.h"
#include "wcs_timer_start.h"
#include "gtmimagename.h"
#include "gtcm_jnl_switched.h"
#include "cert_blk.h"
#include "wcs_read_in_progress_wait.h"
#include "wcs_phase2_commit_wait.h"
#include "wcs_recover.h"
#include "shmpool.h"		/* Needed for the shmpool structures */
#include "db_snapshot.h"
#include "wcs_wt.h"
#include "stringpool.h"

error_def(ERR_DBFILERR);
error_def(ERR_FREEBLKSLOW);
error_def(ERR_GBLOFLOW);
error_def(ERR_TEXT);
error_def(ERR_WCBLOCKED);

#define MAX_CYCLES	2

#define DO_FAST_INTEG_CHECK(old_block, cs_addrs, cs, lcl_ss_ctx, blkid, write_to_snapshot_file)					\
{																\
	/* For fast integ, do NOT write the data block in global variable tree to snapshot file */ 				\
	blk_hdr			*blk_hdr_ptr;											\
	boolean_t		is_in_gv_tree;											\
																\
	/* level-0 block in DIR tree was already processed */									\
	assert((CSE_LEVEL_DRT_LVL0_FREE != cs->level) || (gds_t_writemap == cs->mode));						\
	assert(blkid < lcl_ss_ctx->total_blks);											\
	blk_hdr_ptr = (blk_hdr_ptr_t)old_block;											\
	if (WAS_FREE(cs->blk_prior_state))											\
		write_to_snapshot_file = FALSE;											\
	else if (WAS_RECYCLED(cs->blk_prior_state))										\
		write_to_snapshot_file = (blk_hdr_ptr->levl > 0);								\
	else															\
	{															\
		is_in_gv_tree = (IN_GV_TREE == (cs->blk_prior_state & KEEP_TREE_STATUS));					\
		write_to_snapshot_file = !((0 == blk_hdr_ptr->levl) && is_in_gv_tree);						\
	}															\
	/* If we decide not to write this block to snapshot file, then mark it as written in the snapshot file anyways because:	\
	 * (a) For free and recycled blocks, we prevent writing the before-image later when this block becomes busy		\
	 * (b) For busy blocks, level-0 block in GV tree may change level after swapping with another block. So, by marking it	\
	 * in the shadow bitmap, we prevent writing the before-image later.							\
	 */															\
	if (!write_to_snapshot_file && !ss_chk_shdw_bitmap(cs_addrs, lcl_ss_ctx, blkid))					\
		ss_set_shdw_bitmap(cs_addrs, lcl_ss_ctx, blkid);								\
}

GBLREF	boolean_t		block_saved, certify_all_blocks, dse_running, is_src_server, is_updproc, unhandled_stale_timer_pop,
				write_after_image;
GBLREF	boolean_t		mu_reorg_nosafejnl;		/* TRUE if NOSAFEJNL explicitly specified */
GBLREF	cache_rec_ptr_t		cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	int			gv_fillfactor, rc_set_fragment;	/* Contains offset within data at which data fragment starts */
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	boolean_t		exit_handler_active;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			mu_reorg_encrypt_in_prog;	/* non-zero if MUPIP REORG is in progress */
GBLREF	uint4			dollar_tlevel, process_id, update_array_size;
GBLREF	uint4			mu_upgrade_in_prog;		/* non-zero if MUPIP REORG UPGRADE/DOWNGRADE is in progress */
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		cr_array_index, t_tries;
GBLREF	volatile boolean_t	in_mutex_deadlock_check;
GBLREF	volatile int4		crit_count, fast_lock_count, gtmMallocDepth;

void fileheader_sync(gd_region *reg)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	th_index_ptr_t		cti;
	block_id		high_blk;
	size_t			flush_len, sync_size, rounded_flush_len;
	int4			save_errno;
	unix_db_info		*udi;

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert(csa->now_crit);	/* only way high water mark code works is if in crit */
				/* Adding lock code to it would remove this restriction */
	assert(csa->orig_read_write);
	assert((0 == memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1))
		|| (0 == memcmp(csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1)));
	cnl = csa->nl;
	gvstats_rec_cnl2csd(csa);	/* Periodically transfer statistics from database shared-memory to file-header */
	high_blk = cnl->highest_lbm_blk_changed;
	flush_len = SGMNT_HDR_LEN;
	if (0 <= high_blk)					/* If not negative, flush at least one master map block */
		flush_len += ((high_blk / csd->bplmap / DISK_BLOCK_SIZE / BITS_PER_UCHAR) + 1) * DISK_BLOCK_SIZE;
	if (csd->write_fullblk)
	{	/* round flush_len up to full block length. This is safe since we know that
		 * fullblockwrite_len is a factor of the starting data block - see gvcst_init_sysops.c
		 */
		flush_len = ROUND_UP(flush_len, csa->fullblockwrite_len);
	}
	if (udi->fd_opened_with_o_direct)
		flush_len = ROUND_UP2(flush_len, DIO_ALIGNSIZE(udi));
	assert(flush_len <= BLK_ZERO_OFF(csd->start_vbn));	/* assert that we never overwrite GDS block 0's offset */
	assert(flush_len <= SIZEOF_FILE_HDR(csd));	/* assert that we never go past the mastermap end */
	if (0 == memcmp(csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
		db_header_dwnconv(csd);
	DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, 0, (sm_uc_ptr_t)csd, flush_len, save_errno);
	if (0 != save_errno)
	{
		RTS_ERROR_CSA_ABT(csa, VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("Error during FileHeader Flush"), save_errno);
	}
	/* Reset shared memory value to initial value now that we have successfully flushed all of the needed master map.
	 * Cannot reset the shared memory value BEFORE the DB_LSEEKWRITE macro as that can abruptly terminate the process
	 * if the instance is frozen and we had gotten a SIGTERM/SIGINT. We would then have not flushed the master map
	 * and have lost all indication of how much to flush resulting in a DBMBPINCFL/DBMBMINCFRE integ error.
	 */
	if (0 == memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1))
		cnl->highest_lbm_blk_changed = GDS_CREATE_BLK_MAX;	/* Reset to initial value */
	else DEBUG_ONLY(if (0 == memcmp(csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1)))
		cnl->highest_lbm_blk_changed = V6_GDS_CREATE_BLK_MAX;
#ifdef DEBUG
	else
		assert((0 == memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1))
			|| (0 == memcmp(csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1)));
#endif
	return;
}

/* update a bitmap */
void bm_update(cw_set_element *cs, sm_uc_ptr_t lclmap, boolean_t is_mm)
{
	int4				bml_full, bplmap;
	boolean_t			change_bmm;
	block_id			blkid, total_blks;
	sgmnt_addrs			*csa;
	sgmnt_data_ptr_t		csd;
	node_local_ptr_t		cnl;
	th_index_ptr_t			cti;
	int4				reference_cnt;

	csa = cs_addrs;		/* Local access copies */
	csd = csa->hdr;
	cti = csa->ti;
	assert(csa->now_crit);
	bplmap = csd->bplmap;
	blkid = cs->blk;
	total_blks = cti->total_blks;
	if (((total_blks / bplmap) * bplmap) == blkid)
		total_blks -= blkid;
	else
		total_blks = bplmap;
	reference_cnt = cs->reference_cnt;
	assert(0 <= (block_id)(cti->free_blocks - reference_cnt));
	cti->free_blocks -= reference_cnt;
	change_bmm = FALSE;
	/* assert that cs->reference_cnt is 0 if we are in MUPIP REORG UPGRADE/DOWNGRADE */
	assert(!mu_upgrade_in_prog || !mu_reorg_encrypt_in_prog || (0 == reference_cnt));
	/* assert that if cs->reference_cnt is 0, then we are in MUPIP REORG UPGRADE/DOWNGRADE or DSE MAPS or DSE CHANGE -BHEAD
	 * or MUPIP REORG -TRUNCATE */
	assert(mu_upgrade_in_prog || mu_reorg_encrypt_in_prog || dse_running || (0 != reference_cnt)
		|| (NULL != csa->nl && process_id == csa->nl->trunc_pid));
	if (0 < reference_cnt)
	{	/* Blocks were allocated in this bitmap. Check if local bitmap became full as a result. If so update mastermap. */
		bml_full = bml_find_free(0, (SIZEOF(blk_hdr) + (is_mm ? lclmap : ((sm_uc_ptr_t)GDS_REL2ABS(lclmap)))), total_blks);
		if (NO_FREE_SPACE == bml_full)
		{
			bit_clear(blkid / bplmap, MM_ADDR(csd));
			change_bmm = TRUE;
			if ((0 == csd->extension_size)	/* no extension and less than a bit map or less than 1/32 (3.125%) */
				&& ((BLKS_PER_LMAP > cti->free_blocks) || ((cti->total_blks >> 5) > cti->free_blocks)))
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_FREEBLKSLOW, 4, &(cti->free_blocks), &(cti->total_blks),
					 DB_LEN_STR(gv_cur_region));
		}
	} else if (0 > reference_cnt)
	{	/* blocks were freed up in this bitmap. check if local bitmap became non-full as a result. if so update mastermap */
		if (FALSE == bit_set(blkid / bplmap, MM_ADDR(csd)))
			change_bmm = TRUE;
		assert((inctn_bmp_mark_free_gtm == inctn_opcode) || (inctn_bmp_mark_free_mu_reorg == inctn_opcode)
				|| (inctn_blkmarkfree == inctn_opcode) || dse_running);
		if ((inctn_bmp_mark_free_gtm == inctn_opcode) || (inctn_bmp_mark_free_mu_reorg == inctn_opcode))
		{	/* coming in from gvcst_bmp_mark_free. adjust "csd->blks_to_upgrd" if necessary */
			assert(!dollar_tlevel);	/* gvcst_bmp_mark_free runs in non-TP */
			/* Bitmap block should be the only block updated in this transaction. The only exception is if the
			 * previous cw-set-element is of type gds_t_busy2free (which does not go through bg_update) */
			assert((1 == cw_set_depth)
				|| ((2 == cw_set_depth) && (gds_t_busy2free == (cs-1)->old_mode)));
			/* When deleting pre-V7m index blocks, decrement blks_to_upgrd */
			if (!mu_upgrade_in_prog && (0 != inctn_detail.blknum_struct.blknum)
					&& (!cs_data->fully_upgraded && (GDSV6 < cs_data->desired_db_format))
					&& (GDSV7m > (cs-1)->ondsk_blkver) && !(cs-1)->done)
			{
				DECR_BLKS_TO_UPGRD(csa, csd, 1);
#ifdef				DEBUG_BLKS_TO_UPGRD
				util_out_print("!UL - 0x0 !AD:0x!@XQ:!UL:!UL 0x!@XQ:!UL:!UL", TRUE, cs_data->blks_to_upgrd,
						REG_LEN_STR(gv_cur_region),
						&(cs-1)->blk, (cs-1)->ondsk_blkver, (cs-1)->level,
						&(cs)->blk, (cs)->ondsk_blkver, (cs)->level);
#endif
			}
		}
	}
	/* else cs->reference_cnt is 0, this means no free/busy state change in non-bitmap blocks, hence no mastermap change */
	if (change_bmm)
	{	/* The following works while all uses of these fields are in crit */
		cnl = csa->nl;
		if (blkid > cnl->highest_lbm_blk_changed)
			cnl->highest_lbm_blk_changed = blkid;	    /* Retain high-water mark */
	}
	return;
}

enum cdb_sc mm_update(cw_set_element *cs, trans_num ctn, trans_num effective_tn)
{
	block_id		blkid;
	cw_set_element		*cs_ptr, *nxt;
	off_chain		chain;
	sm_uc_ptr_t		chain_ptr, db_addr[2];
	boolean_t		write_to_snapshot_file, long_blk_id;
	snapshot_context_ptr_t	lcl_ss_ctx;
#	ifdef DEBUG
	int4			blk_id_sz;
	jbuf_rsrv_struct_t	*jrs;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG
	/* Assert that we never start db commit until all journal records have been written out in phase2 */
	jrs = dollar_tlevel ? cs_addrs->sgm_info_ptr->jbuf_rsrv_ptr : TREF(nontp_jbuf_rsrv);
	assert((NULL == jrs) || !jrs->tot_jrec_len);
#	endif
	assert(cs_addrs->now_crit);
	assert((gds_t_committed > cs->mode) && (gds_t_noop < cs->mode));
	INCR_DB_CSH_COUNTER(cs_addrs, n_bgmm_updates, 1);
	blkid = cs->blk;
	assert((0 <= blkid) && (blkid < cs_addrs->ti->total_blks));
	db_addr[0] = MM_BASE_ADDR(cs_addrs) + ((sm_off_t)cs_data->blk_size * (blkid));
	/* check for online backup -- ATTN: this part of code is similar to the BG_BACKUP_BLOCK macro */
	if ((blkid >= cs_addrs->nl->nbb) && (NULL != cs->old_block)
		&& (0 == cs_addrs->shmpool_buffer->failed)
		&& (((blk_hdr_ptr_t)(db_addr[0]))->tn < cs_addrs->shmpool_buffer->backup_tn)
		&& (((blk_hdr_ptr_t)(db_addr[0]))->tn >= cs_addrs->shmpool_buffer->inc_backup_tn))
	{
		backup_block(cs_addrs, blkid, NULL, db_addr[0]);
		if (!dollar_tlevel)
			block_saved = TRUE;
		else
			sgm_info_ptr->backup_block_saved = TRUE;
	}
	if (SNAPSHOTS_IN_PROG(cs_addrs) && (NULL != cs->old_block))
	{
		lcl_ss_ctx = SS_CTX_CAST(cs_addrs->ss_ctx);
		assert(lcl_ss_ctx);
		if (blkid < lcl_ss_ctx->total_blks)
		{
			if (FASTINTEG_IN_PROG(lcl_ss_ctx))
			{
				DO_FAST_INTEG_CHECK(db_addr[0], cs_addrs, cs, lcl_ss_ctx, blkid, write_to_snapshot_file);
			} else
				write_to_snapshot_file = TRUE;
			if (write_to_snapshot_file)
				WRITE_SNAPSHOT_BLOCK(cs_addrs, NULL, db_addr[0], blkid, lcl_ss_ctx);
			assert(!FASTINTEG_IN_PROG(lcl_ss_ctx) || !write_to_snapshot_file
					|| ss_chk_shdw_bitmap(cs_addrs, lcl_ss_ctx, blkid));
			assert(FASTINTEG_IN_PROG(lcl_ss_ctx) || ss_chk_shdw_bitmap(cs_addrs, lcl_ss_ctx, blkid)
					|| ((blk_hdr_ptr_t)(db_addr[0]))->tn >= lcl_ss_ctx->ss_shm_ptr->ss_info.snapshot_tn);
		}
	}
	if (gds_t_writemap == cs->mode)
	{
		assert(0 == (blkid & (BLKS_PER_LMAP - 1)));
		if (FALSE == cs->done)
			gvcst_map_build(cs->upd_addr.map, db_addr[0], cs, effective_tn);
		else
		{	/* It has been built; Update tn in the block and copy from private memory to shared space. */
			assert(write_after_image);
			assert(((blk_hdr_ptr_t)cs->new_buff)->tn == effective_tn);
			memcpy(db_addr[0], cs->new_buff, ((blk_hdr_ptr_t)cs->new_buff)->bsiz);
		}
		bm_update(cs, db_addr[0], TRUE);
	} else
	{	/* either it is a non-local bit-map or we are in dse_maps or MUPIP RECOVER writing an AIMG record */
		assert((0 != (blkid & (BLKS_PER_LMAP - 1))) || write_after_image);
		if (!cs_data->fully_upgraded)
		{	/* Transitional DB format */
			assert(GDSV6 < cs_data->desired_db_format);
			if (GDSV7m != cs->ondsk_blkver)
			{
				if (gds_t_acquired == cs->mode)
				{	/* Increment blks_to_upgrd for new V6p idx blocks */
					INCR_BLKS_TO_UPGRD(cs_addrs, cs_data, 1);
#ifdef					DEBUG_BLKS_TO_UPGRD
					util_out_print("!UL + 0x!@XQ !AD:0x!@XQ:!UL:!UL:!UL", TRUE, cs_data->blks_to_upgrd, &ctn,
							REG_LEN_STR(gv_cur_region), &cs->blk, cs->ondsk_blkver,
							cs->level, cs->mode);
#endif
				} else if (!mu_upgrade_in_prog && (0 == cs->level) && (GDSV6 == cs->ondsk_blkver)
						&& (gds_t_write_recycled != cs->mode) && !cs->done)
				{	/* Level zero block, update the header and decrement blks_to_upgrd */
					DECR_BLKS_TO_UPGRD(cs_addrs, cs_data, 1);
#ifdef					DEBUG_BLKS_TO_UPGRD
					util_out_print("!UL - 0x!@XQ !AD:0x!@XQ:!UL:!UL:!UL", TRUE, cs_data->blks_to_upgrd, &ctn,
							REG_LEN_STR(gv_cur_region), &cs->blk, cs->ondsk_blkver,
							cs->level, cs->mode);
#endif
					cs->ondsk_blkver = cs_data->desired_db_format;
				}
			} else if ((GDSV7m == cs->ondsk_blkver) && mu_upgrade_in_prog)
			{					/* Decrement blks_to_upgrd for new V7m blocks */
				DECR_BLKS_TO_UPGRD(cs_addrs, cs_data, 1);
#ifdef				DEBUG_BLKS_TO_UPGRD
				util_out_print("!UL - 0x!@XQ !AD:0x!@XQ:!UL:!UL:!UL", TRUE, cs_data->blks_to_upgrd, &ctn,
						REG_LEN_STR(gv_cur_region), &cs->blk, cs->ondsk_blkver, cs->level, cs->mode);
#endif
			}
		}
		if (FALSE == cs->done)
		{	/* if the current block has not been built (from being referenced in TP) */
			if (NULL != cs->new_buff)
				cs->first_copy = TRUE;
			gvcst_blk_build(cs, db_addr[0], effective_tn);
		} else
		{	/* It has been built; Update tn in the block and copy from private memory to shared space */
			assert(write_after_image || dollar_tlevel);
			assert(dse_running || (ctn == effective_tn));
				/* ideally should be dse_chng_bhead specific but using generic dse_running flag for now */
			if (!dse_running)
				((blk_hdr_ptr_t)db_addr[0])->tn = ((blk_hdr_ptr_t)cs->new_buff)->tn = ctn;
			memcpy(db_addr[0], cs->new_buff, ((blk_hdr_ptr_t)cs->new_buff)->bsiz);
		}
		assert(SIZEOF(blk_hdr) <= ((blk_hdr_ptr_t)db_addr[0])->bsiz);
		assert((int)(((blk_hdr_ptr_t)db_addr[0])->bsiz) > 0);
		assert((int)(((blk_hdr_ptr_t)db_addr[0])->bsiz) <= cs_data->blk_size);
		long_blk_id = IS_64_BLK_ID(db_addr[0]);
		DEBUG_ONLY(blk_id_sz = SIZEOF_BLK_ID(long_blk_id));
		if (!dollar_tlevel)
		{
			if (0 != cs->ins_off)
			{	/* reference to resolve: insert real block numbers in the buffer */
				assert(0 <= (short)cs->index);
				assert(&cw_set[cs->index] < cs);
				assert((SIZEOF(blk_hdr) + SIZEOF(rec_hdr)) <= cs->ins_off);
				assert((cs->ins_off + blk_id_sz) <= ((blk_hdr_ptr_t)db_addr[0])->bsiz);
				WRITE_BLK_ID(long_blk_id, cw_set[cs->index].blk, db_addr[0] + cs->ins_off);
				if (((nxt = cs + 1) < &cw_set[cw_set_depth]) && (gds_t_write_root == nxt->mode))
				{	/* If the next cse is a WRITE_ROOT, it contains a second block pointer
					 * to resolve though it operates on the current cse's block.
					 */
					assert(0 <= (short)nxt->index);
					assert(&cw_set[nxt->index] < nxt);
					assert((SIZEOF(blk_hdr) + SIZEOF(rec_hdr)) <= nxt->ins_off);
					assert((nxt->ins_off + blk_id_sz) <= ((blk_hdr_ptr_t)db_addr[0])->bsiz);
					WRITE_BLK_ID(long_blk_id, cw_set[nxt->index].blk, db_addr[0] + nxt->ins_off);
				}
			}
		} else
		{	/* TP */
			if (0 != cs->first_off)
			{	/* TP resolve pointer references to new blocks */
				for (chain_ptr = db_addr[0] + cs->first_off; ; chain_ptr += chain.next_off)
				{
					READ_OFF_CHAIN(long_blk_id, &chain, chain_ptr);
					assert(1 == chain.flag);
					assert((int)(chain_ptr - db_addr[0] + chain.next_off + blk_id_sz)
							<= (int)(((blk_hdr_ptr_t)db_addr[0])->bsiz));
					assert((SIZEOF(int) * 8) >= CW_INDEX_MAX_BITS);
					assert((int)chain.cw_index < sgm_info_ptr->cw_set_depth);
					tp_get_cw(sgm_info_ptr->first_cw_set, (int)chain.cw_index, &cs_ptr);
					WRITE_BLK_ID(long_blk_id, cs_ptr->blk, chain_ptr);
					if (0 == chain.next_off)
						break;
				}
			}
		}	/* TP */
	}	/* not a map */
	CERT_BLK_IF_NEEDED(certify_all_blocks, gv_cur_region, cs, db_addr[0], gv_target);
	return cdb_sc_normal;
}

/* update buffered global database */
enum cdb_sc bg_update_phase1(cw_set_element *cs, trans_num ctn)
{
	boolean_t		twinning_on;
	int4			n;
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr, cr_new;
	boolean_t		read_finished, wait_for_rip, write_finished, intend_finished;
	block_id		blkid;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	enum gds_t_mode		mode;
	enum db_ver		desired_db_format;
	gv_namehead		*gvt;
	srch_blk_status		*blk_hist;
	void_ptr_t		retcrptr;
#	ifdef DEBUG
	boolean_t		read_before_image;
	cache_rec_ptr_t		save_cr;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;		/* Local access copies */
	csd = csa->hdr;
	cnl = csa->nl;
	assert(csd == cs_data);
	mode = cs->mode;
	assert((gds_t_committed > mode) && (gds_t_noop < mode));
	assert(0 != ctn);
	assert(csa->now_crit);
	blkid = cs->blk;
	/* assert changed to assertpro 2/15/2012. can be changed back once reorg truncate has been running for say 3 to 4 years */
	assert((0 <= blkid) && (blkid < csa->ti->total_blks));
	INCR_DB_CSH_COUNTER(csa, n_bgmm_updates, 1);
	/* If "cs" had a "bt" noted down in t_end/tp_tend, use it and try to avoid a heavyweight "bt_put()".
	 * Note that the noted "bt" could have been reused for a different block before we reach here (more likely
	 * in the tp_tend case if a lot of blocks were updated) due to a prior call to "bg_update_phase1()" invoking
	 * "bt_put()" for a different block in case the noted "bt" was the oldest bt at that point in time. Hence the
	 * "blkid != bt->blk" check below. If they are indeed different, treat this as if we did not note down a "bt"
	 * and fall through to the heavyweight "bt_put()" code path.
	 *
	 * If noted "bt" is still non-NULL, then note down "cs->cr" too and avoid a heavyweight "db_csh_get()" call.
	 */
	bt = cs->bt;
	if ((NULL != bt) && (blkid != bt->blk))
		bt = NULL;
	if (NULL != bt)
	{
		bt_rec_ptr_t		q0;
		th_rec_ptr_t		th;

#		ifdef DEBUG
		bt_rec_ptr_t	bt2;

		bt2 = bt_get(blkid);
		assert(bt2 == bt);
#		endif
		/* Implement a faster bt_put() */
		q0 = (bt_rec_ptr_t)((sm_uc_ptr_t)bt + bt->tnque.fl);
		th = (th_rec_ptr_t)remqt((que_ent_ptr_t)((sm_uc_ptr_t)q0 + SIZEOF(th->tnque)));
		assert(EMPTY_QUEUE != (sm_long_t)th);
		insqt((que_ent_ptr_t)th, (que_ent_ptr_t)csa->th_base);
		bt->tn = ctn;
		cr = cs->cr; /* Note down "cr" from "cs" as well to try and avoid a "db_csh_get" */
		assert((CR_NOTVALID == bt->cache_index) || (cr == (cache_rec_ptr_t)GDS_REL2ABS(bt->cache_index)));
		assert((CR_NOTVALID != bt->cache_index) || (NULL == cr));
	} else
	{
		bt = bt_put(csa, blkid);
		GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_BTPUTNULL, bt, NULL);
		if (NULL == bt)
		{
			assert(ydb_white_box_test_case_enabled);
			return cdb_sc_cacheprob;
		}
		cr = (cache_rec_ptr_t)(INTPTR_T)bt->cache_index;
		if ((cache_rec_ptr_t)CR_NOTVALID == cr)
			cr = NULL;
		else
			cr = (cache_rec_ptr_t)GDS_REL2ABS(cr);
	}
	if (cs->write_type & GDS_WRITE_KILLTN)
		bt->killtn = ctn;
	DEBUG_ONLY(read_before_image =
			((JNL_ENABLED(csa) && csa->jnl_before_image) || csa->backup_in_prog || SNAPSHOTS_IN_PROG(csa));)
	if (NULL == cr)
	{	/* no cache record associated with the bt_rec */
		cr = db_csh_get(blkid);
		GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_DBCSHGET_INVALID, cr, (cache_rec_ptr_t)CR_NOTVALID);
		if (NULL == cr)
		{	/* no cache_rec associated with the block */
			assert(((gds_t_acquired == mode) && (!read_before_image || (NULL == cs->old_block)))
					|| ((gds_t_acquired != mode) && (NULL != cs->new_buff)));
			INCR_DB_CSH_COUNTER(csa, n_bg_update_creates, 1);
			cr = db_csh_getn(blkid);
#			ifdef DEBUG
			int	dummy;

			save_cr = NULL;
			if (ydb_white_box_test_case_enabled)
			{
				save_cr = cr;	/* save cr for r_epid cleanup before setting it to INVALID */
				/* stop self to test sechshr_db_clnup clears the read state */
				if (WBTEST_SIGTSTP_IN_T_QREAD == ydb_white_box_test_case_number)
				{	/* this should never fail, but because of the way we developed the test we got paranoid */
					dummy = kill(process_id, SIGTERM);
					assert(0 == dummy);
					for (dummy = 10; dummy; dummy--)
						LONG_SLEEP(10); /* time for sigterm to take hit before we clear block_now_locked */
				}
			}
#			endif
			GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_DBCSHGETN_INVALID, cr, (cache_rec_ptr_t)CR_NOTVALID);
			if ((cache_rec_ptr_t)CR_NOTVALID == cr)
			{
				assert(ydb_white_box_test_case_enabled);
#				ifdef DEBUG
				if (NULL != save_cr)
				{	/* release the r_epid lock on the valid cache-record returned from db_csh_getn */
					assert(save_cr->r_epid == process_id);
					save_cr->r_epid = 0;
					assert(0 == save_cr->read_in_progress);
					RELEASE_BUFF_READ_LOCK(save_cr);
					TREF(block_now_locked) = NULL;
				}
#				endif
				BG_TRACE_PRO(wcb_t_end_sysops_nocr_invcr);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_t_end_sysops_nocr_invcr"),
					process_id, &ctn, DB_LEN_STR(gv_cur_region));
				return cdb_sc_cacheprob;
			}
			assert(NULL != cr);
			assert(cr->blk == blkid);
			assert(0 == cr->in_cw_set);
		} else if ((cache_rec_ptr_t)CR_NOTVALID == cr)
		{
			assert(ydb_white_box_test_case_enabled);
			BG_TRACE_PRO(wcb_t_end_sysops_cr_invcr);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_t_end_sysops_cr_invcr"),
				process_id, &ctn, DB_LEN_STR(gv_cur_region));
			return cdb_sc_cacheprob;
		} else if (-1 != cr->read_in_progress)
		{	/* wait for another process in t_qread to stop overlaying the buffer (possible in the following cases)
			 *	a) reuse of a killed block that's still in the cache
			 *	b) the buffer has already been constructed in private memory (cse->new_buff is non-NULL)
			 */
			assert(((gds_t_acquired == mode) && (!read_before_image || (NULL == cs->old_block)))
					|| ((gds_t_acquired != mode) && (NULL != cs->new_buff)));
			read_finished = wcs_read_in_progress_wait(cr, WBTEST_BG_UPDATE_READINPROGSTUCK1);
			if (!read_finished)
			{
				BG_TRACE_PRO(wcb_t_end_sysops_rip_wait);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_t_end_sysops_rip_wait"),
					process_id, &ctn, DB_LEN_STR(gv_cur_region));
				return cdb_sc_cacheprob;
			}
			assert(-1 == cr->read_in_progress);
		}
		cs->first_copy = TRUE;
		assert(0 == cr->in_tend);
		/* Set cr->in_tend before the semaphore (and data_invalid). But note that cr->in_tend needs to be reset
		 * in case we encounter an error in phase1. This way if "secshr_db_clnup" is invoked and does a commit for
		 * this block into a new cr it would set cr->stopped and "wcs_recover" would know to use the cr->stopped one.
		 * If "secshr_db_clnup" is not invoked (e.g. kill -9 of process in middle of commit), "wcs_recover" would
		 * see cr->in_tend as non-zero and in that case it can know this cr cannot be discarded (i.e. no corresponding
		 * new cr with cr->stopped=TRUE got created).
		 */
		cr->in_tend = process_id;
		assert(0 == cr->dirty);
		/* Even though the buffer is not in the active (or wip queue if csd->asyncio is ON) and we are in crit, it is
		 * possible for the cache-record to have the write interlock still set. This is because in wcs_wtstart
		 * (and wcs_wtfini) csr->dirty is reset to 0 before it releases the write interlock on the buffer.
		 * Because all routines (bt_put, db_csh_getn and wcs_get_space) wait only for cr->dirty to become 0 before
		 * considering the buffer ready for reuse, it is possible to have the write interlock set at this
		 * point with a concurrent wcs_wtstart/wcs_wtfini almost ready to release the interlock. In this case wait.
		 * Hence we cannot call LOCK_NEW_BUFF_FOR_UPDATE directly.
		 * Since the only case where the write interlock is not clear is a two-instruction window
		 * (described in the above comment), we don't expect the lock-not-clear situation to be frequent.
		 * Hence, for performance reasons we do the check before invoking the wcs_write_in_progress_wait function
		 * (instead of moving the if check into the function which would mean an unconditional function call).
		 */
		if (LATCH_CLEAR != WRITE_LATCH_VAL(cr))
		{
			write_finished = wcs_write_in_progress_wait(cnl, cr, WBTEST_BG_UPDATE_DIRTYSTUCK1);
			if (!write_finished)
			{
				assert(ydb_white_box_test_case_enabled);
				BG_TRACE_PRO(wcb_t_end_sysops_dirtystuck1);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED,
						6, LEN_AND_LIT("wcb_t_end_sysops_dirtystuck1"), process_id, &ctn,
						DB_LEN_STR(gv_cur_region));
				cr->in_tend = 0;
				return cdb_sc_cacheprob;
			}
		} else
			LOCK_NEW_BUFF_FOR_UPDATE(cr);	/* writer has released interlock and this process is crit */
		assert(LATCH_SET <= WRITE_LATCH_VAL(cr));
		BG_TRACE(new_buff);
		cr->bt_index = GDS_ABS2REL(bt);
		bt->cache_index = (int4)GDS_ABS2REL(cr);
		cr->backup_cr_is_twin = FALSE;
	} else	/* end of if else on cr NOTVALID */
	{
		assert(bt == (bt_rec_ptr_t)GDS_REL2ABS(cr->bt_index));
		assert(CR_BLKEMPTY != cr->blk);
		assert(blkid == cr->blk);
		if (cr->in_tend)
		{	/* Wait for another process in bg_update_phase2 to stop overlaying the buffer (possible in case of)
			 *	a) reuse of a killed block that's still in the cache
			 *	b) the buffer has already been constructed in private memory (cse->new_buff is non-NULL)
			 *	c) MUPIP REORG -UPGRADE in process - no TP and does not have a new_buff
			 */
			assert(process_id != cr->in_tend);
			assert(((gds_t_acquired == mode) && (!read_before_image || (NULL == cs->old_block)))
					|| ((gds_t_acquired != mode) && ((NULL != cs->new_buff) || mu_upgrade_in_prog)));
			intend_finished = wcs_phase2_commit_wait(csa, cr);
			GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_INTENDSTUCK, intend_finished, 0);
			if (!intend_finished)
			{
				assert(ydb_white_box_test_case_enabled);
				BG_TRACE_PRO(wcb_t_end_sysops_intend_wait);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED,
						6, LEN_AND_LIT("wcb_t_end_sysops_intend_wait"),
						process_id, &ctn, DB_LEN_STR(gv_cur_region));
				return cdb_sc_cacheprob;
			}
		}
		assert(0 == cr->in_tend);
		assert(0 == cr->data_invalid);
		cr->in_tend = process_id;
		wait_for_rip = FALSE;
		/* If we find the buffer we intend to update is concurrently being flushed to disk,
		 *	a) If asyncio=OFF, writes are to the filesystem cache (which are relatively fast) so wait for the
		 *		active writer to finish flushing.
		 *	b) If asyncio=ON, then writes are with O_DIRECT and directly hardened to disk (no cache in between)
		 *		so take a long time to return. Do not want to wait for that IO event while holding crit.
		 *		Create a twin buffer and dump the update on that buffer instead of waiting.
		 */
		LOCK_BUFF_FOR_UPDATE(cr, n, &cnl->db_latch);
		assert((LATCH_CONFLICT >= n) && (LATCH_CLEAR <= n));
		twinning_on = TWINNING_ON(csd);
		cr->backup_cr_is_twin = FALSE;
		if (!twinning_on)
		{	/* TWINNING is not active because of asyncio=OFF */
			if (!OWN_BUFF(n))
			{
				write_finished = wcs_write_in_progress_wait(cnl, cr, WBTEST_BG_UPDATE_DIRTYSTUCK2);
				if (!write_finished)
				{
					assert(ydb_white_box_test_case_enabled);
					BG_TRACE_PRO(wcb_t_end_sysops_dirtystuck2);
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED,
							6, LEN_AND_LIT("wcb_t_end_sysops_dirtystuck2"),
							process_id, &ctn, DB_LEN_STR(gv_cur_region));
					cr->in_tend = 0;
					return cdb_sc_cacheprob;
				}
			}
			assert((0 == cr->dirty) || (-1 == cr->read_in_progress)); /* dirty buffer cannot be read in progress */
			if (-1 != cr->read_in_progress)
				wait_for_rip = TRUE;
		} else
		{	/* TWINNING is enabled because of asyncio=ON. Create TWIN instead of waiting for write to complete */
			if (0 == cr->dirty)		/* Free, move to active queue */
			{	/* If "asyncio" is turned ON, "wcs_wtfini" is the only one that will reset "cr->dirty". And
				 * that runs inside crit. Since we hold crit now, wcs_wtfini could not have run after we
				 * did the LOCK_BUFF_FOR_UPDATE and so we are guaranteed the current value of the write
				 * interlock is LATCH_SET. Assert that.
				 */
				assert(LATCH_SET == WRITE_LATCH_VAL(cr));
				assert(0 == cr->twin);
				if (-1 != cr->read_in_progress)
					wait_for_rip = TRUE;
			} else if (LATCH_CONFLICT > n)
			{	/* it's modified but available */
				assert(-1 == cr->read_in_progress);	/* so "wait_for_rip" can stay FALSE */
			} else
			{	/* This cr is owned by a concurrent writer. Create TWIN. */
				assert(-1 == cr->read_in_progress);	/* so "wait_for_rip" can stay FALSE */
				cr_new = db_csh_getn(blkid);
#				ifdef DEBUG
				save_cr = NULL;
				if (ydb_white_box_test_case_enabled)
					save_cr = cr_new;	/* save cr for r_epid cleanup before setting to INVALID */
#				endif
				GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_DBCSHGETN_INVALID2, cr_new, (cache_rec *)CR_NOTVALID);
				if ((cache_rec *)CR_NOTVALID == cr_new)
				{
					assert(ydb_white_box_test_case_enabled);
#					ifdef DEBUG
					if (NULL != save_cr)
					{	/* Since we are simulating a "db_csh_getn" failure return,
						 * undo all changes in db_csh_getn that would otherwise persist.
						 */
						assert(save_cr->r_epid == process_id);
						retcrptr = remqh((que_ent_ptr_t)((sm_uc_ptr_t)save_cr + save_cr->blkque.bl));
						assert(retcrptr == save_cr);
						save_cr->r_epid = 0;
						assert(0 == save_cr->read_in_progress);
						RELEASE_BUFF_READ_LOCK(save_cr);
						assert(blkid == save_cr->blk);
						save_cr->blk = CR_BLKEMPTY;
						TREF(block_now_locked) = NULL;
					}
#					endif
					BG_TRACE_PRO(wcb_t_end_sysops_dirty_invcr);
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
							LEN_AND_LIT("wcb_t_end_sysops_dirty_invcr"),
							process_id, &ctn, DB_LEN_STR(gv_cur_region));
					cr->in_tend = 0;
					return cdb_sc_cacheprob;
				}
				assert(NULL != cr_new);
				assert(0 == cr_new->dirty);
				assert(cr_new->blk == blkid);
				LOCK_NEW_BUFF_FOR_UPDATE(cr_new);	/* is new or cleaning up old; can't be active */
				if (cr != cr_new)
				{	/* db_csh_getn did not give back the same cache-record, which it could do
					 * if it had to invoke wcs_wtfini.
					 */
					assert(0 == cr_new->in_cw_set);
					assert(0 == cr_new->in_tend);
					if (0 != cr->dirty)
					{	/* "cr" is still dirty and in WIP queue. "cr" cannot already have an OLDER twin.
						 * This is because we know "n" == LATCH_CONFLICT at this point which means
						 * "wcs_wtstart" took over this "cr" for writing. But it will not have done that
						 * if "cr->twin" was still non-zero.
						 */
						assert(!cr->twin);
						if (!dollar_tlevel)		/* stuff it in the array before setting in_cw_set */
						{
							assert(ARRAYSIZE(cr_array) > cr_array_index);
							PIN_CACHE_RECORD(cr_new, cr_array, cr_array_index);
						} else
							TP_PIN_CACHE_RECORD(cr_new, sgm_info_ptr);
						assert(process_id == cr->in_tend);
						cr->in_tend = 0;
						cr_new->in_tend = process_id;
						cr_new->ondsk_blkver = cr->ondsk_blkver; /* copy blk version from old cache rec */
						if (gds_t_writemap == mode)
						{	/* gvcst_map_build doesn't do first_copy */
							memcpy(GDS_REL2ABS(cr_new->buffaddr), GDS_REL2ABS(cr->buffaddr),
													BM_SIZE(csd->bplmap));
						}
						/* form twin*/
						cr_new->twin = GDS_ABS2REL(cr);
						cr->twin = GDS_ABS2REL(cr_new);
						/* Currently we compare out-of-crit "cr->buffaddr->tn" with the "hist->tn"
						 * to see if a block has been modified since the time we did our read
						 * (places are t_qread, tp_hist, gvcst_search and gvcst_put). With twinning,
						 * if a cache-record is currently being written to disk, and we need to
						 * update it, we find out another free cache-record and twin the two
						 * and make all changes only in the newer twin. Because of this, if we
						 * are doing our blkmod check against the old cache-record, our check
						 * may incorrectly conclude that nothing has changed. To prevent this
						 * the cycle number of the older twin has to be incremented. This way,
						 * the following cycle-check (in all the above listed places, a
						 * cdb_sc_blkmod check is immediately followed by a cycle check) will
						 * detect a restartable condition. Note that cr->bt_index should be set to 0
						 * BEFORE cr->cycle++ as t_qread relies on this order.
						 */
						cr->bt_index = 0;
						cr->cycle++;	/* increment cycle whenever blk number changes (for tp_hist) */
						cs->first_copy = TRUE;
						if (gds_t_write == mode) /* update landed in a different cache-record (twin) */
						{	/* If valid clue and this block is in it, need to update buffer address to
							 * point to NEWER twin. The NEWER twin has not yet been built (happens in
							 * phase2 of commit) so its block contents are invalid but it is okay for
							 * the clue to point to it since the clue will not be used until the commit
							 * completes. In case of any error before we reach phase2 for this block,
							 * we would invoke "secshr_db_clnup" which will create a cr->stopped
							 * cache-record and update clue to point to that more-uptodate cr. So we
							 * are fine setting clue prematurely to point to the incomplete NEWER twin
							 * here in all cases.
							 */
							gvt = (!dollar_tlevel ? gv_target : cs->blk_target);
							if ((NULL != gvt) && (0 != gvt->clue.end))
							{
								blk_hist = &gvt->hist.h[cs->level];
								if (blk_hist->blk_num == blkid)
								{
									blk_hist->buffaddr =
											(sm_uc_ptr_t)GDS_REL2ABS(cr_new->buffaddr);
									blk_hist->cr = cr_new;
									blk_hist->cycle = cr_new->cycle;
								}
							}
						}
						cr = cr_new;
						cr->backup_cr_is_twin = TRUE;	/* OLDER "twin" has before-image for backup etc. */
						/* Note that a "cr"'s read_in_progress will be set whenever it is obtained through
						 *   db_csh_getn which is done for two cases in the bg_update function,
						 *	(i) one for a newly created block
						 *	(ii) one for the twin of an existing block
						 * This read-in-progress lock is released before the actual gvcst_blk_build of the
						 *	block by a RELEASE_BUFF_READ_LOCK done down below in a codepath common to
						 *	both case(i) and (ii).
						 * Both cases result in buffers that are empty and hence should not be used by any
						 *	other process for doing their gvcst_blk_search. To this effect we should
						 *	set things up so that one of the validation checks will fail later on these
						 *	buffers.
						 * Case (i) is easy since no other process would be trying to search through a
						 *	to-be-created block and hence requires no special handling.
						 * Case (ii) refers to an existing block and hence we need to set the block-tn in
						 *	the empty buffer to be csa->ti->curr_tn to ensure the other process using
						 *	this buffer for their gvcst_blk_search fails the cdb_sc_blkmod check in the
						 *	intermediate validation routine tp_hist.
						 * Since the above needs to be done only for case (ii), we do the necessary stuff
						 *	here rather than just before the RELEASE_BUFF_READ_LOCK which is common to
						 *	both cases.
						 */
						((blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr))->tn = ctn;
					} else
					{	/* If not cr->dirty, then "wcs_wtfini" (invoked from "db_csh_getn" above)
						 * has processed it, just proceed with "cr". Discard "cr_new".
						 */
						assert(process_id == cr_new->r_epid);	/* "db_csh_getn" returned "cr_new" */
						cr_new->r_epid = 0;
						assert(0 == cr_new->read_in_progress);
						RELEASE_BUFF_READ_LOCK(cr_new);
						TREF(block_now_locked) = NULL;
						/* Release update lock on "cr_new" and get it on "cr" now that we changed minds */
						assert(!cr_new->dirty);
						WRITE_LATCH_VAL(cr_new) = LATCH_CLEAR;
						assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr));
						LOCK_NEW_BUFF_FOR_UPDATE(cr);
						/* The block "blkid" can no longer be found in "cr_new".
						 * It can only be found in "cr". Set that accordingly.
						 */
						assert(blkid == cr_new->blk);
						cr_new->blk = CR_BLKEMPTY;
					}
				}	/* end of if (cr != cr_new) */
				assert(cr->blk == blkid);
				bt->cache_index = GDS_ABS2REL(cr);
				cr->bt_index = GDS_ABS2REL(bt);
			}
		}
		if (wait_for_rip)
		{	/* wait for another process in t_qread to stop overlaying the buffer, possible due to
			 *	(a) reuse of a killed block that's still in the cache OR
			 *	(b) the buffer has already been constructed in private memory
			 */
			assert(((gds_t_acquired == mode) && (!read_before_image || (NULL == cs->old_block)))
					|| ((gds_t_acquired != mode) && (NULL != cs->new_buff)));
			read_finished = wcs_read_in_progress_wait(cr, WBTEST_BG_UPDATE_READINPROGSTUCK2);
			if (!read_finished)
			{
				assert(ydb_white_box_test_case_enabled);
				BG_TRACE_PRO(wcb_t_end_sysops_dirtyripwait);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
					LEN_AND_LIT("wcb_t_end_sysops_dirtyripwait"),
					process_id, &ctn, DB_LEN_STR(gv_cur_region));
				cr->in_tend = 0;
				return cdb_sc_cacheprob;
			}
			assert(-1 == cr->read_in_progress);
		}
	}	/* end of if / else on cr NOTVALID */
	if (0 == cr->in_cw_set)
	{	/* in_cw_set should always be set unless we're in DSE (indicated by dse_running)
		 * or writing an AIMG record (possible by either DSE or MUPIP JOURNAL RECOVER),
		 * or this is a newly created block, or we have an in-memory copy.
		 */
		 assert(dse_running || write_after_image || mu_upgrade_in_prog
			 || ((gds_t_acquired != mode) ? (0 != cs->new_buff): (!read_before_image || (NULL == cs->old_block))));
		if (!dollar_tlevel)		/* stuff it in the array before setting in_cw_set */
		{
			assert(ARRAYSIZE(cr_array) > cr_array_index);
			PIN_CACHE_RECORD(cr, cr_array, cr_array_index);
		} else
			TP_PIN_CACHE_RECORD(cr, sgm_info_ptr);
	}
	assert(0 == cr->data_invalid);
	if (0 != cr->r_epid)
	{	/* must have got it with a db_csh_getn */
#		ifdef DEBUG
		/* In rare cases (when free buffers are not easy to find), it is possible that "db_csh_getn" returned us a cr
		 * whose buffer was used as part of a "gvcst_blk_build" of a prior block in the currently committing transaction.
		 * In that case, the block header in that global buffer would hold a transaction number that is EQUAL TO the
		 * current database transaction number. This will then fail the "blktn < ctn" assert in "gvcst_blk_build"
		 * since in that case "blktn" will be EQUAL to "ctn" at the start of "gvcst_blk_build". To avoid that assert
		 * failure, reset the blktn to be one less than ctn here (only for debug builds).
		 */
		((blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr))->tn = (ctn - 1);
#		endif
		if ((gds_t_acquired != mode) || mu_upgrade_in_prog)
		{	/* Not a newly created block, yet we have got it with a db_csh_getn. This means we have an in-memory
			 * copy of the block already built. In that case, cr->ondsk_blkver is uninitialized. Copy it over
			 * from cs->ondsk_blkver which should hold the correct value.
			 */
			cr->ondsk_blkver = cs->ondsk_blkver;
		}
		assert(cr->r_epid == process_id);
		cr->r_epid = 0;
		assert(0 == cr->read_in_progress);
		RELEASE_BUFF_READ_LOCK(cr);
		TREF(block_now_locked) = NULL;
	}
	/* Update csd->blks_to_upgrd while we have crit */
	/* assert that appropriate inctn journal records were written at the beginning of the commit in t_end */
	assert((inctn_blkupgrd_fmtchng != inctn_opcode) || ((GDSV6p == cr->ondsk_blkver) && (GDSV7m == csd->desired_db_format)));
	assert(!(JNL_ENABLED(csa) && csa->jnl_before_image) || !mu_reorg_nosafejnl
		|| (inctn_blkupgrd != inctn_opcode) || (cr->ondsk_blkver == csd->desired_db_format));
	assert(!mu_upgrade_in_prog || !mu_reorg_encrypt_in_prog || (gds_t_acquired != mode));
	assert((gds_t_write_recycled != mode) || mu_upgrade_in_prog || mu_reorg_encrypt_in_prog);
	if (!csd->fully_upgraded)
	{
		assert(GDSV6 < csd->desired_db_format);
		desired_db_format = csd->desired_db_format;
		if (GDSV7m != cs->ondsk_blkver)
		{
			if  (gds_t_acquired == mode)
			{	/* Increment blks_to_upgrd for new V6p idx blocks */
				INCR_BLKS_TO_UPGRD(csa, csd, 1);	/* Increment blks_to_upgrd for new V6p index blocks */
#ifdef				DEBUG_BLKS_TO_UPGRD
				util_out_print("!UL + 0x!@XQ !AD:0x!@XQ:!UL:!UL:!UL", TRUE, csd->blks_to_upgrd, &ctn,
						REG_LEN_STR(gv_cur_region), &cs->blk, cs->ondsk_blkver, cs->level, cs->mode);
#endif
			} else if (!mu_upgrade_in_prog && (0 == cs->level) && (GDSV6 == cs->ondsk_blkver)
					&& (gds_t_write_recycled != cs->mode) && !cs->done)
			{	/* Level zero block, update the header and decrement blks_to_upgrd */
				DECR_BLKS_TO_UPGRD(cs_addrs, csd, 1);
#ifdef				DEBUG_BLKS_TO_UPGRD
				util_out_print("!UL - 0x!@XQ !AD:0x!@XQ:!UL:!UL:!UL", TRUE, csd->blks_to_upgrd, &ctn,
						REG_LEN_STR(gv_cur_region), &cs->blk, cs->ondsk_blkver, cs->level, cs->mode);
#endif
				cr->ondsk_blkver = cs->ondsk_blkver = desired_db_format;
			}
		} else if ((GDSV7m == cs->ondsk_blkver) && mu_upgrade_in_prog)
		{
			DECR_BLKS_TO_UPGRD(csa, csd, 1);	/* Decrement in transition block */
#ifdef			DEBUG_BLKS_TO_UPGRD
			util_out_print("!UL - 0x!@XQ !AD:0x!@XQ:!UL:!UL:!UL", TRUE, csd->blks_to_upgrd, &ctn,
					REG_LEN_STR(gv_cur_region), &cs->blk, cs->ondsk_blkver, cs->level, cs->mode);
#endif
		}
	}
	/* generic dse_running variable below is used for caller == dse_maps */
	assert((gds_t_writemap != mode) || dse_running || mu_upgrade_in_prog
		|| cr->twin || (CR_BLKEMPTY == cs->cr->blk) || ((cs->cr == cr) && (cs->cycle == cr->cycle)));
	/* Before marking this cache-record dirty, record the value of cr->dirty into cr->tn.
	 * This is used in phase2 to determine "recycled".
	 * Also, now that we have locked a buffer for commit, there is one less free buffer available. Decrement wc_in_free.
	 * Do not do this if the cache-record is already dirty since this would have already been done the first time
	 * it transitioned from non-dirty to dirty.
	 */
	if (0 == cr->dirty)
	{
		cr->tn = 0;
		SUB_ENT_FROM_FREE_QUE_CNT(cnl);
		INCR_GVSTATS_COUNTER(csa, cnl, n_clean2dirty, 1);
		cr->dirty = ctn;		/* block will be dirty.	 Note the tn in which this occurred */
		/* At this point cr->flushed_dirty_tn could be EQUAL to ctn if this cache-record was used to update a different
		 * block in this very same transaction and reused later for the current block. Reset it to 0 to avoid confusion.
		 */
		cr->flushed_dirty_tn = 0;
	} else
		cr->tn = ctn;
	/* Take backup of block in phase2 (outside of crit). */
	cs->cr = cr;		/* note down "cr" so phase2 can find it easily (given "cs") */
	/* If this is the first time the the database block has been written, we must write
	 * the entire database block if csd->write_fullblk = 2 */
	/* Note that the check for csd->write_fullblk happens when we decide to write the block,
	 * not here; so if the block is new, mark as needing first write */
	if (WAS_FREE(cs->blk_prior_state))
		cs->cr->needs_first_write = TRUE;
	cs->cycle = cr->cycle;	/* update "cycle" as well (used later in tp_clean_up to update cycle in history) */
	cs->old_mode = -cs->old_mode;	/* negate it to indicate phase1 is complete for this cse (used by secshr_db_clnup) */
	assert(0 > cs->old_mode);
	/* Final asserts before letting go of this cache-record in phase1 */
	assert(process_id == cr->in_tend);
	assert(process_id == cr->in_cw_set);
	assert(cr->blk == cs->blk);
	assert(cr->dirty);
	assert(cr->dirty <= ctn);
	/* We have the cr locked so a concurrent writer should not be touching this. */
	assert(0 == cr->epid);
	assert(cr->dirty > cr->flushed_dirty_tn);
	assert(cr->tn <= ctn);
	assert(0 == cr->data_invalid);
	assert(-1 == cr->read_in_progress);
	assert(LATCH_SET <= WRITE_LATCH_VAL(cr));
	return cdb_sc_normal;
}

enum cdb_sc bg_update_phase2(cw_set_element *cs, trans_num ctn, trans_num effective_tn)
{
	int4			n;
	off_chain		chain;
	sm_uc_ptr_t		blk_ptr, backup_blk_ptr, chain_ptr;
	cw_set_element		*cs_ptr, *nxt;
	cache_rec_ptr_t		cr, backup_cr;
	boolean_t		bmp_status, long_blk_id;
	block_id		blkid;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	cache_que_heads_ptr_t	cache_state;
	boolean_t		write_to_snapshot_file;
	snapshot_context_ptr_t	lcl_ss_ctx;
#	ifdef DEBUG
	int4			blk_id_sz;
	jbuf_rsrv_struct_t	*jrs;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	cr = cs->cr;
	/* Make sure asserts that were valid before letting go of this cache-record in phase1 are still so */
	assert(process_id == cr->in_tend);		/* should have been set in phase1 to update buffer */
	assert(process_id == cr->in_cw_set);		/* should have been set in phase1 to pin buffer until commit completes */
	assert(cr->blk == cs->blk);
	assert(cr->dirty);
	assert(cr->dirty <= ctn);
	/* We have the cr locked so a concurrent writer should not be touching this. */
	assert(0 == cr->epid);
	assert(cr->dirty > cr->flushed_dirty_tn);
	assert(cr->tn <= ctn);
	assert(0 == cr->data_invalid);
	assert(-1 == cr->read_in_progress);
	assert(LATCH_SET <= WRITE_LATCH_VAL(cr));	/* Assert that we hold the update lock on the cache-record */
	csa = cs_addrs;		/* Local access copies */
#	ifdef DEBUG
	/* Assert that we never start phase2 of db commit until all journal records have been written out in phase2 */
	jrs = dollar_tlevel ? csa->sgm_info_ptr->jbuf_rsrv_ptr : TREF(nontp_jbuf_rsrv);
	assert((NULL == jrs) || !jrs->tot_jrec_len);
#	endif
	csd = csa->hdr;
	cnl = csa->nl;
	blkid = cs->blk;
	/* The following assert should NOT go off, even with the possibility of concurrent truncates. The cases are:
	 * 1. blkid is a bitmap block. In this case, we've held crit since last checking for a truncate.
	 * 2. a non-bitmap block. We might not have crit at this point. A concurrent truncate may very well have happened,
	 *    BUT it should not have truncated as far as this block. Here's why: the bitmap block corresponding to blkid has
	 *    already been marked busy, which would signal (via highest_lbm_with_busy_blk) an ongoing mu_truncate to pull back.
	 *    The remaining possibility is that mu_truncate began after the bitmap block was marked busy. But in this case,
	 *    mu_truncate would see (in phase 1) that blkid has been marked busy. Another process could not have freed blkid
	 *    in the bitmap because this process has pinned blkid's corresponding buffer.
	 */
	assert((0 <= blkid) && (blkid < csa->ti->total_blks));
	GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_PHASE2FAIL, cr, NULL);
#	ifdef DEBUG
	if (NULL == cr)
	{
		assert(ydb_white_box_test_case_enabled);
		return cdb_sc_cacheprob;
	}
#	endif
	blk_ptr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
	/* Take backup of block in phase2 (outside of crit). */
	if (!WAS_FREE(cs->blk_prior_state)) /* don't do before image write for backup for FREE blocks */
	{
		if (!cr->backup_cr_is_twin)
		{
			backup_cr = cr;
			backup_blk_ptr = blk_ptr;
		} else
		{
			assert(TWINNING_ON(csd));
			backup_cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->twin);
			backup_blk_ptr = (sm_uc_ptr_t)GDS_REL2ABS(backup_cr->buffaddr);
		}
		BG_BACKUP_BLOCK(csa, csd, cnl, cr, cs, blkid, backup_cr, backup_blk_ptr, block_saved,		\
									sgm_info_ptr->backup_block_saved);
	}
	/* Update cr->ondsk_blkver to reflect the current desired_db_format. */
	SET_ONDSK_BLKVER(cr, csd, ctn);
	if (SNAPSHOTS_IN_PROG(cs_addrs) && (NULL != cs->old_block))
	{
		lcl_ss_ctx = SS_CTX_CAST(csa->ss_ctx);
		assert(lcl_ss_ctx);
		if (blkid < lcl_ss_ctx->total_blks)
		{
			if (FASTINTEG_IN_PROG(lcl_ss_ctx))
			{
				DO_FAST_INTEG_CHECK(blk_ptr, csa, cs, lcl_ss_ctx, blkid, write_to_snapshot_file);
			} else
				write_to_snapshot_file = TRUE;
			if (write_to_snapshot_file)
				WRITE_SNAPSHOT_BLOCK(csa, cr, NULL, blkid, lcl_ss_ctx);
			assert(!FASTINTEG_IN_PROG(lcl_ss_ctx) || !write_to_snapshot_file
					|| ss_chk_shdw_bitmap(csa, lcl_ss_ctx, blkid));
			assert(FASTINTEG_IN_PROG(lcl_ss_ctx) || ss_chk_shdw_bitmap(csa, lcl_ss_ctx, blkid)
					|| ((blk_hdr_ptr_t)(blk_ptr))->tn >= lcl_ss_ctx->ss_shm_ptr->ss_info.snapshot_tn);
		}
	}
	SET_DATA_INVALID(cr);	/* data_invalid should be set signaling intent to update contents of a valid block */
	if (gds_t_writemap == cs->mode)
	{
		assert(csa->now_crit);	/* at this point, bitmap blocks are built while holding crit */
		assert(0 == (blkid & (BLKS_PER_LMAP - 1)));
		if (FALSE == cs->done)
			gvcst_map_build(cs->upd_addr.map, blk_ptr, cs, effective_tn);
		else
		{	/* It has been built; Update tn in the block and copy from private memory to shared space */
			assert(write_after_image);
			VALIDATE_BM_BLK(blkid, (blk_hdr_ptr_t)blk_ptr, csa, gv_cur_region, bmp_status);
			assert(bmp_status);
			assert(((blk_hdr_ptr_t)cs->new_buff)->tn == effective_tn);
			memcpy(blk_ptr, cs->new_buff, ((blk_hdr_ptr_t)cs->new_buff)->bsiz);
			/* Since this is unusual code (either DSE or MUPIP RECOVER while playing AIMG records),
			 * we want to validate the bitmap block's buffer twice, once BEFORE and once AFTER the update.
			 */
			VALIDATE_BM_BLK(blkid, (blk_hdr_ptr_t)blk_ptr, csa, gv_cur_region, bmp_status);
			assert(bmp_status);
		}
		bm_update(cs, (sm_uc_ptr_t)cr->buffaddr, FALSE);
	} else
	{	/* either it is a non-local bit-map or we are in dse_maps or MUPIP RECOVER writing an AIMG record */
		assert((0 != (blkid & (BLKS_PER_LMAP - 1))) || write_after_image);
		/* we should NOT be in crit for phase2 except dse_maps/dse_chng_bhead OR if cse has a non-zero recompute list. The
		 * only exception to this is ONLINE ROLLBACK or MUPIP TRIGGER -UPGRADE which holds crit for the entire duration
		 */
		assert(!csa->now_crit || cs->recompute_list_head || dse_running || jgbl.onlnrlbk || TREF(in_trigger_upgrade)
			|| mu_upgrade_in_prog);
		if (FALSE == cs->done)
		{	/* if the current block has not been built (from being referenced in TP) */
			if (NULL != cs->new_buff)
				cs->first_copy = TRUE;
			gvcst_blk_build(cs, blk_ptr, effective_tn);
		} else
		{	/* It has been built; Update tn in the block and copy from private memory to shared space */
			assert(write_after_image || dollar_tlevel);
			assert(dse_running || (ctn == effective_tn));
				/* ideally should be dse_chng_bhead specific but using generic dse_running flag for now */
			if (!dse_running)
				((blk_hdr *)blk_ptr)->tn = ((blk_hdr_ptr_t)cs->new_buff)->tn = ctn;
			memcpy(blk_ptr, cs->new_buff, ((blk_hdr_ptr_t)cs->new_buff)->bsiz);
		}
		assert(SIZEOF(blk_hdr) <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
		assert((int)((blk_hdr_ptr_t)blk_ptr)->bsiz > 0);
		assert((int)((blk_hdr_ptr_t)blk_ptr)->bsiz <= csd->blk_size);
		long_blk_id = IS_64_BLK_ID(blk_ptr);
		DEBUG_ONLY(blk_id_sz = SIZEOF_BLK_ID(long_blk_id));
		if (!dollar_tlevel)
		{
			if (0 != cs->ins_off)
			{	/* reference to resolve: insert real block numbers in the buffer */
				assert(0 <= (short)cs->index);
				assert(cs - cw_set > cs->index);
				assert((SIZEOF(blk_hdr) + SIZEOF(rec_hdr)) <= cs->ins_off);
				assert((cs->ins_off + blk_id_sz) <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
				WRITE_BLK_ID(long_blk_id, cw_set[cs->index].blk, (blk_ptr + cs->ins_off));
				if (((nxt = cs + 1) < &cw_set[cw_set_depth]) && (gds_t_write_root == nxt->mode))
				{	/* If the next cse is a WRITE_ROOT, it contains a second block pointer
					 * to resolve though it operates on the current cse's block.
					 */
					assert(0 <= (short)nxt->index);
					assert(nxt - cw_set > nxt->index);
					assert(SIZEOF(blk_hdr) <= nxt->ins_off);
					assert(nxt->ins_off <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
					WRITE_BLK_ID(long_blk_id, cw_set[nxt->index].blk, blk_ptr + nxt->ins_off);
				}
			}
		} else
		{	/* TP */
			if (0 != cs->first_off)
			{	/* TP - resolve pointer references to new blocks */
				for (chain_ptr = blk_ptr + cs->first_off; ; chain_ptr += chain.next_off)
				{
					READ_OFF_CHAIN(long_blk_id, &chain, chain_ptr);
					assert(1 == chain.flag);
					assert((int)(chain_ptr - blk_ptr + chain.next_off + blk_id_sz)
							<= (int)((blk_hdr_ptr_t)blk_ptr)->bsiz);
					assert((SIZEOF(int) * 8) >= CW_INDEX_MAX_BITS);
					assert((int)chain.cw_index < sgm_info_ptr->cw_set_depth);
					tp_get_cw(sgm_info_ptr->first_cw_set, (int)chain.cw_index, &cs_ptr);
					WRITE_BLK_ID(long_blk_id, cs_ptr->blk, chain_ptr);
					if (0 == chain.next_off)
						break;
				}
			}
		}
	}
	RESET_DATA_INVALID(cr);
	CERT_BLK_IF_NEEDED(certify_all_blocks, gv_cur_region, cs, blk_ptr, gv_target);
	assert(!cr->tn || (cr->dirty > cr->flushed_dirty_tn));
	assert(cr->tn || (LATCH_SET == WRITE_LATCH_VAL(cr)));
	assert(!cr->tn || (LATCH_CLEAR < WRITE_LATCH_VAL(cr)));
	cache_state = csa->acc_meth.bg.cache_state;
	if (!cr->tn)
	{	/* stuff it on the active queue */
		assert(0 == cr->epid);
		/* Earlier revisions of this code had a kludge in place here to work around INSQTI failures (D9D06-002342).
		 * Those are now removed as the primary error causing INSQTI failures is believed to have been resolved.
		 */
		n = INSQTI((que_ent_ptr_t)&cr->state_que, (que_head_ptr_t)&cache_state->cacheq_active);
		GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_INSQTIFAIL, n, INTERLOCK_FAIL);
		if (INTERLOCK_FAIL == n)
		{
			assert(ydb_white_box_test_case_enabled);
			BG_TRACE_PRO(wcb_bg_update_lckfail1);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_bg_update_lckfail1"),
				process_id, &ctn, DB_LEN_STR(gv_cur_region));
			return cdb_sc_cacheprob;
		}
		ADD_ENT_TO_ACTIVE_QUE_CNT(cnl);
		cr->jnl_addr = cs->jnl_freeaddr;	/* update jnl_addr only if cache-record is not already in active queue */
	}
	RELEASE_BUFF_UPDATE_LOCK(cr, n, &cnl->db_latch);
	/* "n" holds the pre-release value, so check accordingly */
	assert(LATCH_CONFLICT >= n);
	assert(LATCH_CLEAR < n);	/* check that we did hold the lock before releasing it above */
	if (WRITER_BLOCKED_BY_PROC(n))
	{	/* It's off the active que, so put it back at the head to minimize the chances of blocks being "pinned" in memory.
		 * Note that this needs to be done BEFORE releasing the in_tend and in_cw_set locks as otherwise it is possible
		 * that a concurrent process in bg_update_phase1 could lock this buffer for update and incorrectly conclude that
		 * it has been locked by a writer when it has actually been locked by a process in bg_update_phase2.
		 */
		assert(0 == cr->epid);
		n = INSQHI((que_ent_ptr_t)&cr->state_que, (que_head_ptr_t)&cache_state->cacheq_active);
		GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_INSQHIFAIL, n, INTERLOCK_FAIL);
		if (INTERLOCK_FAIL == n)
		{
			assert(ydb_white_box_test_case_enabled);
			BG_TRACE_PRO(wcb_bg_update_lckfail2);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_bg_update_lckfail2"),
				process_id, &ctn, DB_LEN_STR(gv_cur_region));
			return cdb_sc_cacheprob;
		}
	}
	RESET_CR_IN_TEND_AFTER_PHASE2_COMMIT(cr, csa, csd); /* resets cr->in_tend & cr->in_cw_set (for older twin too if needed) */
	VERIFY_QUEUE_LOCK(&cache_state->cacheq_active, &cnl->db_latch, csa);
	cs->old_mode = -cs->old_mode;	/* negate it back to indicate phase2 is complete for this cse (used by secshr_db_clnup) */
	assert(0 < cs->old_mode);
	return cdb_sc_normal;
}

/* Used to prevent staleness of buffers. Start timer to call wcs_stale to do periodic flushing */
void wcs_timer_start(gd_region *reg, boolean_t io_ok)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	enum db_acc_method	acc_meth;
	int4			wtstart_errno;
	jnl_private_control	*jpc;
	uint4		buffs_per_flush, flush_target;

	assert(reg->open); /* there is no reason we know of why a region should be closed at this point */
	if (!reg->open)    /* in pro, be safe though and don't touch an already closed region */
		return;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	acc_meth = csd->acc_meth;
	/* This process can only have one flush timer per region. Overall, there can only be
	 * 2 outstanding timers per region for the entire system. Note: wcs_timers starts at -1.
	 */
	if ((FALSE == csa->timer) && (cnl->wcs_timers < (is_updproc ? 0 : 1)))
	{
		if ((dba_bg == acc_meth) ||				/* bg mode or */
		    (dba_mm == acc_meth && (0 < csd->defer_time)))	/* defer'd mm mode */
		{
			if (!exit_handler_active)
			{
				INTPTR_T		reg_parm;

				csa->canceled_flush_timer = FALSE;
				csa->timer = TRUE;
				INCR_CNT(&cnl->wcs_timers, &cnl->wc_var_lock);
				INSERT_WT_PID(csa);
				reg_parm = (UINTPTR_T)reg;
				start_timer((TID)reg,
					    (csd->flush_time * (dba_bg == acc_meth ? 1 : csd->defer_time)),
					    &wcs_stale, SIZEOF(reg_parm), (char *)&reg_parm);
				BG_TRACE_ANY(csa, stale_timer_started);
			}
			/* else: We are already in exit processing. Do not start timers as it is unsafe (YDB#679). */
		}
	}
	if (is_updproc && (cnl->wcs_timers > ((FALSE == csa->timer) ? -1 : 0)))
		return;		/* Another process has a timer, so let that do the work. */
	/* If we are being called from a timer driven routine, it is not possible to do IO at this time
	 * because the state of the machine (crit check, lseekio, etc.) is not being checked here.
	 */
	if (FALSE == io_ok)
		return;
	/* Use this opportunity to sync the db if necessary (as a result of writing an epoch record). */
	if ((dba_bg == acc_meth) && JNL_ENABLED(csd))
	{
		jpc = csa->jnl;
		if (jpc && jpc->jnl_buff->need_db_fsync && (NOJNL != jpc->channel))
			jnl_qio_start(jpc);	/* See jnl_qio_start for how it achieves the db_fsync */
	}
	/* Need to add something similar for MM here */
	/* If we are getting too full, do some i/o to clear some out.
	 * This should happen only as we are getting near the saturation point.
	 */
	/* assume defaults for flush_target and buffs_per_flush */
	flush_target = csd->flush_trigger;
	buffs_per_flush = 0;
	if ((0 != csd->epoch_taper) && (0 != cnl->wcs_active_lvl) &&
			JNL_ENABLED(csd) && (0 != cnl->jnl_file.u.inode) && csd->jnl_before_image)
	{
		EPOCH_TAPER_IF_NEEDED(csa, csd, cnl, reg, TRUE, buffs_per_flush, flush_target);
	}
	if ((flush_target  <= cnl->wcs_active_lvl) && !FROZEN_CHILLED(csa))
	{	/* Already in need of a good flush */
		BG_TRACE_PRO_ANY(csa, active_lvl_trigger);
		wtstart_errno = wcs_wtstart(reg, buffs_per_flush, NULL, NULL);
		if ((dba_mm == acc_meth) && (ERR_GBLOFLOW == wtstart_errno))
			wcs_recover(reg);
		csa->stale_defer = FALSE;		/* This took care of any pending work for this region */
	}
	return;
}

/* A timer has popped. Some buffers are stale -- start writing to the database */
void wcs_stale(TID tid, int4 hd_len, gd_region **region)
{
	boolean_t		need_new_timer;
	gd_region		*save_region;
	sgmnt_addrs		*csa, *save_csaddrs, *check_csaddrs;
	sgmnt_data_ptr_t	csd, save_csdata;
	gd_region		*reg;
	jnlpool_addrs_ptr_t	save_jnlpool;
	enum db_acc_method	acc_meth;
	node_local_ptr_t	cnl;
	jnl_private_control	*jpc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_region = gv_cur_region;		/* Certain debugging calls expect gv_cur_region to be correct */
	save_csaddrs = cs_addrs;
	save_csdata = cs_data;
	save_jnlpool = jnlpool;
	check_csaddrs = (NULL == save_region || FALSE == save_region->open) ? NULL : &FILE_INFO(save_region)->s_addrs;
		/* Save to see if we are in crit anywhere */
	reg = *region;
	assert(reg->open);
	/* Note the non-usage of TP_CHANGE_REG_IF_NEEDED macros since this routine can be timer driven. */
	TP_CHANGE_REG(reg);
	csa = cs_addrs;
	csd = cs_data;
	assert(NULL != csa);
	assert(NULL != csd);
	assert(csd == csa->hdr);
	acc_meth = csd->acc_meth;
	cnl = csa->nl;
	jpc = csa->jnl;
	assert((NULL == jpc) || (NOJNL != jpc->channel) || JNL_FILE_SWITCHED(jpc));
	/* Check if this is a condition where we don't want to start a new timer for sure */
	if (((dba_mm == acc_meth) && (csa->total_blks != csa->ti->total_blks)) /* access method is MM and file extended */
		|| (is_updproc && (cnl->wcs_timers > 0)) /* Update process need not take up the burden of flushing if
							  * there is at least one other flusher */
		|| ((NULL != jpc) && JNL_FILE_SWITCHED(jpc))) /* Journal file has been switched but we cannot open the new journal
							 * file while inside interrupt code (see wcs_clean_dbsync.c comment
							 * on jnl_write_attempt/jnl_output_sp for why) and so better to relinquish
							 * timer slot to some other process which does not have this issue.
							 */
	{	/* We aren't creating a new timer so decrement the count for this one that is now done */
		DECR_CNT(&cnl->wcs_timers, &cnl->wc_var_lock);
		REMOVE_WT_PID(csa);
		csa->timer = FALSE;		/* No timer set for this region by this process anymore */
		csa->canceled_flush_timer = TRUE;
		/* Restore region */
		if (save_region != gv_cur_region)
		{
			gv_cur_region = save_region;
			cs_addrs = save_csaddrs;
			cs_data = save_csdata;
			jnlpool = save_jnlpool;
		}
		return;
	}
	BG_TRACE_ANY(csa, stale_timer_pop);
	/* Default to need a new timer in case bypass main code because of invalid conditions */
	need_new_timer = TRUE;
	/****************************************************************************************************
	   We don't want to do expensive IO flushing if:
	   1) We are in the midst of lseek/read/write IO. This could reset an lseek.
	   2) We are aquiring crit in any of our regions.
	      Note that the function "mutex_deadlock_check" resets crit_count to 0 temporarily even though we
	      might actually be in the midst of acquiring crit. Therefore we should not interrupt mainline code
	      if we are in "mutex_deadlock_check" as otherwise it presents reentrancy issues.
	   3) We have crit in any region OR are in the middle of commit for this region even though we don't
	      hold crit (in bg_update_phase2) OR are in wcs_wtstart (potentially holding write interlock and
	      keeping another process in crit waiting). Assumption is that if region we were in was not crit, we're
	      clear. This is not strictly true in some special TP cases on the final retry if the previous retry did
	      not get far enough into the transaction to cause all regions to be locked down but this case is
	      statistically infrequent enough that we will go ahead and do the IO in crit "this one time".
	   4) We are in a "fast lock".
	   5) We are in an external call and the database is encrypted (both of which could use memory management
	      functions).
	   6) We are in an online freeze.
	   Note: Please ensure the terms in the "if" below appear in descending order based upon likelihood
	      of their being FALSE. Any terms that call functions should be at the end.
	   **************************************************************************************************/
	if (!in_mutex_deadlock_check
		&& ((NULL == check_csaddrs) || !T_IN_CRIT_OR_COMMIT_OR_WRITE(check_csaddrs))
		&& (0 == fast_lock_count)
		&& (!(TREF(in_ext_call) && csd->is_encrypted))
		&& OK_TO_INTERRUPT)
	{
		BG_TRACE_PRO_ANY(csa, stale);
		switch (acc_meth)
		{
			case dba_bg:
				if (!FROZEN_CHILLED(csa))
				{	/* Flush at least some of our cache */
					wcs_wtstart(reg, 0, NULL, NULL);
					/* If there is no dirty buffer left in the active queue, then no need for new timer */
					if (0 == csa->acc_meth.bg.cache_state->cacheq_active.fl)
						need_new_timer = FALSE;
				}
				else
				{	/* We can't flush to the file, but we can flush the journal */
					jnl_wait(reg);
				}
				break;
			case dba_mm:
				assert(!FROZEN_CHILLED(csa));
				wcs_wtstart(reg, 0, NULL, NULL);
				assert(csd == csa->hdr);
				need_new_timer = FALSE;
				break;
			default:
				break;
		}
	} else
	{
		csa->stale_defer = TRUE;
		unhandled_stale_timer_pop = TRUE;
		BG_TRACE_ANY(csa, stale_process_defer);
	}
	assert((dba_bg == acc_meth) || (0 < csd->defer_time));
	/* If "exit_handler_active" is TRUE, we are already in exit processing. Do not start timers as it is unsafe (YDB#679). */
	if (!exit_handler_active && (need_new_timer && (0 >= cnl->wcs_timers)))
	{
		start_timer((TID)reg, (csd->flush_time * (dba_bg == acc_meth ? 1 : csd->defer_time)),
			    &wcs_stale, SIZEOF(region), (char *)region);
		BG_TRACE_ANY(csa, stale_timer_started);
	} else
	{	/* We aren't creating a new timer so decrement the count for this one that is now done */
		DECR_CNT(&cnl->wcs_timers, &cnl->wc_var_lock);
		REMOVE_WT_PID(csa);
		csa->timer = FALSE;		/* No timer set for this region by this process anymore */
		/* Since this is a case where we know for sure no dirty buffers remain (if "exit_handler_active" is FALSE),
		 * there is no need of the flush timer anymore, so "csa->canceled_flush_timer" can be safely set to FALSE.
		 */
		assert(!csa->canceled_flush_timer);	/* should have been reset when "csa->timer = TRUE" happened */
		csa->canceled_flush_timer = FALSE;	/* be safe in pro just in case */
	}
	/* To restore to former glory, don't use TP_CHANGE_REG, 'coz we might mistakenly set cs_addrs and cs_data to NULL
	 * if the region we are restoring has been closed. Don't use tp_change_reg 'coz we might be ripping out the structures
	 * needed in tp_change_reg in gv_rundown.
	 */
	gv_cur_region = save_region;
	cs_addrs = save_csaddrs;
	cs_data = save_csdata;
	jnlpool = save_jnlpool;
	return;
}

/* --------------------------------------------------------------------------------------------
 * This code is very similar to the code in gvcst_put for the non-block-split case. Any changes
 * in either place should be reflected in the other.
 * --------------------------------------------------------------------------------------------
 */
enum cdb_sc	t_recompute_upd_array(srch_blk_status *bh, struct cw_set_element_struct *cse, cache_rec_ptr_t cr)
{
	blk_hdr_ptr_t		old_block;
	blk_segment		*bs1, *bs_ptr;
	boolean_t		new_rec = FALSE;
	/*cache_rec_ptr_t		cr; calculate instead of pass in ?*/
	char			*va;
	enum cdb_sc		status;
	gv_key			*pKey = NULL;
	gv_namehead		*gvt;
	int4			blk_fill_size, blk_seg_cnt, blk_size, cur_blk_size, delta, n, new_rec_size, next_rec_shrink,
	rec_cmpc, target_key_size;
	int			tmp_cmpc;
	key_cum_value		*kv, *kvhead;			/* kvhead tp only */
	mstr			value;
	DEBUG_ONLY(block_ref	chain1;)		/* tp only */
	rec_hdr_ptr_t		curr_rec_hdr, next_rec_hdr, rp;
	sgmnt_addrs		*csa;
	sm_uc_ptr_t		cp1, buffaddr;
	srch_blk_status		*t1;
	uint4			segment_update_array_size;	/* tp only */
	unsigned int		bsiz;
	unsigned short		rec_size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	BG_TRACE_PRO_ANY(csa, recompute_upd_array_calls);
	assert(csa->now_crit && !cse->level);	/* better be in crit and be a leaf-level block */
	assert((gds_t_write == cse->mode) && (GDS_WRITE_PLAIN == cse->write_type));
	assert(!cse->first_off);	/* does this apply to non-TP ??? */
	assert(dollar_tlevel ? (NULL != sgm_info_ptr) : (NULL == cse->new_buff));
	if (dba_bg == csa->hdr->acc_meth)
	{	/* For BG method, modify history with uptodate cache-record, buffer and cycle information.
		* This is necessary in case history contains an older twin cr or a cr which has since been recycled
		*/
		if (dollar_tlevel)
		{
			cse->first_copy = TRUE;
			assert(cse->blk_target);
			cr = db_csh_get(bh->blk_num);
		}
		assert(CR_NOTVALID != (sm_long_t)cr);
		if (NULL == cr || CR_NOTVALID == (sm_long_t)cr || 0 <= cr->read_in_progress)
		{
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_rip);
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_lostcr;
		}
		if (cr->in_tend)
		{	/* Possible if this cache-record is being modified concurrently by another process in bg_update_phase2.
			* In this case, we cannot determine if recomputation is possible. Have to restart.
			*/
			assert(CDB_STAGNATE > t_tries);
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_in_tend);
			return cdb_sc_blkmod;
		}
		if (!dollar_tlevel)
		{	/* Caller is t_end(). It has already done the "cycle" check (i.e. cr->cycle == bh->cycle is TRUE) */
			if (bh->cr != cr)
			{	/* History is not in sync with cr. Possible in case cr is a newer twin and history points
				 * to older twin. Assert accordingly. And update history to be in sync with newer twin in
				 * the hope that the recomputation will succeed and the history validation in t_end()
				 * (caller) will otherwise succeed.
				 */
				assert(csa->hdr->asyncio);
				assert(0 != cr->twin);
				assert(0 != cr->bt_index);
				bh->cr = cr;
				bh->cycle = cr->cycle;
				bh->tn = csa->ti->curr_tn;
				assert(bh->buffaddr != (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr));
				/* old_block needs to be repointed to the NEW buffer but the fact that this block was free
				 * does not change in this entire function. So cse->blk_prior_state's free_status can stay as it is.
				 */
				cse->old_block = bh->buffaddr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
			} else
			{
				assert(bh->cycle == cr->cycle);	/* t_end() (caller) would have restarted otherwise */
				assert(bh->buffaddr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr));
			}
			/* assert that cse->old_block is indeed pointing to the buffer that the cache-record is pointing to */
			assert(cse->old_block == (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr));
		} else
		{	/* Caller is tp_tend(). In this case, the "cycle" check is not already done (will be done only
			 * after this call). So if "bh->cr == cr", it is not guaranteed that "bh->cycle" and "bh->buffaddr"
			 * are the same as "cr->cycle" and "cr->buffaddr" like was the case when "t_end()" was the caller
			 * (in the "if" block above). So fix all those fields explicitly for now.
			 *
			 * NOTE: One might hope that "tp_tend()" can also be fixed to be just like "t_end()" at which point,
			 * the below code can be removed and the "if" block above can be made common to both t_end and tp_tend.
			 * But that is unlikely because "tp_tend()" validation skips the "cycle" check for blocks that were
			 * only part of the read-set. Only blocks that are part of the read-write set go through the "cycle"
			 * check. Therefore, this else block will stay on.
			 */
			bh->cr = cr;
			bh->cycle = cr->cycle;
			cse->old_block = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
			bh->buffaddr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
		}
	}
	buffaddr = bh->buffaddr;
	if ((NULL != cse->old_block) && (cse->ondsk_blkver < ((blk_hdr_ptr_t)cse->old_block)->bver))
	{	/* Adjust the ondsk_blkver as needed */
		assert(!cs_data->fully_upgraded);
		assert(GDSV7m == ((blk_hdr_ptr_t)cse->old_block)->bver);
		cse->ondsk_blkver = ((blk_hdr_ptr_t)cse->old_block)->bver;
	}
	assert(NULL != cse->recompute_list_head);
	for (kvhead = kv = cse->recompute_list_head; (NULL != kv); kv = kv->next)
	{
		pKey = (gv_key *)&kv->keybuf.nobase;
		value = kv->value;
		if (!dollar_tlevel)
		{
			assert(!IS_IN_STRINGPOOL(value.addr, value.len));
			s2pool(&value);
		}
		target_key_size = pKey->end + 1;
		if (kvhead != kv)
		{
			assert(dollar_tlevel);
			assert(FALSE == cse->done);
			assert(0 == cse->reference_cnt);
			assert(0 == cse->ins_off);		/* because leaf-level block */
			assert(0 == cse->level);
			assert(0 == cse->index);
			assert(FALSE == cse->forward_process);	/* because no kills should have taken place in this block */
			gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, 0);
			bh->buffaddr = buffaddr = cse->new_buff;
		}
		if (cdb_sc_normal != (status = gvcst_search_blk(pKey, bh)))
		{
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_search_blk);
			return status;
		}
		cur_blk_size = ((blk_hdr_ptr_t)buffaddr)->bsiz;
		new_rec = (target_key_size != bh->curr_rec.match);
		rp = (rec_hdr_ptr_t)(buffaddr + bh->curr_rec.offset);
		if (bh->curr_rec.offset == cur_blk_size)
		{
			if (FALSE == new_rec)
			{
				assert(CDB_STAGNATE > t_tries);
				BG_TRACE_PRO_ANY(csa, recompute_upd_array_new_rec);
				return cdb_sc_mkblk;
			}
			rec_cmpc = 0;
			rec_size = 0;
		} else
		{
			GET_USHORT(rec_size, &rp->rsiz);
			rec_cmpc = EVAL_CMPC(rp);
			if ((sm_uc_ptr_t)rp + rec_size > (sm_uc_ptr_t)buffaddr + cur_blk_size)
			{
				assert(CDB_STAGNATE > t_tries);
				BG_TRACE_PRO_ANY(csa, recompute_upd_array_rec_size);
				return cdb_sc_mkblk;
			}
		}
		if (new_rec)
		{
			new_rec_size = SIZEOF(rec_hdr) + target_key_size - bh->prev_rec.match + value.len;
			if (cur_blk_size <= (int)bh->curr_rec.offset)
				next_rec_shrink = 0;
			else
				next_rec_shrink = bh->curr_rec.match - rec_cmpc;
			delta = new_rec_size - next_rec_shrink;
		} else
		{
			if (rec_cmpc != bh->prev_rec.match)
			{
				assert(CDB_STAGNATE > t_tries);
				BG_TRACE_PRO_ANY(csa, recompute_upd_array_rec_cmpc);
				return cdb_sc_mkblk;
			}
			new_rec_size = SIZEOF(rec_hdr) + (target_key_size - rec_cmpc) + value.len;
			delta = new_rec_size - rec_size;
			next_rec_shrink = 0;
		}
		blk_size = cs_data->blk_size;	/* "blk_size" is also used by the BLK_FINI macro below */
		blk_fill_size = (blk_size * gv_fillfactor) / 100 -
			(cse->level ? cs_data->i_reserved_bytes : cs_data->reserved_bytes);
		if ((cur_blk_size + delta) > blk_fill_size)
		{
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_blk_split);
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_blksplit;
		}
		if (dollar_tlevel)
		{
			assertpro(0 == rc_set_fragment);
			DEBUG_ONLY(chain1.id = bh->blk_num);
			assert(0 == chain1.chain.flag);
			segment_update_array_size = UA_NON_BM_SIZE(cs_data);
			ENSURE_UPDATE_ARRAY_SPACE(segment_update_array_size);
		} else
		{	/* non-TP */
			if (0 != rc_set_fragment)
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_mkblk;	/* let gvcst_put do the recomputation out of crit in case of rc_set */
			}
			assert(NULL == kv->next);
			assert(NULL != update_array);
			assert(NULL != update_array_ptr);
			assert(0 != update_array_size);
			assert(update_array + update_array_size >= update_array_ptr);
			assert(1 == cw_set_depth);
			/* because cw_set_depth is guaranteed to be 1 (by the above assert), we can be sure that the only update
			 * array space we have used is for the current (and only) cw_set_element "cse" and hence can reuse space
			 * by resetting update_array_ptr
			 */
			assert(ROUND_UP2((INTPTR_T)update_array, UPDATE_ELEMENT_ALIGN_SIZE) == (INTPTR_T)cse->upd_addr.ptr);
			RESET_UPDATE_ARRAY; /* no CHECK_AND_RESET_UPDATE_ARRAY because we are resetting an active update array */
		}
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, buffaddr + SIZEOF(blk_hdr), bh->curr_rec.offset - SIZEOF(blk_hdr));
		BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
		curr_rec_hdr->rsiz = new_rec_size;
		SET_CMPC(curr_rec_hdr, bh->prev_rec.match);
		BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
		BLK_ADDR(cp1, target_key_size - bh->prev_rec.match, unsigned char);
		memcpy(cp1, pKey->base + bh->prev_rec.match, target_key_size - bh->prev_rec.match);
		BLK_SEG(bs_ptr, cp1, target_key_size - bh->prev_rec.match);
		if (0 != value.len)
		{
			BLK_ADDR(va, value.len, char);
			memcpy(va, value.addr, value.len);
			BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
		}
		if (!new_rec)
			rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
		n = (int)(cur_blk_size - ((sm_uc_ptr_t)rp - buffaddr));
		if (n > 0)
		{
			if (new_rec)
			{
				BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				next_rec_hdr->rsiz = rec_size - next_rec_shrink;
				SET_CMPC(next_rec_hdr, bh->curr_rec.match);
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
				next_rec_shrink += SIZEOF(rec_hdr);
			}
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp + next_rec_shrink, n - next_rec_shrink);
		}
		if (0 == BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_blk_fini);
			return cdb_sc_mkblk;
		}
		cse->upd_addr.blk = bs1;
		cse->done = FALSE;
		assert(NULL != gv_target);
	}
	BG_TRACE_PRO_ANY(csa, recompute_upd_array_paidoff);
	/* Update bh->tn to reflect the fact that it is uptodate as of the current database transaction.
	 * Not doing so could actually cause unnecessary restarts.
	 */
	bh->tn = csa->hdr->trans_hist.curr_tn;
	/* If block in this history element is the same as gv_target's leaf block and it has a non-zero clue, update it */
	gvt = bh->blk_target;
	assert(!bh->level);	/* this is why it is safe to access 0th array index in the next line */
	t1 = gvt->hist.h;
	if (gvt->clue.end && (t1->blk_num == bh->blk_num))
	{
		*t1 = *bh;
		/* Update clue to reflect last key in recompute list. No need to update gvt->first_rec and gvt->last_rec
		 * as they are guaranteed to be the same as what it was when the clue was filled in by gvcst_search (if
		 * they are different, an index block would have changed which means we would restart this transaction
		 * anyways and the clue would be reset to 0).
		 */
		assert(NULL != pKey);
		assert(!TREF(expand_prev_key));	/* this ensures it is safe to use EXPAND_PREV_KEY_FALSE below */
		COPY_CURR_AND_PREV_KEY_TO_GVTARGET_CLUE(gvt, pKey, EXPAND_PREV_KEY_FALSE);
		if (new_rec)
			t1->curr_rec.match = gvt->clue.end + 1;	/* Keep srch_hist and clue in sync for NEXT gvcst_search */
		/* Now that the clue is known to be non-zero, we have the potential for the first_rec part of it to be
		 * unreliable. Reset it to be safe. See comment in similar section in tp_hist for details on why.
		 */
		GVT_CLUE_INVALIDATE_FIRST_REC(gvt);
	}
	if (dollar_tlevel)
	{
		/* At this point, cse->new_buff could be non-NULL either because the same variable was being updated multiple times
		 * inside of the TP transaction or because cse->recompute_list_head contained more than one variable (in which case
		 * cse->new_buff will be set by the invocation of gvcst_blk_build (above) for the second element in the list. In
		 * either case, the final update-array contents rely on the shared memory buffer (in case of BG access method) and
		 * not on cse->new_buff. Therefore we need to PIN the corresponding cache-record in tp_tend. So reset cse->new_buff.
		 */
		cse->new_buff = NULL;
	}
	if (!WAS_FREE(cse->blk_prior_state) && (NULL != cse->old_block) && JNL_ENABLED(csa) && csa->jnl_before_image)
	{	/* Recompute checksums in case necessary */
		old_block = (blk_hdr_ptr_t)cse->old_block;
		if (old_block->tn < csa->jnl->jnl_buff->epoch_tn)
		{
			bsiz = old_block->bsiz;
			/* See comment before similar check in "gvincr_recompute_upd_array" for why this check is needed */
			if (bsiz > csa->hdr->blk_size)
			{	/* This is a restartable condition. Restart */
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_mkblk;
			}
			cse->blk_checksum = jnl_get_checksum(old_block, csa, bsiz);
		} else
			cse->blk_checksum = 0;
	}
	return cdb_sc_normal;
}
