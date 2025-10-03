/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "cli.h"
#include "util.h"
#include "send_msg.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "bit_set.h"
#include "bit_clear.h"
#include "t_begin_crit.h"
#include "t_write.h"
#include "t_end.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"
#include "process_deferred_stale.h"
#include "gvcst_blk_build.h"

GBLREF boolean_t	unhandled_stale_timer_pop;
GBLREF char		*update_array, *update_array_ptr;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF short		crash_count;
GBLREF srch_hist	dummy_hist;
GBLREF uint4		update_array_size;

error_def(ERR_DBRDONLY);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEFAIL);

void dse_maps(void)
{
	blk_segment		*bs1, *bs_ptr;
	block_id		blk, blk_index, bml_blk, bml_index, bml_list_size, total_blks;
	boolean_t		was_crit;
	cache_rec_ptr_t		dummy_cr;
	char			util_buff[MAX_UTIL_LEN];
	int			util_len;
	int4			blk_seg_cnt, blk_size;		/* needed for BLK_INIT, BLK_SEG and BLK_FINI macros */
	int4			blks_in_bitmap, bml_size, bplmap, dummy_int;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		bp;
	srch_blk_status		blkhist;
	uchar_ptr_t		blk_ptr;
	unsigned char		*bml_list;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	DSE_DB_IS_TOO_OLD(cs_addrs, cs_data, gv_cur_region);
	if (CLI_PRESENT == cli_present("BUSY") || CLI_PRESENT == cli_present("FREE") ||
		CLI_PRESENT == cli_present("MASTER") || CLI_PRESENT == cli_present("RESTORE_ALL"))
	{
		if (gv_cur_region->read_only)
			RTS_ERROR_CSA_ABT(csa, VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	}
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	assert(&FILE_INFO(gv_cur_region)->s_addrs == csa);
	was_crit = csa->now_crit;
	UPDATE_CRASH_COUNT(csa, crash_count);
	csd = csa->hdr;
	assert(csd == cs_data);
	bplmap = csd->bplmap;
	if (0 == bplmap)
	{
		util_out_print("Cannot perform map updates:  bplmap field of file header is zero.", TRUE);
		return;
	}
	blk_size = csd->blk_size;
	if (CLI_PRESENT == cli_present("RESTORE_ALL"))
	{
		total_blks = csa->ti->total_blks;
		assert(ROUND_DOWN2(blk_size, 2 * SIZEOF(int4)) == blk_size);
		bml_size = BM_SIZE(bplmap);
		bml_list_size = ((total_blks + bplmap - 1) / bplmap) * bml_size;
		bml_list = (unsigned char *)malloc(bml_list_size);
		for (blk_index = 0, bml_index = 0;  blk_index < total_blks; blk_index += bplmap, bml_index++)
			bml_newmap((blk_hdr_ptr_t)(bml_list + bml_index * bml_size),
					bml_size, csa->ti->curr_tn, csd->desired_db_format);
		if (!was_crit)
		{
			grab_crit_encr_cycle_sync(gv_cur_region, WS_59);
			csa->hold_onto_crit = TRUE;	/* need to do this AFTER grab_crit */
		}
		blk = get_dir_root();
		assert(blk < bplmap);
		csa->ti->free_blocks = total_blks - DIVIDE_ROUND_UP(total_blks, bplmap);
		bml_busy(blk, bml_list + SIZEOF(blk_hdr));
		csa->ti->free_blocks =  csa->ti->free_blocks - 1;
		dse_m_rest(blk, bml_list, bml_size, &csa->ti->free_blocks, TRUE);
		for (blk_index = 0, bml_index = 0;  blk_index < total_blks; blk_index += bplmap, bml_index++)
		{
			t_begin_crit(ERR_DSEFAIL);
			CHECK_TN(csa, csd, csd->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
			CWS_RESET;
			CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
			assert(csa->ti->early_tn == csa->ti->curr_tn);
			blk_ptr = bml_list + (bml_index * bml_size);
			blkhist.blk_num = blk_index;
			if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
				RTS_ERROR_CSA_ABT(csa, VARLSTCNT(3) ERR_DSEBLKRDFAIL, 1, blkhist.blk_num);
			BLK_INIT(bs_ptr, bs1);
			BLK_SEG(bs_ptr, blk_ptr + SIZEOF(blk_hdr), bml_size - SIZEOF(blk_hdr));
			BLK_FINI(bs_ptr, bs1);
			t_write(&blkhist, bs1, 0, 0, LCL_MAP_LEVL, TRUE, FALSE, GDS_WRITE_KILLTN);
			BUILD_AIMG_IF_JNL_ENABLED(csd, csa->ti->curr_tn);
			t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);
		}
		/* Fill in master map */
		for (blk_index = 0, bml_index = 0;  blk_index < total_blks; blk_index += bplmap, bml_index++)
		{
			/* (total_blks - blk_index) is used to determine the number of blks in the last lmap of the DB
			 * so the value should never be larger then BLKS_PER_LMAP and thus fit in a int4
			 */
			assert((blk_index + bplmap <= total_blks) || (BLKS_PER_LMAP >= (total_blks - blk_index)));
			blks_in_bitmap = (blk_index + bplmap <= total_blks) ? bplmap : (int4)(total_blks - blk_index);
			assert(1 < blks_in_bitmap);	/* the last valid block in the database should never be a bitmap block */
			if (NO_FREE_SPACE != bml_find_free(0, (bml_list + bml_index * bml_size) + SIZEOF(blk_hdr), blks_in_bitmap))
				bit_set(blk_index / bplmap, csa->bmm);
			else
				bit_clear(blk_index / bplmap, csa->bmm);
			if (blk_index > csa->nl->highest_lbm_blk_changed)
				csa->nl->highest_lbm_blk_changed = blk_index;
		}
		if (!was_crit)
		{
			csa->hold_onto_crit = FALSE;	/* need to do this before the rel_crit */
			rel_crit(gv_cur_region);
		}
		if (unhandled_stale_timer_pop)
			process_deferred_stale();
		free(bml_list);
		csd->kill_in_prog = csd->abandoned_kills = 0;
		return;
	}
	if (CLI_PRESENT == cli_present("FREE"))
	{
		if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSENOBML, DSEBLKCUR)))		/* WARNING: assignment */
			return;
		bml_blk = blk / bplmap * bplmap;
		bm_setmap(bml_blk, blk, FALSE);
		return;
	}
	if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSEBMLOK, DSEBLKCUR)))		/* WARNING: assignment */
		return;
	if (CLI_PRESENT == cli_present("BUSY"))
	{
		bml_blk = blk / bplmap * bplmap;
		TREF(tqread_grab_bml) = (dba_bg == csd->acc_meth)	/* Marking block as busy; avoid restarts */
			&& (DEFAULT_BITMAP_PREPIN == csd->nobitmap_prepin);
		bm_setmap(bml_blk, blk, TRUE);
		return;
	}
	if (CLI_PRESENT == cli_present("MASTER"))
	{
		if (!was_crit)
			grab_crit_encr_cycle_sync(gv_cur_region, WS_60);
		bml_blk = blk / bplmap * bplmap;
		if (dba_mm == csd->acc_meth)
			bp = MM_BASE_ADDR(csa) + (off_t)bml_blk * blk_size;
		else
		{
			assert(dba_bg == csd->acc_meth);
			if (!(bp = t_qread(bml_blk, &dummy_int, &dummy_cr)))
				RTS_ERROR_CSA_ABT(csa, VARLSTCNT(3) ERR_DSEBLKRDFAIL, 1, bml_blk);
		}
		if ((csa->ti->total_blks / bplmap) * bplmap == bml_blk)
			total_blks = (csa->ti->total_blks - bml_blk);
		else
			total_blks = bplmap;
		if (NO_FREE_SPACE == bml_find_free(0, bp + SIZEOF(blk_hdr), total_blks))
			bit_clear(bml_blk / bplmap, csa->bmm);
		else
			bit_set(bml_blk / bplmap, csa->bmm);
		if (bml_blk > csa->nl->highest_lbm_blk_changed)
			csa->nl->highest_lbm_blk_changed = bml_blk;
		if (!was_crit)
			rel_crit(gv_cur_region);
		return;
	}
	MEMCPY_LIT(util_buff, "!/Block ");
	util_len = SIZEOF("!/Block ") - 1;
	util_len += i2hexl_nofill(blk, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
	memcpy(&util_buff[util_len], " is marked !AD in its local bit map.!/",
		SIZEOF(" is marked !AD in its local bit map.!/") - 1);
	util_len += SIZEOF(" is marked !AD in its local bit map.!/") - 1;
	util_buff[util_len] = 0;
	if (!was_crit)
		grab_crit_encr_cycle_sync(gv_cur_region, WS_61);
	util_out_print(util_buff, TRUE, 4, dse_is_blk_free(blk, &dummy_int, &dummy_cr) ? "free" : "busy");
	if (!was_crit)
		rel_crit(gv_cur_region);
	return;
}
