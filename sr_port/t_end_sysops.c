/****************************************************************
 *								*
 *	Copyright 2007, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#if defined(VMS)
#include <iodef.h>
#include <psldef.h>
#include <rms.h>
#include <ssdef.h>

#elif defined(UNIX)
#include "gtm_stdlib.h"		/* for GETENV */
#include "gtm_ipc.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"		/*  for strlen() in RTS_ERROR_TEXT macro */

#include <sys/mman.h>
#include <errno.h>
#endif

#include "gtm_facility.h"
#include "gdsroot.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbml.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "cdb_sc.h"
#include "copy.h"
#include "filestruct.h"
#include "interlock.h"
#include "jnl.h"
#include "iosp.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
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

#if defined(VMS)
#include "efn.h"
#include "timers.h"
#include "ast.h"
#include "dbfilop.h"
#include "iosb_disk.h"

#elif defined(UNIX)
#include "aswp.h"
#include "gtmio.h"
#include "io.h"			/* for gtmsecshr.h */
#include "performcaslatchcheck.h"
#include "gtmmsg.h"
#include "error.h"		/* for gtm_fork_n_core() prototype */
#include "util.h"
#include "caller_id.h"
#include "add_inter.h"
#include "rel_quant.h"
#include "wcs_write_in_progress_wait.h"
#endif

/* Include prototypes */
#include "send_msg.h"
#include "bit_set.h"
#include "bit_clear.h"
#include "mupipbckup.h"
#include "gvcst_blk_build.h"
#include "gvcst_map_build.h"
#include "relqop.h"
#include "is_proc_alive.h"
#include "wcs_sleep.h"
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
#include "shmpool.h"	/* Needed for the shmpool structures */
#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif

error_def(ERR_DBFILERR);
error_def(ERR_FREEBLKSLOW);
error_def(ERR_GBLOFLOW);
UNIX_ONLY(error_def(ERR_TEXT);)
error_def(ERR_WCBLOCKED);

/* Set the cr->ondsk_blkver to the csd->desired_db_format */
#define SET_ONDSK_BLKVER(cr, csd, ctn)											\
{															\
	/* Note that even though the corresponding blks_to_uprd adjustment for this cache-record happened in phase1 	\
	 * while holding crit, we are guaranteed that csd->desired_db_format did not change since then because the 	\
	 * function that changes this ("desired_db_format_set") waits for all phase2 commits to	complete before 	\
	 * changing the format. Before resetting cr->ondsk_blkver, ensure db_format in file header did not change in 	\
	 * between phase1 (inside of crit) and phase2 (outside of crit). This is needed to ensure the correctness of 	\
	 * the blks_to_upgrd counter.											\
	 */														\
	assert((ctn > csd->desired_db_format_tn) || ((ctn == csd->desired_db_format_tn) && (1 == ctn)));		\
	cr->ondsk_blkver = csd->desired_db_format;									\
}

#if defined(UNIX)
#define MAX_CYCLES	2
NOPIO_ONLY(GBLREF boolean_t	*lseekIoInProgress_flags;)
void	wcs_stale(TID tid, int4 hd_len, gd_region **region);

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

#elif defined(VMS)
GBLREF	short			astq_dyn_avail;
void	wcs_stale(gd_region *reg);
#endif

GBLREF	volatile int4		crit_count;
GBLREF	volatile boolean_t	in_mutex_deadlock_check;
GBLREF  volatile int4		gtmMallocDepth;
GBLREF	boolean_t		certify_all_blocks;
GBLREF	uint4			process_id;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	cache_rec_ptr_t		cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF	unsigned int		cr_array_index;
GBLREF	uint4			dollar_tlevel;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	boolean_t		block_saved;
GBLREF	boolean_t		write_after_image;
GBLREF	boolean_t		dse_running;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		mu_reorg_upgrd_dwngrd_in_prog;	/* TRUE if MUPIP REORG UPGRADE/DOWNGRADE is in progress */
GBLREF	boolean_t		mu_reorg_nosafejnl;		/* TRUE if NOSAFEJNL explicitly specified */
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	cw_set_element		cw_set[];
GBLREF	unsigned char		cw_set_depth;
GBLREF	volatile int4		fast_lock_count;
GBLREF	boolean_t		unhandled_stale_timer_pop;
GBLREF	void			(*wcs_stale_fptr)();
#ifdef UNIX
GBLREF	jnl_gbls_t		jgbl;
#endif

void fileheader_sync(gd_region *reg)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	th_index_ptr_t		cti;
	int4			high_blk;
#	if defined(UNIX)
	size_t			flush_len, sync_size, rounded_flush_len;
	int4			save_errno;
	unix_db_info		*udi;
#	elif defined(VMS)
	file_control		*fc;
	int4			flush_len;
	vms_gds_info		*udi;
#	endif

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert(csa->now_crit);	/* only way high water mark code works is if in crit */
				/* Adding lock code to it would remove this restriction */
	assert(!reg->read_only);
	assert(0 == memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1));
	cnl = csa->nl;
	gvstats_rec_cnl2csd(csa);	/* Periodically transfer statistics from database shared-memory to file-header */
	high_blk = cnl->highest_lbm_blk_changed;
	cnl->highest_lbm_blk_changed = -1;			/* Reset to initial value */
	flush_len = SGMNT_HDR_LEN;
	if (0 <= high_blk)					/* If not negative, flush at least one master map block */
		flush_len += ((high_blk / csd->bplmap / DISK_BLOCK_SIZE / BITS_PER_UCHAR) + 1) * DISK_BLOCK_SIZE;
	if (csa->do_fullblockwrites)
	{	/* round flush_len up to full block length. This is safe since we know that
		 * fullblockwrite_len is a factor of the starting data block - see gvcst_init_sysops.c
		 */
		flush_len = ROUND_UP(flush_len, csa->fullblockwrite_len);
	}
	assert(flush_len <= (csd->start_vbn - 1) * DISK_BLOCK_SIZE);	/* assert that we never overwrite GDS block 0's offset */
	assert(flush_len <= SIZEOF_FILE_HDR(csd));	/* assert that we never go past the mastermap end */
#	if defined(VMS)
	fc = reg->dyn.addr->file_cntl;
	fc->op = FC_WRITE;
	fc->op_buff = (char *)csd;
	fc->op_len = ROUND_UP(flush_len, DISK_BLOCK_SIZE);
	fc->op_pos = 1;
	dbfilop(fc);
#	elif defined(UNIX)
	DB_LSEEKWRITE(csa, udi->fn, udi->fd, 0, (sm_uc_ptr_t)csd, flush_len, save_errno);
	if (0 != save_errno)
	{
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("Error during FileHeader Flush"), save_errno);
	}
#	endif
	return;
}

