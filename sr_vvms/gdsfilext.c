/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <iodef.h>
#include <lckdef.h>
#include <psldef.h>
#include <dvidef.h>
#include <rms.h>
#include <ssdef.h>
#include <efndef.h>

#include "ast.h"
#include "buddy_list.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbml.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "iosp.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "vmsdtype.h"
#include "locks.h"
#include "send_msg.h"
#include "bit_set.h"
#include "gt_timer.h"
#include "dbfilop.h"
#include "disk_block_available.h"
#include "gtmmsg.h"
#include "gdsfilext.h"
#include "bm_getfree.h"
#include "gdsblk.h"		/* needed for gds_blk_downgrade.h */
#include "gds_blk_downgrade.h"	/* for IS_GDS_BLK_DOWNGRADE_NEEDED macro */
#include "gtmimagename.h"

GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data_ptr_t cs_data;
GBLREF	unsigned char	cw_set_depth;
GBLREF	uint4		dollar_tlevel;
GBLREF	gd_region	*gv_cur_region;
GBLREF	boolean_t	mu_reorg_process;
GBLREF	inctn_opcode_t	inctn_opcode;
GBLREF	sgm_info	*sgm_info_ptr;
GBLREF	unsigned int	t_tries;
GBLREF	jnl_gbls_t	jgbl;
GBLREF	inctn_detail_t	inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	boolean_t	gtm_dbfilext_syslog_disable;	/* control whether db file extension message is logged or not */

error_def(ERR_DBFILERR);
error_def(ERR_DBFILEXT);
error_def(ERR_DBOPNERR);
error_def(ERR_DSKSPACEFLOW);
error_def(ERR_ERRCALL);
error_def(ERR_GBLOFLOW);
error_def(ERR_JNLFILOPN);
error_def(ERR_JNLFLUSH);
error_def(ERR_MUSTANDALONE);
error_def(ERR_TEXT);
error_def(ERR_TOTALBLKMAX);

#define ERROR_CLEANUP				\
	sys$close(&fcb);			\
	if (FALSE == was_crit)			\
		rel_crit(gv_cur_region);

