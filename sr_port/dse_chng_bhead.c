/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "cli.h"
#include "send_msg.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "process_deferred_stale.h"
#include "util.h"
#include "t_abort.h"
#include "gvcst_blk_build.h"	/* for the BUILD_AIMG_IF_JNL_ENABLED macro */
#include "gtmmsg.h"
#include "gtmcrypt.h"

GBLREF	cache_rec_ptr_t		cr_array[]; /* Maximum number of blocks that can be in transaction */
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	cw_set_element		cw_set[];
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	srch_hist		dummy_hist;
GBLREF	uint4			update_array_size;

error_def(ERR_AIMGBLKFAIL);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEFAIL);
error_def(ERR_DBRDONLY);

void dse_chng_bhead(void)
{
	blk_hdr			new_hdr;
	blk_segment		*bs1, *bs_ptr;
	block_id		blk;
	boolean_t		chng_blk, ismap, was_hold_onto_crit;
	int4			blk_seg_cnt, blk_size;	/* needed for BLK_INIT,BLK_SEG and BLK_FINI macros */
	int4			x;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	srch_blk_status		blkhist;
	trans_num		tn;
	uint4			mapsize;

	csa = cs_addrs;
        if (gv_cur_region->read_only)
                rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	chng_blk = FALSE;
	if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSEBMLOK, DSEBLKCUR)))		/* WARNING: assignment */
		return;
	csd = csa->hdr;
	assert(csd == cs_data);
	blk_size = csd->blk_size;
	ismap = IS_BITMAP_BLK(blk);
	mapsize = BM_SIZE(csd->bplmap);
	t_begin_crit(ERR_DSEFAIL);
	blkhist.blk_num = blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	new_hdr = *(blk_hdr_ptr_t)blkhist.buffaddr;
	if (CLI_PRESENT == cli_present("LEVEL"))
	{
		if (!cli_get_hex("LEVEL", (uint4 *)&x))
		{
			t_abort(gv_cur_region, csa);
			return;
		}
		if (ismap && (unsigned char)x != LCL_MAP_LEVL)
		{
			util_out_print("Error: invalid level for a bit map block.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		}
		if (!ismap && (x < 0 || x > MAX_BT_DEPTH + 1))
		{
			util_out_print("Error: invalid level.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		}
		new_hdr.levl = (unsigned char)x;
		chng_blk = TRUE;
		if (new_hdr.bsiz < SIZEOF(blk_hdr))
			new_hdr.bsiz = SIZEOF(blk_hdr);
		if (new_hdr.bsiz  > blk_size)
			new_hdr.bsiz = blk_size;
	}
	if (CLI_PRESENT == cli_present("BSIZ"))
	{
		if (!cli_get_hex("BSIZ", (uint4 *)&x))
		{
			t_abort(gv_cur_region, csa);
			return;
		}
		if (ismap && x != mapsize)
		{
			util_out_print("Error: invalid bsiz.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		} else if (x < SIZEOF(blk_hdr) || x > blk_size)
		{
			util_out_print("Error: invalid bsiz.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		}
		chng_blk = TRUE;
		new_hdr.bsiz = x;
	}
	if (!chng_blk)
		t_abort(gv_cur_region, csa);
	else
	{
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, blkhist.buffaddr + SIZEOF(new_hdr), new_hdr.bsiz - SIZEOF(new_hdr));
		if (!BLK_FINI(bs_ptr, bs1))
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_AIMGBLKFAIL, 3, blk, DB_LEN_STR(gv_cur_region));
			t_abort(gv_cur_region, csa);
			return;
		}
		t_write(&blkhist, (unsigned char *)bs1, 0, 0, new_hdr.levl, TRUE, FALSE, GDS_WRITE_KILLTN);
		BUILD_AIMG_IF_JNL_ENABLED(csd, csa->ti->curr_tn);
		t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);
	}
	if (CLI_PRESENT == cli_present("TN"))
	{
		if (!cli_get_hex64("TN", &tn))
			return;
		t_begin_crit(ERR_DSEFAIL);
		CHECK_TN(csa, csd, csd->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
		assert(csa->ti->early_tn == csa->ti->curr_tn);
		if (NULL == (blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
			t_abort(gv_cur_region, csa);
			return;
		}
		if (new_hdr.bsiz < SIZEOF(blk_hdr))
			new_hdr.bsiz = SIZEOF(blk_hdr);
		if (new_hdr.bsiz  > blk_size)
			new_hdr.bsiz = blk_size;
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, blkhist.buffaddr + SIZEOF(new_hdr), new_hdr.bsiz - SIZEOF(new_hdr));
		BLK_FINI(bs_ptr, bs1);
		t_write(&blkhist, (unsigned char *)bs1, 0, 0,
			((blk_hdr_ptr_t)blkhist.buffaddr)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
		/* Pass the desired tn as argument to bg_update/mm_update below */
		BUILD_AIMG_IF_JNL_ENABLED_AND_T_END_WITH_EFFECTIVE_TN(csa, csd, tn, &dummy_hist);
	}
	return;
}