/* update a bitmap */
void	bm_update(cw_set_element *cs, sm_uc_ptr_t lclmap, boolean_t is_mm)
{
	int4				bml_full, total_blks, bplmap;
	boolean_t			change_bmm;
	block_id			blkid;
	sgmnt_addrs			*csa;
	sgmnt_data_ptr_t		csd;
	node_local_ptr_t		cnl;
	th_index_ptr_t			cti;
	int4				reference_cnt;

	VMS_ONLY(
		unsigned char		*mastermap[2];
		io_status_block_disk	iosb;
		int4			status;
	)

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
	assert(0 <= (int)(cti->free_blocks - reference_cnt));
	cti->free_blocks -= reference_cnt;
	change_bmm = FALSE;
	/* assert that cs->reference_cnt is 0 if we are in MUPIP REORG UPGRADE/DOWNGRADE */
	assert(!mu_reorg_upgrd_dwngrd_in_prog || (0 == reference_cnt));
	/* assert that if cs->reference_cnt is 0, then we are in MUPIP REORG UPGRADE/DOWNGRADE or DSE MAPS or DSE CHANGE -BHEAD
	 * or MUPIP REORG -TRUNCATE */
	assert(mu_reorg_upgrd_dwngrd_in_prog || dse_running || (0 != reference_cnt)
		GTM_TRUNCATE_ONLY(|| (NULL != csa->nl && process_id == csa->nl->trunc_pid)));
	if (0 < reference_cnt)
	{	/* Blocks were allocated in this bitmap. Check if local bitmap became full as a result. If so update mastermap. */
		bml_full = bml_find_free(0, (SIZEOF(blk_hdr) + (is_mm ? lclmap : ((sm_uc_ptr_t)GDS_REL2ABS(lclmap)))), total_blks);
		if (NO_FREE_SPACE == bml_full)
		{
			bit_clear(blkid / bplmap, MM_ADDR(csd));
			change_bmm = TRUE;
			if ((0 == csd->extension_size)	/* no extension and less than a bit map or less than 1/32 (3.125%) */
				&& ((BLKS_PER_LMAP > cti->free_blocks) || ((cti->total_blks >> 5) > cti->free_blocks)))
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_FREEBLKSLOW, 4, cti->free_blocks, cti->total_blks,
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
				|| (2 == cw_set_depth) && (gds_t_busy2free == (cs-1)->old_mode));
			if (0 != inctn_detail.blknum_struct.blknum)
				DECR_BLKS_TO_UPGRD(csa, csd, 1);
		}
	}
	/* else cs->reference_cnt is 0, this means no free/busy state change in non-bitmap blocks, hence no mastermap change */
	if (change_bmm)
	{	/* The following works while all uses of these fields are in crit */
		cnl = csa->nl;
		if (blkid > cnl->highest_lbm_blk_changed)
			cnl->highest_lbm_blk_changed = blkid;	    /* Retain high-water mark */
		VMS_ONLY(
			/* It would be better to remove this VMS-only logic and instead use the
			 * cnl->highest_lbm_blk_changed approach that Unix uses. -- nars - 2007/10/22.
			 */
			if (is_mm)
			{
				mastermap[0] = MM_ADDR(csd)
						+ ((blkid / bplmap / BITS_PER_UCHAR / DISK_BLOCK_SIZE) * DISK_BLOCK_SIZE);
				mastermap[1] = mastermap[0] + DISK_BLOCK_SIZE - 1;
				if (SS$_NORMAL == sys$updsec(mastermap, NULL, PSL$C_USER, 0, efn_immed_wait, &iosb, NULL, 0))
				{
					status = sys$synch(efn_immed_wait, &iosb);
					if (SS$_NORMAL == status)
						status = iosb.cond;
					assert(SS$_NORMAL == status);
				} else
					assert(FALSE);
			} else
			{
				assert(dba_bg == csd->acc_meth);
				cti->mm_tn++;
			}
		)
	}
	return;
}

enum cdb_sc	mm_update(cw_set_element *cs, trans_num ctn, trans_num effective_tn, sgm_info *si)
{
	block_id		blkid;
	cw_set_element		*cs_ptr, *nxt;
	off_chain		chain;
	sm_uc_ptr_t		chain_ptr, db_addr[2];
#	ifdef GTM_SNAPSHOT
	boolean_t 		write_to_snapshot_file;
	snapshot_context_ptr_t	lcl_ss_ctx;
#	endif
#	if defined(VMS)
	unsigned int		status;
	io_status_block_disk	iosb;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(cs_addrs->now_crit);
	assert((gds_t_committed > cs->mode) && (gds_t_noop < cs->mode));
	INCR_DB_CSH_COUNTER(cs_addrs, n_bgmm_updates, 1);
	blkid = cs->blk;
	assert((0 <= blkid) && (blkid < cs_addrs->ti->total_blks));
	db_addr[0] = MM_BASE_ADDR(cs_addrs) + (sm_off_t)cs_data->blk_size * (blkid);
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
			si->backup_block_saved = TRUE;
	}
#	ifdef GTM_SNAPSHOT
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
#	endif
	if (gds_t_writemap == cs->mode)
	{
		assert(0 == (blkid & (BLKS_PER_LMAP - 1)));
		if (FALSE == cs->done)
			gvcst_map_build((uint4 *)cs->upd_addr, db_addr[0], cs, effective_tn);
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
		if (!dollar_tlevel)
		{
			if (0 != cs->ins_off)
			{	/* reference to resolve: insert real block numbers in the buffer */
				assert(0 <= (short)cs->index);
				assert(&cw_set[cs->index] < cs);
				assert((SIZEOF(blk_hdr) + SIZEOF(rec_hdr)) <= cs->ins_off);
				assert((cs->ins_off + SIZEOF(block_id)) <= ((blk_hdr_ptr_t)db_addr[0])->bsiz);
				PUT_LONG(db_addr[0] + cs->ins_off, cw_set[cs->index].blk);
				if (((nxt = cs + 1) < &cw_set[cw_set_depth]) && (gds_t_write_root == nxt->mode))
				{	/* If the next cse is a WRITE_ROOT, it contains a second block pointer
					 * to resolve though it operates on the current cse's block.
					 */
					assert(0 <= (short)nxt->index);
					assert(&cw_set[nxt->index] < nxt);
					assert((SIZEOF(blk_hdr) + SIZEOF(rec_hdr)) <= nxt->ins_off);
					assert((nxt->ins_off + SIZEOF(block_id)) <= ((blk_hdr_ptr_t)db_addr[0])->bsiz);
					PUT_LONG(db_addr[0] + nxt->ins_off, cw_set[nxt->index].blk);
				}
			}
		} else
		{	/* TP */
			if (0 != cs->first_off)
			{	/* TP resolve pointer references to new blocks */
				for (chain_ptr = db_addr[0] + cs->first_off; ; chain_ptr += chain.next_off)
				{
					GET_LONGP(&chain, chain_ptr);
					assert(1 == chain.flag);
					assert((int)(chain_ptr - db_addr[0] + chain.next_off)
							<= (int)(((blk_hdr_ptr_t)db_addr[0])->bsiz));
					assert((int)chain.cw_index < sgm_info_ptr->cw_set_depth);
					tp_get_cw(si->first_cw_set, chain.cw_index, &cs_ptr);
					PUT_LONG(chain_ptr, cs_ptr->blk);
					if (0 == chain.next_off)
						break;
				}
			}
		}	/* TP */
	}	/* not a map */
	CERT_BLK_IF_NEEDED(certify_all_blocks, gv_cur_region, cs, db_addr[0], gv_target);
	if (0 == cs_data->defer_time)
	{
#		if defined(VMS)
		db_addr[1] = db_addr[0] + cs_data->blk_size - 1;
		status = sys$updsec(db_addr, NULL, PSL$C_USER, 0, efn_immed_wait, &iosb, NULL, 0);
		if (SS$_NORMAL == status)
		{
			status = sys$synch(efn_immed_wait, &iosb);
			if (SS$_NORMAL == status)
				status = iosb.cond;
		}
		if (SS$_NORMAL != status)
		{
			assert(FALSE);
			if (SS$_NOTMODIFIED != status)		/* don't expect notmodified, but no harm to go on */
				return cdb_sc_comfail;
		}
#		endif
	}
	return cdb_sc_normal;
}

/* update buffered global database */
enum cdb_sc	bg_update(cw_set_element *cs, trans_num ctn, trans_num effective_tn, sgm_info *si)
{
	enum cdb_sc		status;

	cs->old_mode = cs->mode;
	status = bg_update_phase1(cs, ctn, si);
	if (cdb_sc_normal == status)
		status = bg_update_phase2(cs, ctn, effective_tn, si);
	return status;
}

