/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "cli.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "skan_offset.h"
#include "skan_rnum.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "gvcst_blk_build.h"
#include "util.h"
#include "t_abort.h"
#include "gtmmsg.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF cw_set_element	cw_set[];
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF srch_hist	dummy_hist;
GBLREF unsigned short	patch_comp_count;
GBLREF uint4		update_array_size;

error_def(ERR_AIMGBLKFAIL);
error_def(ERR_DBRDONLY);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEFAIL);

void dse_chng_rhead(void)
{
	blk_segment	*bs1, *bs_ptr;
	block_id	blk;
	boolean_t	chng_rec;
	int4		blk_seg_cnt, blk_size;
	rec_hdr		new_rec;
	sm_uc_ptr_t	bp, b_top, cp, rp;
	srch_blk_status	blkhist;
	uint4		x;

        if (gv_cur_region->read_only)
                rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSENOBML, DSEBLKCUR)))		/* WARNING: assignment */
		return;
	t_begin_crit(ERR_DSEFAIL);
	blkhist.blk_num = blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	bp = blkhist.buffaddr;
	blk_size = cs_addrs->hdr->blk_size;
	chng_rec = FALSE;
	b_top = bp + ((blk_hdr_ptr_t)bp)->bsiz;
	if (((blk_hdr_ptr_t)bp)->bsiz > blk_size || ((blk_hdr_ptr_t)bp)->bsiz < SIZEOF(blk_hdr))
		chng_rec = TRUE;	/* force rewrite to correct size */
	if (CLI_PRESENT == cli_present("RECORD"))
	{
		if (!(rp = skan_rnum(bp, FALSE)))
		{
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
	} else if (!(rp = skan_offset(bp, FALSE)))
	{
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	GET_SHORT(new_rec.rsiz, &((rec_hdr_ptr_t)rp)->rsiz);
	SET_CMPC(&new_rec, EVAL_CMPC((rec_hdr_ptr_t)rp));
	if (CLI_PRESENT == cli_present("CMPC"))
	{
		if (!cli_get_hex("CMPC", &x))
		{
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		if (x >= MAX_KEY_SZ)
		{
			util_out_print("Error: invalid cmpc.",TRUE);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		if (x > patch_comp_count)
			util_out_print("Warning:  specified compression count is larger than the current expanded key size.", TRUE);
		SET_CMPC(&new_rec, x);
		chng_rec = TRUE;
	}
	if (CLI_PRESENT == cli_present("RSIZ"))
	{
		if (!cli_get_hex("RSIZ", &x))
		{
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		if (x < SIZEOF(rec_hdr) || x > blk_size)
		{
			util_out_print("Error: invalid rsiz.", TRUE);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		new_rec.rsiz = x;
		chng_rec = TRUE;
	}
	if (chng_rec)
	{
		BLK_INIT(bs_ptr, bs1);
		cp = bp;
		cp += SIZEOF(blk_hdr);
		if (chng_rec)
		{
			BLK_SEG(bs_ptr, cp, rp - cp);
			BLK_SEG(bs_ptr, (uchar_ptr_t)&new_rec, SIZEOF(rec_hdr));
			cp = rp + SIZEOF(rec_hdr);
		}
		if (b_top - cp)
			BLK_SEG(bs_ptr, cp, b_top - cp);
		if (!BLK_FINI(bs_ptr, bs1))
		{
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_AIMGBLKFAIL, 3, blk, DB_LEN_STR(gv_cur_region));
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		t_write(&blkhist, (unsigned char *)bs1, 0, 0, ((blk_hdr_ptr_t)bp)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
		BUILD_AIMG_IF_JNL_ENABLED(cs_data, cs_addrs->ti->curr_tn);
		t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);
	}
	return;
}