uint4 gdsfilext(uint4 blocks, uint4 filesize)
{
	bool			was_crit;
	char			*buff;
	unsigned char		*mastermap[2], *retadr[2];
	short			iosb[4];
	uint4			alloc_blocks, avail_blocks, new_bit_maps, blocks_needed, block_factor,
				bplmap, jnl_status, map, new_blocks, new_total,
				save_inctn_opcode, save_stv, status;
	struct FAB		fcb;
	struct RAB		rab;
	file_control		*fc;
	vms_gds_info		*gds_info;
	trans_num		curr_tn;
	int4			prev_extend_blks_to_upgrd;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;

	/* Both blocks and total blocks are unsigned ints so make sure we aren't asking for huge numbers that will
	   overflow and end up doing silly things.
	*/
	assert(blocks <= (MAXTOTALBLKS(cs_data) - cs_data->trans_hist.total_blks));

	if (0 == blocks)
		return (NO_FREE_SPACE); /* should this be changed to show extension not enabled ? */
	gds_info = gv_cur_region->dyn.addr->file_cntl->file_info;
	assert(cs_data == cs_addrs->hdr);	/* because following line used to check cs_addrs->hdr (instead of cs_data) */
	if (dba_mm == cs_data->acc_meth)
	{	/* mm only works MUPIP standalone, but calling this keeps the logic in one place */
		status = gtm_enqw(EFN$C_ENF, LCK$K_EXMODE, &gds_info->file_cntl_lsb,
				LCK$M_CONVERT | LCK$M_NOQUEUE | LCK$M_NODLCKWT,
				NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = gds_info->file_cntl_lsb.cond;
		if (SS$_NORMAL != status)
		{
			gtm_putmsg(VARLSTCNT(5) ERR_MUSTANDALONE, 2, FAB_LEN_STR(&fcb), status);
			return (NO_FREE_SPACE); /* should this be changed to show extention not enabled ? */
		}
	}
	was_crit = cs_addrs->now_crit;
	/* If we are coming from mupip_extend (which gets crit itself) we better have waited for any unfreezes to occur.
	 * If we are coming from online rollback (when that feature is available), we will come in holding crit and in
	 * 	the final retry. In that case too, we expect to have waited for unfreezes to occur in the caller itself.
	 * Therefore if we are coming in holding crit from MUPIP, we expect the db to be unfrozen so no need to wait for
	 * freeze.
	 * If we are coming from GT.M and final retry (in which case we come in holding crit) we expect to have waited
	 * 	for any unfreezes (by invoking tp_crit_all_regions) to occur (TP or non-TP) before coming into this
	 *	function. However, there is one exception. In the final retry, if tp_crit_all_regions notices that
	 *	at least one of the participating regions did ONLY READs, it will not wait for any freeze on THAT region
	 *	to complete before grabbing crit. Later, in the final retry, if THAT region did an update which caused
	 *	op_tcommit to invoke bm_getfree->gdsfilext, then we would have come here with a frozen region on which
	 *	we hold crit.
	 */
	assert(!was_crit || !cs_data->freeze || (dollar_tlevel && (CDB_STAGNATE <= t_tries)));
	/*
	 * If we are in the final retry and already hold crit, it is possible that csa->nl->wc_blocked is also set to
	 * TRUE (by a concurrent process in phase2 which encountered an error in the midst of commit and secshr_db_clnup
	 * finished the job for it). In this case we do NOT want to invoke wcs_recover as that will update the "bt"
	 * transaction numbers without correspondingly updating the history transaction numbers (effectively causing
	 * a cdb_sc_blkmod type of restart). Therefore do NOT call grab_crit (which unconditionally invokes wcs_recover)
	 * if we already hold crit.
	 */
	if (!was_crit)
	{
		for ( ; ; )
		{
			grab_crit(gv_cur_region);
			if (!cs_data->freeze)
				break;
			rel_crit(gv_cur_region);
			while (cs_data->freeze)
				hiber_start(1000);
		}
	} else if (cs_data->freeze && dollar_tlevel)
	{	/* We don't want to continue with file extension as explained above. Hence return with an error code which
		 * op_tcommit will recognize (as a cdb_sc_needcrit type of restart) and restart accordingly.
		 */
		assert(CDB_STAGNATE <= t_tries);
		return (uint4)(FINAL_RETRY_FREEZE_PROG);
	}
	assert(cs_addrs->ti->total_blks == cs_data->trans_hist.total_blks);
	if (cs_data->trans_hist.total_blks != filesize)
	{
		if (FALSE == was_crit)
			rel_crit(gv_cur_region);
		return (SS$_NORMAL);
	}
	if (IS_GTM_IMAGE && (2 * (dollar_tlevel ? sgm_info_ptr->cw_set_depth : cw_set_depth) < cs_addrs->ti->free_blocks))
	{
		if (FALSE == was_crit)
		{
			rel_crit(gv_cur_region);
			return (EXTEND_SUSPECT);
		}
		/* If free_blocks counter is not ok, then correct it. Do the check again. If still fails, then GTMASSERT. */
		if (is_free_blks_ctr_ok() ||
				(2 * (dollar_tlevel ? sgm_info_ptr->cw_set_depth : cw_set_depth) < cs_addrs->ti->free_blocks))
			GTMASSERT;	/* held crit through bm_getfree into gdsfilext and still didn't get it right */
	}
	CHECK_TN(cs_addrs, cs_data, cs_data->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
	bplmap = cs_data->bplmap;
	fcb = cc$rms_fab;
	fcb.fab$b_shr = gds_info->fab->fab$b_shr;
	fcb.fab$l_fna = &(gv_cur_region->dyn.addr->fname);
	fcb.fab$b_fns = gv_cur_region->dyn.addr->fname_len;
	fcb.fab$b_fac = FAB$M_BIO | FAB$M_GET | FAB$M_PUT;
	fcb.fab$l_fop = FAB$M_CBT;
	if (RMS$_NORMAL == (status = sys$open(&fcb)))
	{
		block_factor = cs_data->blk_size / DISK_BLOCK_SIZE;
		if (SS$_NORMAL == (status = disk_block_available(gds_info->fab->fab$l_stv, &avail_blocks)))
		{
			avail_blocks /= block_factor;
			if ((blocks * EXTEND_WARNING_FACTOR) > avail_blocks)
			{
				if (blocks > avail_blocks)
				{
					send_msg(VARLSTCNT(11) ERR_GBLOFLOW, 0, ERR_DBFILERR, 2, FAB_LEN_STR(&fcb),
						ERR_ERRCALL, 3, CALLFROM);
					ERROR_CLEANUP;
					return (NO_FREE_SPACE);
				}
				send_msg(VARLSTCNT(10) ERR_DSKSPACEFLOW, 3, FAB_LEN_STR(&fcb), avail_blocks * block_factor,
					ERR_ERRCALL, 3, CALLFROM);
			}
		} else
		{
			send_msg(VARLSTCNT(13) ERR_GBLOFLOW, 0, ERR_DBFILERR, 2, FAB_LEN_STR(&fcb), status, 0,
				ERR_ERRCALL, 3, CALLFROM);
			ERROR_CLEANUP;
			return (NO_FREE_SPACE);
		}
	} else
	{
		if (FALSE == was_crit)
			rel_crit(gv_cur_region);
		send_msg(VARLSTCNT(15) ERR_GBLOFLOW, 0, ERR_DBOPNERR, 2, FAB_LEN_STR(&fcb), status, 0, fcb.fab$l_stv, 0,
			ERR_ERRCALL, 3, CALLFROM);
		return (NO_FREE_SPACE);
	}
	/* new total of non-bitmap blocks will be number of current, non-bitmap blocks, plus new blocks desired
	 * There are (bplmap - 1) non-bitmap blocks per bitmap, so add (bplmap - 2) to number of non-bitmap blocks
	 *	and divide by (bplmap - 1) to get total number of bitmaps for expanded database. (must round up in this
	 *	manner as every non-bitmap block must have an associated bitmap)
	 * Current number of bitmaps is (total number of current blocks + bplmap - 1) / bplmap.
	 * Subtract current number of bitmaps from number needed for expanded database to get number of new bitmaps needed.
	 */
	new_bit_maps = DIVIDE_ROUND_UP(cs_data->trans_hist.total_blks
			- DIVIDE_ROUND_UP(cs_data->trans_hist.total_blks, bplmap) + blocks, bplmap - 1)
			- DIVIDE_ROUND_UP(cs_data->trans_hist.total_blks, bplmap);
	new_blocks = blocks + new_bit_maps;
	assert(0 < (int)new_blocks);
	new_total = cs_data->trans_hist.total_blks + new_blocks;
	if (new_total > MAXTOTALBLKS(cs_data))
	{
		send_msg(VARLSTCNT(7) ERR_TOTALBLKMAX, 0, ERR_ERRCALL, 3, CALLFROM);
		ERROR_CLEANUP;
		return (NO_FREE_SPACE);
	}
	if (JNL_ENABLED(cs_data))
	{
		if (!jgbl.dont_reset_gbl_jrec_time)
			SET_GBL_JREC_TIME;	/* needed before jnl_ensure_open as that can write jnl records */
		jpc = cs_addrs->jnl;
		jbp = jpc->jnl_buff;
		/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order
		 * of jnl records. This needs to be done BEFORE the jnl_ensure_open as that could write
		 * journal records (if it decides to switch to a new journal file).
		 */
		ADJUST_GBL_JREC_TIME(jgbl, jbp);
		jnl_status = jnl_ensure_open();
		if (jnl_status)
		{
			send_msg(VARLSTCNT(11) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region),
				ERR_ERRCALL, 3, CALLFROM);
			ERROR_CLEANUP;
			return (NO_FREE_SPACE); /* should have better return status */
		}
	}
	buff = malloc(cs_data->blk_size);
	memset(buff, 0, cs_data->blk_size);
	rab = cc$rms_rab;
	rab.rab$l_fab = &fcb;
	rab.rab$l_rbf = buff;
	rab.rab$w_rsz = cs_data->blk_size;
	rab.rab$l_rop |= RAB$M_EOF;
	if (RMS$_NORMAL != (status = sys$connect(&rab)))
	{
		save_stv = fcb.fab$l_stv;
		free(buff);
		send_msg(VARLSTCNT(15) ERR_GBLOFLOW, 0, ERR_DBFILERR, 2, FAB_LEN_STR(&fcb), status, 0, save_stv, 0,
			 ERR_ERRCALL, 3, CALLFROM);
		ERROR_CLEANUP;
		return (NO_FREE_SPACE);
	}
	/* get last block needed, position write to be (block_size/512-1) blocks before so that will end at desired block */
	rab.rab$l_bkt = cs_data->start_vbn + (block_factor * new_total) - block_factor;
	status = sys$write(&rab);
	free(buff);
	if (RMS$_NORMAL != status)
	{
		save_stv = fcb.fab$l_stv;
		send_msg(VARLSTCNT(15) ERR_GBLOFLOW, 0, ERR_DBFILERR, 2, FAB_LEN_STR(&fcb), status, 0, save_stv, 0,
			 ERR_ERRCALL, 3, CALLFROM);
		ERROR_CLEANUP;
		return (NO_FREE_SPACE);
	}

	if (RMS$_NORMAL != (status = sys$close(&fcb)))
	{
		if (FALSE == was_crit)
			rel_crit(gv_cur_region);
		send_msg(VARLSTCNT(15) ERR_GBLOFLOW, 0, ERR_DBFILERR, 2, FAB_LEN_STR(&fcb), status, 0, fcb.fab$l_stv, 0,
			ERR_ERRCALL, 3, CALLFROM);
		return (NO_FREE_SPACE);
	}
	DEBUG_ONLY(prev_extend_blks_to_upgrd = cs_data->blks_to_upgrd;)
	/* inctn_detail.blks_to_upgrd_delta holds the increase in "csd->blks_to_upgrd" due to the file extension */
	inctn_detail.blks2upgrd_struct.blks_to_upgrd_delta
		= (IS_GDS_BLK_DOWNGRADE_NEEDED(cs_data->desired_db_format) ? new_bit_maps : 0);
	if (JNL_ENABLED(cs_data))
	{
		save_inctn_opcode = inctn_opcode;
		if (mu_reorg_process)
			inctn_opcode = inctn_gdsfilext_mu_reorg;
		else
			inctn_opcode = inctn_gdsfilext_gtm;
		if (0 == jpc->pini_addr)
			jnl_put_jrt_pini(cs_addrs);
		jnl_write_inctn_rec(cs_addrs);
		inctn_opcode = save_inctn_opcode;
		/* Harden INCTN to disk before updating/flushing database. This will ensure that any positive adjustment to the
		 * blks_to_upgrd counter (stored in the inctn record) is seen by recovery before a V4 format bitmap block is.
		 */
		jnl_status = jnl_flush(gv_cur_region);
		if (SS_NORMAL != jnl_status)
		{
			send_msg(VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(cs_data),
				ERR_TEXT, 2, RTS_ERROR_TEXT("Error with journal flush during gdsfilext"),
				jnl_status);
			assert(NOJNL == jpc->channel); /* jnl file lost has been triggered */
			/* In this routine, all code that follows from here on does not assume anything about the
			 * journaling characteristics of this database so it is safe to continue execution even though
			 * journaling got closed in the middle. Let the caller deal with this situation.
			 */
		}
	}
	if (new_bit_maps)
	{
		for (map = ROUND_UP(cs_data->trans_hist.total_blks, bplmap); map < new_total; map += bplmap)
		{
			DEBUG_ONLY(new_bit_maps--;)
			if (SS$_NORMAL != (status = bml_init(map)))
			{
				if (FALSE == was_crit)
					rel_crit(gv_cur_region);
				send_msg(VARLSTCNT(13) ERR_GBLOFLOW, 0, ERR_DBFILERR, 2, FAB_LEN_STR(&fcb), status, 0,
					ERR_ERRCALL, 3, CALLFROM);
				return (NO_FREE_SPACE);
			}
		}
		assert(0 == new_bit_maps);
	}
	assert(cs_data->blks_to_upgrd == (inctn_detail.blks2upgrd_struct.blks_to_upgrd_delta + prev_extend_blks_to_upgrd));
	assert(0 < (int)blocks);
	assert(0 < (int)(cs_data->trans_hist.free_blocks + blocks));
	cs_data->trans_hist.free_blocks += blocks;
	blocks = cs_data->trans_hist.total_blks;
	cs_data->trans_hist.total_blks = new_total;
	if (blocks / bplmap * bplmap != blocks)
	{
		bit_set( blocks / bplmap, MM_ADDR(cs_data)); /* Mark old last local map as having space */
		if ((int4)blocks > cs_addrs->nl->highest_lbm_blk_changed)
			cs_addrs->nl->highest_lbm_blk_changed = blocks;
	}
	cs_addrs->ti->mm_tn++;
	curr_tn = cs_addrs->ti->curr_tn;
	assert(cs_addrs->ti->early_tn == cs_addrs->ti->curr_tn);
	/* do not increment transaction number for forward recovery */
	if (!jgbl.forw_phase_recovery || JNL_ENABLED(cs_data))
	{
		cs_data->trans_hist.early_tn = cs_data->trans_hist.curr_tn + 1;
		INCREMENT_CURR_TN(cs_data);
	}
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->file_type = cs_addrs->hdr->acc_meth;
	fc->op = FC_WRITE;
	fc->op_buff = (sm_uc_ptr_t)cs_addrs->hdr;
	fc->op_len = SIZEOF_FILE_HDR(cs_data);
	fc->op_pos = 1;		/* write from the start of the file-header */
		/* Note: (for bg) if machine crashes after update is done, BT queues could malformed from partial write. */
	status = dbfilop(fc);
	if (FALSE == was_crit)
		rel_crit(gv_cur_region);
	if (SS$_NORMAL != status)
	{
		send_msg(VARLSTCNT(13) ERR_GBLOFLOW, 0, ERR_DBFILERR, 2, FAB_LEN_STR(&fcb), status, 0, ERR_ERRCALL, 3, CALLFROM);
		return (NO_FREE_SPACE);
	}
	INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_db_extends, 1);
	if (!gtm_dbfilext_syslog_disable)
		send_msg(VARLSTCNT(7) ERR_DBFILEXT, 5, FAB_LEN_STR(&fcb), blocks, new_total, &curr_tn);
	return (SS$_NORMAL);
}