enum cdb_sc	bg_update_phase1(cw_set_element *cs, trans_num ctn, sgm_info *si)
{
	int4			n;
	uint4			lcnt;
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr, save_cr;
	boolean_t		read_finished, wait_for_rip, write_finished, intend_finished;
	boolean_t		read_before_image;
	block_id		blkid;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t        cnl;
	enum gds_t_mode		mode;
	enum db_ver		desired_db_format;
	trans_num		dirty_tn;
#	if defined(VMS)
	unsigned int		status;
	cache_rec_ptr_t		cr1, backup_cr;
	sm_uc_ptr_t		blk_ptr, backup_blk_ptr;
	sm_off_t		backup_cr_off;
	uint4			in_cw_set;
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
	assertpro((0 <= blkid) && (blkid < csa->ti->total_blks));
	INCR_DB_CSH_COUNTER(csa, n_bgmm_updates, 1);
	bt = bt_put(gv_cur_region, blkid);
#ifdef UNIX
	GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_BTPUTNULL, bt, NULL);
	GTM_WHITE_BOX_TEST(WBTEST_ANTIFREEZE_DBDANGER, bt, NULL);
#endif
	if (NULL == bt)
	{
		assert(gtm_white_box_test_case_enabled);
		return cdb_sc_cacheprob;
	}
	if (cs->write_type & GDS_WRITE_KILLTN)
		bt->killtn = ctn;
	cr = (cache_rec_ptr_t)(INTPTR_T)bt->cache_index;
	DEBUG_ONLY(read_before_image =
			((JNL_ENABLED(csa) && csa->jnl_before_image) || csa->backup_in_prog || SNAPSHOTS_IN_PROG(csa));)
	if ((cache_rec_ptr_t)CR_NOTVALID == cr)
	{	/* no cache record associated with the bt_rec */
		cr = db_csh_get(blkid);
		GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_DBCSHGET_INVALID, cr, (cache_rec_ptr_t)CR_NOTVALID);
		if (NULL == cr)
		{	/* no cache_rec associated with the block */
			assert(((gds_t_acquired == mode) && (!read_before_image || (NULL == cs->old_block)))
					|| (gds_t_acquired != mode) && (NULL != cs->new_buff));
			INCR_DB_CSH_COUNTER(csa, n_bg_update_creates, 1);
			cr = db_csh_getn(blkid);
			DEBUG_ONLY(
				save_cr = NULL;
				if (gtm_white_box_test_case_enabled)
					save_cr = cr;	/* save cr for r_epid cleanup before setting it to INVALID */
			)
			GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_DBCSHGETN_INVALID, cr, (cache_rec_ptr_t)CR_NOTVALID);
			if ((cache_rec_ptr_t)CR_NOTVALID == cr)
			{
				assert(gtm_white_box_test_case_enabled);
				DEBUG_ONLY(
					if (NULL != save_cr)
					{	/* release the r_epid lock on the valid cache-record returned from db_csh_getn */
						assert(save_cr->r_epid == process_id);
						save_cr->r_epid = 0;
						assert(0 == save_cr->read_in_progress);
						RELEASE_BUFF_READ_LOCK(save_cr);
					}
				)
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
			assert(gtm_white_box_test_case_enabled);
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
					|| (gds_t_acquired != mode) && (NULL != cs->new_buff));
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
		cr->in_tend = process_id;		/* in_tend should be set before the semaphore (and data_invalid) */
		assert(0 == cr->dirty);
		/* Even though the buffer is not in the active queue and we are in crit, it is possible in Unix
		 * for the cache-record to have the write interlock still set. This is because in wcs_wtstart
		 * csr->dirty is reset to 0 before it releases the write interlock on the buffer. Because all
		 * routines (bt_put, db_csh_getn and wcs_get_space) wait only for cr->dirty to become 0 before
		 * considering the buffer ready for reuse, it is possible to have the write interlock set at this
		 * point with a concurrent wcs_wtstart almost ready to release the interlock. In this case wait.
		 * Hence we cannot call LOCK_NEW_BUFF_FOR_UPDATE directly. In VMS this is not an issue since
		 * it is wcs_wtfini (which runs in crit) that clears the write interlock.
		 */
		VMS_ONLY(
			assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr));
			LOCK_NEW_BUFF_FOR_UPDATE(cr);		/* not on the active queue and this process is crit */
		)
#		ifdef UNIX
		/* Since the only case where the write interlock is not clear in Unix is a two-instruction window
		 * (described in the above comment), we dont expect the lock-not-clear situation to be frequent.
		 * Hence, for performance reasons we do the check before invoking the wcs_write_in_progress_wait function
		 * (instead of moving the if check into the function which would mean an unconditional function call).
		 */
		if (LATCH_CLEAR != WRITE_LATCH_VAL(cr))
		{
			write_finished = wcs_write_in_progress_wait(cnl, cr, WBTEST_BG_UPDATE_DIRTYSTUCK1);
			if (!write_finished)
			{
				assert(gtm_white_box_test_case_enabled);
				BG_TRACE_PRO(wcb_t_end_sysops_dirtystuck1);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED,
						6, LEN_AND_LIT("wcb_t_end_sysops_dirtystuck1"), process_id, &ctn,
						DB_LEN_STR(gv_cur_region));
				return cdb_sc_cacheprob;
			}
		} else
			LOCK_NEW_BUFF_FOR_UPDATE(cr);	/* writer has released interlock and this process is crit */
