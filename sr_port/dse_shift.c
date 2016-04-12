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

#include "gtm_string.h"


#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "dse.h"
#include "cli.h"
#include "filestruct.h"
#include "jnl.h"
#include "util.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "gvcst_blk_build.h"
#include "t_abort.h"
#include "gtmmsg.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF cw_set_element   cw_set[];
GBLREF gd_region        *gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF srch_hist	dummy_hist;
GBLREF uint4		update_array_size;

error_def(ERR_AIMGBLKFAIL);
error_def(ERR_DBRDONLY);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEFAIL);

void dse_shift(void)
{
	blk_segment	*bs1, *bs_ptr;
	block_id	blk;
	boolean_t	forward;
	int4		blk_seg_cnt, blk_size, size;
	sm_uc_ptr_t	bp;
	srch_blk_status	blkhist;
	uchar_ptr_t	lbp;
	uint4		offset, shift;

        if (gv_cur_region->read_only)
                rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSENOBML, DSEBLKCUR)))		/* WARNING: assignment */
		return;
	if (CLI_PRESENT != cli_present("OFFSET"))
	{
		util_out_print("Error:  offset must be specified.", TRUE);
		return;
	}
	if (!cli_get_hex("OFFSET", &offset))
		return;
	shift = 0;
	if (CLI_PRESENT == cli_present("FORWARD"))
	{
		if (!cli_get_hex("FORWARD", &shift))
			return;
		forward = TRUE;
		lbp = (unsigned char *)malloc((size_t)shift);
	} else if (CLI_PRESENT == cli_present("BACKWARD"))
	{
		if (!cli_get_hex("BACKWARD", &shift))
			return;
                if (shift > offset)
		{
		  	util_out_print("Error: shift greater than offset not allowed.", TRUE);
			return;
                }
		forward = FALSE;
		lbp = (unsigned char *)0;
	}
	if (!shift)
	{
		util_out_print("Error:  must specify amount to shift.", TRUE);
		if (lbp)
			free(lbp);
		return;
	}
	blk_size = cs_addrs->hdr->blk_size;
	t_begin_crit(ERR_DSEFAIL);
	blkhist.blk_num = blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
	{
		if (lbp)
			free(lbp);
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	}
	bp = blkhist.buffaddr;
	size = ((blk_hdr *)bp)->bsiz;
	if (size < 0)
		size = 0;
	else if (size > cs_addrs->hdr->blk_size)
		size = cs_addrs->hdr->blk_size;
	if (offset < SIZEOF(blk_hdr) || offset > size)
	{
		util_out_print("Error:  offset not in range of block.", TRUE);
		t_abort(gv_cur_region, cs_addrs);
		if (lbp)
			free(lbp);
		return;
	}
	BLK_INIT(bs_ptr, bs1);
	if (forward)
	{
		if (shift + size >= cs_addrs->hdr->blk_size)
		{
			util_out_print("Error:  block not large enough to accommodate shift.", TRUE);
			t_abort(gv_cur_region, cs_addrs);
			if (lbp)
				free(lbp);
			return;
		}
		memset(lbp, 0, shift);
		BLK_SEG(bs_ptr, bp + SIZEOF(blk_hdr), offset - SIZEOF(blk_hdr));
		BLK_SEG(bs_ptr, lbp, shift);
		if (size - offset)
			BLK_SEG(bs_ptr, bp + offset, size - offset);
	} else
	{
		if (shift > offset)
			shift = offset - SIZEOF(blk_hdr);
		if (offset - shift > SIZEOF(blk_hdr))
			BLK_SEG(bs_ptr, bp + SIZEOF(blk_hdr), offset - shift - SIZEOF(blk_hdr));
		if (size - offset)
			BLK_SEG(bs_ptr, bp + offset, size - offset);
	}
	if (!BLK_FINI(bs_ptr, bs1))
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_AIMGBLKFAIL, 3, blk, DB_LEN_STR(gv_cur_region));
		t_abort(gv_cur_region, cs_addrs);
		if (lbp)
			free(lbp);
		return;
	}
	t_write(&blkhist, (unsigned char *)bs1, 0, 0, ((blk_hdr_ptr_t)bp)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
	BUILD_AIMG_IF_JNL_ENABLED(cs_data, cs_addrs->ti->curr_tn);
	t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);
	return;
}
