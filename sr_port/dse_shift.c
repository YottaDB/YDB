/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

GBLREF char		*update_array, *update_array_ptr;
GBLREF gd_region        *gv_cur_region;
GBLREF uint4		update_array_size;
GBLREF srch_hist	dummy_hist;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF gd_addr		*gd_header;
GBLREF block_id 	patch_curr_blk;
GBLREF cw_set_element   cw_set[];

void dse_shift(void)
{
	bool		forward;
	uint4		offset, shift;
	int4		size;
	sm_uc_ptr_t	bp;
	uchar_ptr_t	lbp;
	blk_segment	*bs1, *bs_ptr;
	int4		blk_seg_cnt, blk_size;
	srch_blk_status	blkhist;

	error_def(ERR_DBRDONLY);
	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DSEFAIL);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (patch_curr_blk < 0 || patch_curr_blk >= cs_addrs->ti->total_blks || !(patch_curr_blk % cs_addrs->hdr->bplmap))
	{
		util_out_print("Error: invalid block number.", TRUE);
		return;
	}
	if (cli_present("OFFSET") != CLI_PRESENT)
	{
		util_out_print("Error:  offset must be specified.", TRUE);
		return;
	}
	if (!cli_get_hex("OFFSET", &offset))
		return;
	shift = 0;
	if (cli_present("FORWARD") == CLI_PRESENT)
	{
		if (!cli_get_hex("FORWARD", &shift))
			return;
		forward = TRUE;
		lbp = (unsigned char *)malloc((size_t)shift);
	} else if (cli_present("BACKWARD") == CLI_PRESENT)
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
	blkhist.blk_num = patch_curr_blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
	{
		if (lbp)
			free(lbp);
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
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
			util_out_print("Error:  block not large enough to accomodate shift.", TRUE);
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
		util_out_print("Error: bad blk build.", TRUE);
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