#		endif
		assert(LATCH_SET <= WRITE_LATCH_VAL(cr));
		BG_TRACE(new_buff);
		cr->bt_index = GDS_ABS2REL(bt);
		VMS_ONLY(cr->backup_cr_off = (sm_off_t)0;)
		bt->cache_index = (int4)GDS_ABS2REL(cr);
	} else	/* end of if else on cr NOTVALID */
	{
		cr = (cache_rec_ptr_t)GDS_REL2ABS(cr);
		assert(0 != cr->bt_index);
		assert(CR_BLKEMPTY != cr->blk);
		assert(blkid == cr->blk);
		if (cr->in_tend)
		{	/* Wait for another process in bg_update_phase2 to stop overlaying the buffer (possible in case of)
			 *	a) reuse of a killed block that's still in the cache
			 *	b) the buffer has already been constructed in private memory (cse->new_buff is non-NULL)
			 */
			assert(process_id != cr->in_tend);
			assert(((gds_t_acquired == mode) && (!read_before_image || (NULL == cs->old_block)))
					|| (gds_t_acquired != mode) && (NULL != cs->new_buff));
			intend_finished = wcs_phase2_commit_wait(csa, cr);
			GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_INTENDSTUCK, intend_finished, 0);
			if (!intend_finished)
			{
				assert(gtm_white_box_test_case_enabled);
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
		 *	Unix logic waits for an active writer to finish flushing.
		 *	VMS  logic creates a twin and dumps the update on that buffer instead of waiting.
		 */
#		if defined(UNIX)
		LOCK_BUFF_FOR_UPDATE(cr, n, &cnl->db_latch);
		if (!OWN_BUFF(n))
		{
			write_finished = wcs_write_in_progress_wait(cnl, cr, WBTEST_BG_UPDATE_DIRTYSTUCK2);
			if (!write_finished)
			{
				assert(gtm_white_box_test_case_enabled);
				BG_TRACE_PRO(wcb_t_end_sysops_dirtystuck2);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED,
						6, LEN_AND_LIT("wcb_t_end_sysops_dirtystuck2"),
						process_id, &ctn, DB_LEN_STR(gv_cur_region));
				return cdb_sc_cacheprob;
			}
		}
		assert((0 == cr->dirty) || (-1 == cr->read_in_progress));	/* dirty buffer cannot be read in progress */
		if (-1 != cr->read_in_progress)
			wait_for_rip = TRUE;
#		elif defined(VMS)
		/* the above #ifdef ideally should be #if defined(TWINNING) as that is the below code logically corresponds to */
		LOCK_BUFF_FOR_UPDATE(cr, n, &cnl->db_latch);
		assert(LATCH_CONFLICT >= n);
		assert(LATCH_SET <= n);
		VMS_ONLY(cr->backup_cr_off = (sm_off_t)0;)
		if (0 == cr->dirty)		/* Free, move to active queue */
		{
			assert(LATCH_SET == WRITE_LATCH_VAL(cr));
			assert(0 == cr->iosb.cond);
			assert(0 == cr->twin);
			assert(0 == n);
			if (-1 != cr->read_in_progress)
				wait_for_rip = TRUE;
			BG_TRACE(clean_to_mod);
		} else
		{
			assert(-1 == cr->read_in_progress);
			if (0 < n)
			{	/* it's owned for a write */
				assert(LATCH_CONFLICT == WRITE_LATCH_VAL(cr));
				cr1 = db_csh_getn(blkid);
				DEBUG_ONLY(
					save_cr = NULL;
					if (gtm_white_box_test_case_enabled)
						save_cr = cr1;	/* save cr for r_epid cleanup before setting to INVALID */
				)
				GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_DBCSHGETN_INVALID2, cr1, (cache_rec *)CR_NOTVALID);
				if ((cache_rec *)CR_NOTVALID == cr1)
				{
					assert(gtm_white_box_test_case_enabled);
					DEBUG_ONLY(
						if (NULL != save_cr)
						{	/* release r_epid lock on the valid cr1 returned from db_csh_getn */
							assert(save_cr->r_epid == process_id);
							save_cr->r_epid = 0;
							assert(0 == save_cr->read_in_progress);
							RELEASE_BUFF_READ_LOCK(save_cr);
						}
					)
					BG_TRACE_PRO(wcb_t_end_sysops_dirty_invcr);
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED,
							6, LEN_AND_LIT("wcb_t_end_sysops_dirty_invcr"),
							process_id, &ctn, DB_LEN_STR(gv_cur_region));
					return cdb_sc_cacheprob;
				}
				assert(NULL != cr1);
				assert(0 == cr1->dirty);
				assert(cr1->blk == blkid);
				LOCK_NEW_BUFF_FOR_UPDATE(cr1);	/* is new or cleaning up old; can't be active */
				if (cr != cr1)
				{	/* db_csh_getn did not give back the same cache-record, which it could do
					 * if it had to invoke wcs_wtfini.
					 */
					assert(0 == cr1->in_cw_set);
					assert(0 == cr1->in_tend);
					if (!dollar_tlevel)		/* stuff it in the array before setting in_cw_set */
					{
						assert((((MAX_BT_DEPTH * 2) - 1) * 2) > cr_array_index);
						PIN_CACHE_RECORD(cr1, cr_array, cr_array_index);
					} else
						TP_PIN_CACHE_RECORD(cr1, si);
					cr->in_tend = 0;
					cr1->in_tend = process_id;
					cr1->ondsk_blkver = cr->ondsk_blkver; /* copy blk version from old cache rec */
					if (gds_t_writemap == mode)
					{	/* gvcst_map_build doesn't do first_copy */
						memcpy(GDS_REL2ABS(cr1->buffaddr), GDS_REL2ABS(cr->buffaddr),
												BM_SIZE(csd->bplmap));
					}
					if (0 != cr->dirty)
					{	/* original block still in use */
						for (lcnt = 0; 0 != cr->twin; lcnt++)
						{	/* checking for an existing twin */
							if (FALSE == wcs_wtfini(gv_cur_region))
							{
								assert(gtm_white_box_test_case_enabled);
								BG_TRACE_PRO(wcb_t_end_sysops_wtfini_fail);
								send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
									LEN_AND_LIT("wcb_t_end_sysops_wtfini_fail"),
									process_id, &ctn, DB_LEN_STR(gv_cur_region));
								return cdb_sc_cacheprob;
							}
							/* If the cr already has a twin, then the predecessor should have
							 * been written out already (since otherwise the successor's write
							 * would not have started). Since wcs_wtfini looks at all cacherecs
							 * it should cut the twin connection once it sees the predecessor.
							 * The only exception is if the older twin still has in_cw_set set.
							 * In that case we will wait a while for that to be reset. Check that.
							 */
							DEBUG_ONLY(
								/* Note down in_cw_set in a local BEFORE checking cr->twin in the
								 * assert as the former could be changed concurrently by another
								 * process.
								 */
								in_cw_set = cr->twin
									? ((cache_rec_ptr_t)GDS_REL2ABS(cr->twin))->in_cw_set
									: 0;
							)
							assert((0 == lcnt) || !cr->dirty || !cr->twin || in_cw_set);
							if (0 != lcnt)
							{
								status = sys$dclast(wcs_wtstart, gv_cur_region, 0);
								if (SS$_NORMAL != status)
									send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBFILERR, 2,
										DB_LEN_STR(gv_cur_region), 0, status);
								wcs_sleep(lcnt);
							}
							if (0 != cr->twin)
							{
								GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_DIRTYSTUCK2,
									lcnt, (2 * BUF_OWNER_STUCK));
								if (BUF_OWNER_STUCK * 2 < lcnt)
								{
									assert(gtm_white_box_test_case_enabled);
									BG_TRACE_PRO(wcb_t_end_sysops_twin_stuck);
									send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
										LEN_AND_LIT("wcb_t_end_sysops_twin_stuck"),
										process_id, &ctn, DB_LEN_STR(gv_cur_region));
									return cdb_sc_cacheprob;
								}
								assert(cr->dirty > cr->flushed_dirty_tn);
							}
						}
						if (0 != cr->dirty)
						{	/* form twin*/
							cr1->twin = GDS_ABS2REL(cr);
							cr->twin = GDS_ABS2REL(cr1);
							BG_TRACE_PRO(blocked);
						} else
						{	/* wcs_wrtfini has processed cr. Just proceed with cr1 */
							cr->blk = CR_BLKEMPTY;
							BG_TRACE_PRO(blkd_made_empty);
						}
					} else
					{	/* If not cr->dirty, then wrtfini has processed it, just proceed with cr1 */
						cr->blk = CR_BLKEMPTY;
						BG_TRACE_PRO(blkd_made_empty);
					}
					/* Currently we compare out-of-crit "cr->buffaddr->tn" with the "hist->tn"
					 * to see if a block has been modified since the time we did our read
					 * (places are t_qread, tp_hist, gvcst_search and gvcst_put). In VMS,
					 * if a cache-record is currently being written to disk, and we need to
					 * update it, we find out another free cache-record and twin the two
					 * and make all changes only in the newer twin. Because of this, if we
					 * are doing our blkmod check against the old cache-record, our check
					 * may incorrectly conclude that nothing has changed. To prevent this
					 * the cycle number of the older twin has to be incremented. This way,
					 * the following cycle-check (in all the above listed places, a
					 * cdb_sc_blkmod check is immediately followed by a cycle check) will
					 * detect a restartable condition. Note that cr->bt_index should be set to 0
					 * before cr->cycle++ as t_qread relies on this order.
					 */
					cr->bt_index = 0;
					cr->cycle++;	/* increment cycle whenever blk number changes (for tp_hist) */
					cs->first_copy = TRUE;
					assert(-1 == cr->read_in_progress);
					cr1->backup_cr_off = GDS_ABS2REL(cr);
					cr = cr1;
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
				}	/* end of if (cr != cr1) */
				assert(cr->blk == blkid);
				bt->cache_index = GDS_ABS2REL(cr);
				cr->bt_index = GDS_ABS2REL(bt);
			} else
			{	/* it's modified but available */
				BG_TRACE(mod_to_mod);
			}
		}	/* end of if / else in dirty */
#		endif
		if (wait_for_rip)
		{	/* wait for another process in t_qread to stop overlaying the buffer, possible due to
			 *	(a) reuse of a killed block that's still in the cache OR
			 *	(b) the buffer has already been constructed in private memory
			 */
			assert(((gds_t_acquired == mode) && (!read_before_image || (NULL == cs->old_block)))
					|| (gds_t_acquired != mode) && (NULL != cs->new_buff));
			read_finished = wcs_read_in_progress_wait(cr, WBTEST_BG_UPDATE_READINPROGSTUCK2);
			if (!read_finished)
			{
				assert(gtm_white_box_test_case_enabled);
				BG_TRACE_PRO(wcb_t_end_sysops_dirtyripwait);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
					LEN_AND_LIT("wcb_t_end_sysops_dirtyripwait"),
					process_id, &ctn, DB_LEN_STR(gv_cur_region));
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
		assert(dse_running || write_after_image
				|| ((gds_t_acquired == mode) && (!read_before_image || (NULL == cs->old_block)))
				|| (gds_t_acquired != mode) && (0 != cs->new_buff));
		if (!dollar_tlevel)		/* stuff it in the array before setting in_cw_set */
		{
			assert((((MAX_BT_DEPTH * 2) - 1) * 2) > cr_array_index);
			PIN_CACHE_RECORD(cr, cr_array, cr_array_index);
		} else
			TP_PIN_CACHE_RECORD(cr, si);
	}
	assert(0 == cr->data_invalid);
	if (0 != cr->r_epid)
	{	/* must have got it with a db_csh_getn */
		if (gds_t_acquired != mode)
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
	}
	/* Update csd->blks_to_upgrd while we have crit */
	/* cs->ondsk_blkver is what gets filled in the PBLK record header as the pre-update on-disk block format.
	 * cr->ondsk_blkver is what is used to update the blks_to_upgrd counter in the file-header whenever a block is updated.
	 * They both better be the same. Note that PBLK is written if "read_before_image" is TRUE and cs->old_block is non-NULL.
	 * For created blocks that have NULL cs->old_blocks, t_create should have set format to GDSVCURR. Assert that too.
	 */
	assert(!read_before_image || (NULL == cs->old_block) || (cs->ondsk_blkver == cr->ondsk_blkver));
	assert((gds_t_acquired != mode) || (NULL != cs->old_block) || (GDSVCURR == cs->ondsk_blkver));
	desired_db_format = csd->desired_db_format;
	/* assert that appropriate inctn journal records were written at the beginning of the commit in t_end */
	assert((inctn_blkupgrd_fmtchng != inctn_opcode) || (GDSV4 == cr->ondsk_blkver) && (GDSV6 == desired_db_format));
	assert((inctn_blkdwngrd_fmtchng != inctn_opcode) || (GDSV6 == cr->ondsk_blkver) && (GDSV4 == desired_db_format));
	assert(!(JNL_ENABLED(csa) && csa->jnl_before_image) || !mu_reorg_nosafejnl
		|| (inctn_blkupgrd != inctn_opcode) || (cr->ondsk_blkver == desired_db_format));
	assert(!mu_reorg_upgrd_dwngrd_in_prog || (gds_t_acquired != mode));
	/* RECYCLED blocks could be converted by MUPIP REORG UPGRADE/DOWNGRADE. In this case do NOT update blks_to_upgrd */
	assert((gds_t_write_recycled != mode) || mu_reorg_upgrd_dwngrd_in_prog);
	if (gds_t_acquired == mode)
	{	/* It is a created block. It should inherit the desired db format. This is done as a part of call to
		 * SET_ONDSK_BLKVER in bg_update_phase1 and bg_update_phase2. Also, if that format is V4, increase blks_to_upgrd.
		 */
		if (GDSV4 == desired_db_format)
		{
			INCR_BLKS_TO_UPGRD(csa, csd, 1);
		}
	} else if (cr->ondsk_blkver != desired_db_format)
	{	/* Some sort of state change in the block format is occuring */
		switch(desired_db_format)
		{
			case GDSV6:
				/* V4 -> V5 transition */
				if (gds_t_write_recycled != mode)
					DECR_BLKS_TO_UPGRD(csa, csd, 1);
				break;
			case GDSV4:
				/* V5 -> V4 transition */
				if (gds_t_write_recycled != mode)
					INCR_BLKS_TO_UPGRD(csa, csd, 1);
				break;
			default:
				assertpro(FALSE);
		}
	}
	assert((gds_t_writemap != mode) || dse_running /* generic dse_running variable is used for caller = dse_maps */
		VMS_ONLY(|| cr->twin || CR_BLKEMPTY == cs->cr->blk)
		|| (cs->cr == cr) && (cs->cycle == cr->cycle));
	UNIX_ONLY(assert((gds_t_writemap != mode) || (cs->cycle == cr->cycle));) /* cannot assert in VMS due to twinning */
	/* Before marking this cache-record dirty, record the value of cr->dirty into cr->tn.
	 * This is used in phase2 to determine "recycled".
	 */
	dirty_tn = cr->dirty;
	cr->tn = dirty_tn ? ctn : 0;
	/* Now that we have locked a buffer for commit, there is one less free buffer available. Decrement wc_in_free.
	 * Do not do this if the cache-record is already dirty since this would have already been done the first time
	 * it transitioned from non-dirty to dirty.
	 */
	if (0 == dirty_tn)
	{
		DECR_CNT(&cnl->wc_in_free, &cnl->wc_var_lock);
		cr->dirty = ctn;		/* block will be dirty.	 Note the tn in which this occurred */
		/* At this point cr->flushed_dirty_tn could be EQUAL to ctn if this cache-record was used to update a different
		 * block in this very same transaction and reused later for the current block. Reset it to 0 to avoid confusion.
		 */
		cr->flushed_dirty_tn = 0;
	}
	/* Take backup of block in phase1 (while holding crit) in VMS and phase2 (outside of crit) in Unix.
	 * The reason why we cannot do this outside of crit in VMS is the following.
	 * If we do this in phase2 (outside of crit) and encounter an error in the middle of phase2 commit AND
	 * online backup is running at that point, secshr_db_clnup needs to then preserve the current buffer
	 * (in order for wcs_recover to later invoke backup_block on this buffer). But since we are in phase2,
	 * we dont hold crit so we cannot pick an arbitrary buffer to do the blk builds but instead have to
	 * use the buffer picked in phase1. But we cannot update the buffer and keep its before-image copy
	 * at the same time. Hence we keep the backup_block invocation in phase1 (in crit) for VMS.
	 * In Unix, since secshr_db_clnup does not run in kernel mode, it can do IO (not possible in VMS).
	 * Therefore, instead of in wcs_recover, we invoke "backup_block" right in secshr_db_clnup before
	 * building the buffer. This way, in VMS, if ever we end up in secshr_db_clnup in phase2, we can be sure
	 * there is no need to backup the block as it has already been done in phase1.
	 */
#	ifdef VMS
	blk_ptr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
	backup_cr_off = cr->backup_cr_off;
	if (0 == backup_cr_off)
	{
		backup_cr = cr;
		backup_blk_ptr = blk_ptr;
	} else
	{
		backup_cr = (sm_uc_ptr_t)GDS_REL2ABS(backup_cr_off);
		backup_blk_ptr = (sm_uc_ptr_t)GDS_REL2ABS(backup_cr->buffaddr);
		assert(gds_t_write_root != mode);
	}
	BG_BACKUP_BLOCK(csa, csd, cnl, cr, cs, blkid, backup_cr, backup_blk_ptr, block_saved, si->backup_block_saved);
	/* Update cr->ondsk_blkver to reflect the current desired_db_format. */
	SET_ONDSK_BLKVER(cr, csd, ctn);
#	endif
	cs->cr = cr;		/* note down "cr" so phase2 can find it easily (given "cs") */
	cs->cycle = cr->cycle;	/* update "cycle" as well (used later in tp_clean_up to update cycle in history) */
	cs->old_mode = -cs->old_mode;	/* negate it to indicate phase1 is complete for this cse (used by secshr_db_clnup) */
	assert(0 > cs->old_mode);
	/* Final asserts before letting go of this cache-record in phase1 */
	assert(process_id == cr->in_tend);
	assert(process_id == cr->in_cw_set);
	assert(cr->blk == cs->blk);
	assert(cr->dirty);
	assert(cr->dirty <= ctn);
	/* We have the cr locked so a concurrent writer should not be touching this. In VMS, wcs_wtstart sets cr->epid to
	 * a non-zero value before determining if it holds an exclusive lock on the cr so we cannot assert this there.
	 */
	UNIX_ONLY(assert(0 == cr->epid);)
	assert(cr->dirty > cr->flushed_dirty_tn);
	assert(cr->tn <= ctn);
	assert(0 == cr->data_invalid);
	assert(-1 == cr->read_in_progress);
	assert(LATCH_SET <= WRITE_LATCH_VAL(cr));
	return cdb_sc_normal;
}

enum cdb_sc	bg_update_phase2(cw_set_element *cs, trans_num ctn, trans_num effective_tn, sgm_info *si)
{
	int4			n;
	off_chain		chain;
	sm_uc_ptr_t		blk_ptr, backup_blk_ptr, chain_ptr;
	sm_off_t		backup_cr_off;
	cw_set_element		*cs_ptr, *nxt;
	cache_rec_ptr_t		cr, backup_cr;
	boolean_t		recycled;
	boolean_t		bmp_status;
	block_id		blkid;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t        cnl;
	enum gds_t_mode		mode;
	cache_que_heads_ptr_t	cache_state;
#ifdef GTM_SNAPSHOT
	boolean_t 		write_to_snapshot_file;
	snapshot_context_ptr_t	lcl_ss_ctx;
#endif
#	if defined(VMS)
	gv_namehead		*targ;
	srch_blk_status		*blk_hist;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mode = cs->mode;
	cr = cs->cr;
	/* Make sure asserts that were valid before letting go of this cache-record in phase1 are still so */
	assert(process_id == cr->in_tend);		/* should have been set in phase1 to update buffer */
	assert(process_id == cr->in_cw_set);		/* should have been set in phase1 to pin buffer until commit completes */
	assert(cr->blk == cs->blk);
	assert(cr->dirty);
	assert(cr->dirty <= ctn);
	/* We have the cr locked so a concurrent writer should not be touching this. In VMS, wcs_wtstart sets cr->epid to
	 * a non-zero value before determining if it holds an exclusive lock on the cr so we cannot assert this there.
	 */
	UNIX_ONLY(assert(0 == cr->epid);)
	assert(cr->dirty > cr->flushed_dirty_tn);
	assert(cr->tn <= ctn);
	assert(0 == cr->data_invalid);
	assert(-1 == cr->read_in_progress);
	assert(LATCH_SET <= WRITE_LATCH_VAL(cr));	/* Assert that we hold the update lock on the cache-record */
	csa = cs_addrs;		/* Local access copies */
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
	DEBUG_ONLY(
		if (NULL == cr)
		{
			assert(gtm_white_box_test_case_enabled);
			return cdb_sc_cacheprob;
		}
	)
	blk_ptr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
#	ifdef UNIX
	/* Take backup of block in phase2 (outside of crit) only for Unix.
	 * See comment in bg_update_phase1 (around similar macro usage for VMS) for why.
	 */
	backup_cr = cr;
	backup_blk_ptr = blk_ptr;
	if (!WAS_FREE(cs->blk_prior_state)) /* dont do before image write for backup for FREE blocks */
		BG_BACKUP_BLOCK(csa, csd, cnl, cr, cs, blkid, backup_cr, backup_blk_ptr, block_saved, si->backup_block_saved);
#	endif
	/* Update cr->ondsk_blkver to reflect the current desired_db_format. */
	SET_ONDSK_BLKVER(cr, csd, ctn);
#	ifdef GTM_SNAPSHOT
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
#	endif
	SET_DATA_INVALID(cr);	/* data_invalid should be set signaling intent to update contents of a valid block */
	if (gds_t_writemap == mode)
	{
		assert(csa->now_crit);	/* at this point, bitmap blocks are built while holding crit */
		assert(0 == (blkid & (BLKS_PER_LMAP - 1)));
		if (FALSE == cs->done)
			gvcst_map_build((uint4 *)cs->upd_addr, blk_ptr, cs, effective_tn);
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
		 * only exception to this is ONLINE ROLLBACK which holds crit for the entire duration
		 */
		assert(!csa->now_crit || cs->recompute_list_head || dse_running UNIX_ONLY(|| jgbl.onlnrlbk));
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
		if (!dollar_tlevel)
		{
			if (0 != cs->ins_off)
			{	/* reference to resolve: insert real block numbers in the buffer */
				assert(0 <= (short)cs->index);
				assert(cs - cw_set > cs->index);
				assert((SIZEOF(blk_hdr) + SIZEOF(rec_hdr)) <= cs->ins_off);
				assert((cs->ins_off + SIZEOF(block_id)) <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
				PUT_LONG((blk_ptr + cs->ins_off), cw_set[cs->index].blk);
				if (((nxt = cs + 1) < &cw_set[cw_set_depth]) && (gds_t_write_root == nxt->mode))
				{	/* If the next cse is a WRITE_ROOT, it contains a second block pointer
					 * to resolve though it operates on the current cse's block.
					 */
					assert(0 <= (short)nxt->index);
					assert(nxt - cw_set > nxt->index);
					assert(SIZEOF(blk_hdr) <= nxt->ins_off);
					assert(nxt->ins_off <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
					PUT_LONG((blk_ptr + nxt->ins_off), cw_set[nxt->index].blk);
				}
			}
		} else
		{
			if (0 != cs->first_off)
			{	/* TP - resolve pointer references to new blocks */
				for (chain_ptr = blk_ptr + cs->first_off; ; chain_ptr += chain.next_off)
				{
					GET_LONGP(&chain, chain_ptr);
					assert(1 == chain.flag);
					assert((chain_ptr - blk_ptr + chain.next_off + SIZEOF(block_id))
							<= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
					assert((int)chain.cw_index < sgm_info_ptr->cw_set_depth);
					tp_get_cw(si->first_cw_set, (int)chain.cw_index, &cs_ptr);
					PUT_LONG(chain_ptr, cs_ptr->blk);
					if (0 == chain.next_off)
						break;
				}
			}
		}
	}
	RESET_DATA_INVALID(cr);
	CERT_BLK_IF_NEEDED(certify_all_blocks, gv_cur_region, cs, blk_ptr, gv_target);
	if (cr->tn)
	{
		recycled = TRUE;
		assert(cr->dirty > cr->flushed_dirty_tn);
	} else
		recycled = FALSE;
	if (!recycled)
		cr->jnl_addr = cs->jnl_freeaddr;	/* update jnl_addr only if cache-record is not already in active queue */
	assert(recycled || (LATCH_SET == WRITE_LATCH_VAL(cr)));
	assert(!recycled || (LATCH_CLEAR < WRITE_LATCH_VAL(cr)));
	cache_state = csa->acc_meth.bg.cache_state;
	if (!recycled)
	{	/* stuff it on the active queue */
		VMS_ONLY(assert(0 == cr->iosb.cond);)
		/* Earlier revisions of this code had a kludge in place here to work around INSQTI failures (D9D06-002342).
		 * Those are now removed as the primary error causing INSQTI failures is believed to have been resolved.
		 */
		n = INSQTI((que_ent_ptr_t)&cr->state_que, (que_head_ptr_t)&cache_state->cacheq_active);
		GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_INSQTIFAIL, n, INTERLOCK_FAIL);
		if (INTERLOCK_FAIL == n)
		{
			assert(gtm_white_box_test_case_enabled);
			BG_TRACE_PRO(wcb_bg_update_lckfail1);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_bg_update_lckfail1"),
				process_id, &ctn, DB_LEN_STR(gv_cur_region));
			return cdb_sc_cacheprob;
		}
		ADD_ENT_TO_ACTIVE_QUE_CNT(&cnl->wcs_active_lvl, &cnl->wc_var_lock);
	}
	RELEASE_BUFF_UPDATE_LOCK(cr, n, &cnl->db_latch);
	/* "n" holds the pre-release value in Unix and post-release value in VMS, so check accordingly */
	UNIX_ONLY(assert(LATCH_CONFLICT >= n);)
	UNIX_ONLY(assert(LATCH_CLEAR < n);)	/* check that we did hold the lock before releasing it above */
	VMS_ONLY(assert(LATCH_SET >= n);)
	VMS_ONLY(assert(LATCH_CLEAR <= n);)	/* check that we did hold the lock before releasing it above */
	if (WRITER_BLOCKED_BY_PROC(n))
	{	/* It's off the active que, so put it back at the head to minimize the chances of blocks being "pinned" in memory.
		 * Note that this needs to be done BEFORE releasing the in_tend and in_cw_set locks as otherwise it is possible
		 * that a concurrent process in bg_update_phase1 could lock this buffer for update and incorrectly conclude that
		 * it has been locked by a writer when it has actually been locked by a process in bg_update_phase2.
		 */
#		ifdef VMS
		assert(LATCH_SET == WRITE_LATCH_VAL(cr));
		RELEASE_BUFF_UPDATE_LOCK(cr, n, &cnl->db_latch);
		assert(LATCH_CLEAR == n);
		assert(0 != cr->epid);
		assert(WRT_STRT_PNDNG == cr->iosb.cond);
		cr->epid = 0;
		cr->iosb.cond = 0;
		cr->wip_stopped = FALSE;
#		endif
		n = INSQHI((que_ent_ptr_t)&cr->state_que, (que_head_ptr_t)&cache_state->cacheq_active);
		GTM_WHITE_BOX_TEST(WBTEST_BG_UPDATE_INSQHIFAIL, n, INTERLOCK_FAIL);
		if (INTERLOCK_FAIL == n)
		{
			assert(gtm_white_box_test_case_enabled);
			BG_TRACE_PRO(wcb_bg_update_lckfail2);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_bg_update_lckfail2"),
				process_id, &ctn, DB_LEN_STR(gv_cur_region));
			return cdb_sc_cacheprob;
		}
	}
#	ifdef VMS
	if (cr->backup_cr_off && (gds_t_write == mode))	/* update landed in a different cache-record (twin) */
	{	/* If valid clue and this block is in it, need to update buffer address */
		targ = (!dollar_tlevel ? gv_target : cs->blk_target);
		if ((NULL != targ) && (0 != targ->clue.end))
		{
			blk_hist = &targ->hist.h[cs->level];
			blk_hist->buffaddr = blk_ptr;
			blk_hist->cr = cr;
			blk_hist->cycle = cr->cycle;	/* must do this before unpinning the cache-record */
		}
	}
#	endif
	/* A concurrent process reading this block will wait for in_tend to become FALSE and then proceed with its
	 * database operation. Later it will reach t_end/tp_tend doing validations at which point it will need to set in_cw_set.
	 * It expects in_cw_set to be 0 at that point. Therefore in_cw_set needs to be reset to 0 BEFORE resetting in_tend.
	 * Need a write memory barrier to ensure that these two updates are seen in that order by any other concurrent process.
	 */
	assert(process_id == cr->in_cw_set);
	UNPIN_CACHE_RECORD(cr);
	assert(!cr->in_cw_set);
	SHM_WRITE_MEMORY_BARRIER;
	assert(process_id == cr->in_tend);	/* should still be valid */
	cr->in_tend = 0;
	VERIFY_QUEUE_LOCK(&cache_state->cacheq_active, &cnl->db_latch);
	cs->old_mode = -cs->old_mode;	/* negate it back to indicate phase2 is complete for this cse (used by secshr_db_clnup) */
	assert(0 < cs->old_mode);
	return cdb_sc_normal;
}

/* Used to prevent staleness of buffers. Start timer to call wcs_stale to do periodic flushing */
void	wcs_timer_start(gd_region *reg, boolean_t io_ok)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	enum db_acc_method	acc_meth;
	int4			wtstart_errno;
#	if defined(VMS)
	static readonly int4	pause[2] = { TIM_AST_WAIT, -1 };
	int			n, status;
#	elif defined(UNIX)
	INTPTR_T		reg_parm;
	jnl_private_control	*jpc;
#	endif

	assert(reg->open); /* there is no reason we know of why a region should be closed at this point */
	if (!reg->open)    /* in pro, be safe though and dont touch an already closed region */
		return;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	acc_meth = csd->acc_meth;
	/* This process can only have one flush timer per region. Overall, there can only be
	 * 2 outstanding timers per region for the entire system. Note: wcs_timers starts at -1.
	 */
#	if defined(UNIX)
	if ((FALSE == csa->timer) && (cnl->wcs_timers < 1))
	{
		if ((dba_bg == acc_meth) ||				/* bg mode or */
		    (dba_mm == acc_meth && (0 < csd->defer_time)))	/* defer'd mm mode */
		{
			reg_parm = (UINTPTR_T)reg;
			csa->timer = TRUE;
			INCR_CNT(&cnl->wcs_timers, &cnl->wc_var_lock);
			wcs_stale_fptr = &wcs_stale;
			start_timer((TID)reg,
				    csd->flush_time[0] * (dba_bg == acc_meth ? 1 : csd->defer_time),
				    &wcs_stale, SIZEOF(reg_parm), (char *)&reg_parm);
			BG_TRACE_ANY(csa, stale_timer_started);
		}
	}
#	elif defined(VMS)
	if (dba_mm == acc_meth)
	{	/* not implemented yet */
		return;
	} else	if ((FALSE == csa->timer) && (1 > cnl->wcs_timers))
	{
		for (n = 0; ((0 > cnl->wcs_timers) || (0 == n)); n++)
		{
			while ((1 > astq_dyn_avail) && (0 > cnl->wcs_timers))
			{
				status = sys$setast(DISABLE);
				wcs_wtstart(reg);
				if (SS$_WASSET == status)
					ENABLE_AST;
				if (SS$_NORMAL == sys$setimr(efn_immed_wait, &pause, 0, 0, 0))
				{
					sys$synch(efn_immed_wait, 0);
				}
			}
			if (0 < astq_dyn_avail)
			{
				astq_dyn_avail--;
				csa->timer = TRUE;
				adawi(1, &cnl->wcs_timers);
				status = sys$setimr (efn_ignore, &csd->flush_time[0], wcs_stale, reg, 0);
				if (0 == (status & 1))
				{
					adawi(-1, &cnl->wcs_timers);
					csa->timer = FALSE;
					astq_dyn_avail++;
				}
			}
		}
	}
#	endif
	/* If we are being called from a timer driven routine, it is not possible to do IO at this time
	 * because the state of the machine (crit check, lseekio, etc.) is not being checked here.
	 */
	if (FALSE == io_ok)
		return;
#	ifdef UNIX
	/* Use this opportunity to sync the db if necessary (as a result of writing an epoch record). */
	if (dba_bg == acc_meth && JNL_ENABLED(csd))
	{
		jpc = csa->jnl;
		if (jpc && jpc->jnl_buff->need_db_fsync && (NOJNL != jpc->channel))
			jnl_qio_start(jpc);	/* See jnl_qio_start for how it achieves the db_fsync */
	}
	/* Need to add something similar for MM here */
#	endif
	/* If we are getting too full, do some i/o to clear some out.
	 * This should happen only as we are getting near the saturation point.
	 */
	if (csd->flush_trigger <= cnl->wcs_active_lvl)
	{	/* Already in need of a good flush */
		BG_TRACE_PRO_ANY(csa, active_lvl_trigger);
		DCLAST_WCS_WTSTART(reg, 0, wtstart_errno); /* a macro that dclast's wcs_wtstart and checks for errors etc. */
		/* DCLAST_WCS_WTSTART macro does not set the wtstart_errno variable in VMS. But in any case, we do not
		 * support database file extensions with MM on VMS. So we could never get a ERR_GBLOFLOW error there.
		 * Therefore the file extension check below is done only in Unix.
		 */
		UNIX_ONLY(
			if ((dba_mm == acc_meth) && (ERR_GBLOFLOW == wtstart_errno))
				wcs_recover(reg);
		)
		csa->stale_defer = FALSE;		/* This took care of any pending work for this region */
	}
	return;
}

/* A timer has popped. Some buffers are stale -- start writing to the database */
#if defined(UNIX)
void	wcs_stale(TID tid, int4 hd_len, gd_region **region)
# elif defined(VMS)
void	wcs_stale(gd_region *reg)
#endif
{
	boolean_t		need_new_timer;
	gd_region		*save_region;
	sgmnt_addrs		*csa, *save_csaddrs, *check_csaddrs;
	sgmnt_data_ptr_t	csd, save_csdata;
#	ifdef UNIX
	NOPIO_ONLY(boolean_t	lseekIoInProgress_flag;)
	gd_region		*reg;
#	endif
	enum db_acc_method	acc_meth;

	save_region = gv_cur_region;		/* Certain debugging calls expect gv_cur_region to be correct */
	save_csaddrs = cs_addrs;
	save_csdata = cs_data;
	check_csaddrs = (NULL == save_region || FALSE == save_region->open) ? NULL : &FILE_INFO(save_region)->s_addrs;
		/* Save to see if we are in crit anywhere */
	UNIX_ONLY(reg = *region;)
	assert(reg->open);
	/* Note the non-usage of TP_CHANGE_REG_IF_NEEDED macros since this routine can be timer driven. */
	TP_CHANGE_REG(reg);
	csa = cs_addrs;
	csd = cs_data; /* csa and csd might be NULL if region has been closed; we expect all timers for a closed region to have
			  been cancelled. But, for safety, we return if csd happens to be NULL */
	assert(csd == csa->hdr);
	assert(NULL != csd);
	acc_meth = csd->acc_meth;
	if ((NULL == csd)
		UNIX_ONLY(|| ((dba_mm == acc_meth) && (csa->total_blks != csa->ti->total_blks))) /* csd == NULL <=> csa == NULL */
		)
	{	/* don't write if region has been closed, or in UNIX if acc meth is MM and file extended */
		if (save_region != gv_cur_region)
		{
			gv_cur_region = save_region;
			cs_addrs = save_csaddrs;
			cs_data = save_csdata;
		}
		return;
	}
	VMS_ONLY(assert(dba_bg == acc_meth);)
	BG_TRACE_ANY(csa, stale_timer_pop);
	/* Default to need a new timer in case bypass main code because of invalid conditions */
	need_new_timer = TRUE;
	/****************************************************************************************************
	   We don't want to do expensive IO flushing if:
	   1) UNIX-ONLY : We are in the midst of lseek/read/write IO. This could reset an lseek.
	   2) We are aquiring crit in any of our regions.
	      Note that the function "mutex_deadlock_check" resets crit_count to 0 temporarily even though we
	      might actually be in the midst of acquiring crit. Therefore we should not interrupt mainline code
	      if we are in "mutex_deadlock_check" as otherwise it presents reentrancy issues.
	   3) We have crit in any region OR are in the middle of commit for this region even though we dont
	      hold crit (in bg_update_phase2) OR are in wcs_wtstart (potentially holding write interlock and
	      keeping another process in crit waiting). Assumption is that if region we were in was not crit, we're
	      clear. This is not strictly true in some special TP cases on the final retry if the previous retry did
	      not get far enough into the transaction to cause all regions to be locked down but this case is
	      statistically infrequent enough that we will go ahead and do the IO in crit "this one time".
	   4) We are in a "fast lock".
	   **************************************************************************************************/
	UNIX_ONLY(GET_LSEEK_FLAG(FILE_INFO(reg)->fd, lseekIoInProgress_flag);)
	if ((0 == crit_count) && !in_mutex_deadlock_check && OK_TO_INTERRUPT
		UNIX_ONLY(NOPIO_ONLY(&& (FALSE == lseekIoInProgress_flag)))
		&& ((NULL == check_csaddrs) || !T_IN_CRIT_OR_COMMIT_OR_WRITE(check_csaddrs))
		&& (0 == fast_lock_count))
	{
		BG_TRACE_PRO_ANY(csa, stale);
		switch (acc_meth)
		{
			case dba_bg:
				/* Flush at least some of our cache */
				UNIX_ONLY(wcs_wtstart(reg, 0);)
				VMS_ONLY(wcs_wtstart(reg);)
				/* If there is no dirty buffer left in the active queue, then no need for new timer */
				if (0 == csa->acc_meth.bg.cache_state->cacheq_active.fl)
					need_new_timer = FALSE;
				break;
#    			if defined(UNIX)
			case dba_mm:
				wcs_wtstart(reg, 0);
				assert(csd == csa->hdr);
				need_new_timer = FALSE;
				break;
#			endif
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
	/* If fast_lock_count is non-zero, we must go ahead and set a new timer even if we don't need one
	 * because we cannot fall through to the DECR_CNT for wcs_timers below because we could deadlock.
	 * On VMS, this is not necessarily an issue but rather than disturb this code at this time, we are
	 * making it do the same as on UNIX. This can be revisited. 5/2005 SE.
	 * If fast_lock_count is zero, then the regular tests determine if we set a new timer or not.
	 */
	if (0 != fast_lock_count || (need_new_timer && 0 >= csa->nl->wcs_timers))
	{
		UNIX_ONLY(start_timer((TID)reg,
			    csd->flush_time[0] * (dba_bg == acc_meth ? 1 : csd->defer_time),
			    &wcs_stale,
			    SIZEOF(region),
			    (char *)region);)
		VMS_ONLY(sys$setimr(efn_ignore, csd->flush_time, wcs_stale, reg, 0);)
		BG_TRACE_ANY(csa, stale_timer_started);
	} else
	{	/* We aren't creating a new timer so decrement the count for this one that is now done */
		DECR_CNT(&csa->nl->wcs_timers, &csa->nl->wc_var_lock);
		VMS_ONLY(++astq_dyn_avail;)
		csa->timer = FALSE;		/* No timer set for this region by this process anymore */
	}
	/* To restore to former glory, don't use TP_CHANGE_REG, 'coz we might mistakenly set cs_addrs and cs_data to NULL
	 * if the region we are restoring has been closed. Don't use tp_change_reg 'coz we might be ripping out the structures
	 * needed in tp_change_reg in gv_rundown.
	 */
	gv_cur_region = save_region;
	cs_addrs = save_csaddrs;
	cs_data = save_csdata;
	return;
}
