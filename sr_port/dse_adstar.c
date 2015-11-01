/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include "cli.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "dse.h"


/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "gvcst_blk_build.h"
#include "util.h"
#include "t_abort.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF gd_region        *gv_cur_region;
GBLREF uint4		update_array_size;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF srch_hist	dummy_hist;
GBLREF gd_addr		*gd_header;
GBLREF block_id		patch_curr_blk;
GBLREF cw_set_element   cw_set[];
GBLREF unsigned char    *non_tp_jfb_buff_ptr;

void dse_adstar(void)
{
	sm_uc_ptr_t	bp;
	uchar_ptr_t	lbp, b_top;
	block_id	blk;
	blk_segment	*bs1, *bs_ptr;
	int4		blk_seg_cnt, blk_size;
	short		rsize;
	cw_set_element  *cse;
	error_def(ERR_DSEFAIL);
	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DBRDONLY);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	assert(update_array);
	/* reset new block mechanism */
	update_array_ptr = update_array;

	if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if (!cli_get_hex("BLOCK", &blk))
			return;
		patch_curr_blk = blk;
	}
	if (patch_curr_blk < 0 || patch_curr_blk >= cs_addrs->ti->total_blks || !(patch_curr_blk % cs_addrs->hdr->bplmap))
	{
		util_out_print("Error: invalid block number.", TRUE);
		return;
	}
	if (cli_present("POINTER") != CLI_PRESENT)
	{
		util_out_print("Error: block pointer must be specified.", TRUE);
		return;
	}
	if (!cli_get_hex("POINTER", &blk))
		return;
	t_begin_crit(ERR_DSEFAIL);
	blk_size = cs_addrs->hdr->blk_size;
	if(!(bp = t_qread(patch_curr_blk, &dummy_hist.h[0].cycle, &dummy_hist.h[0].cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	lbp = (uchar_ptr_t)malloc(blk_size);
	memcpy(lbp, bp, blk_size);

	if (!((blk_hdr_ptr_t)lbp)->levl)
	{
		util_out_print("Error: cannot add a star record to a data block.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	if (((blk_hdr_ptr_t) lbp)->bsiz > blk_size)
		b_top = lbp + blk_size;
	else if (((blk_hdr_ptr_t) lbp)->bsiz < sizeof(blk_hdr))
		b_top = lbp + sizeof(blk_hdr);
	else
		b_top = lbp + ((blk_hdr_ptr_t) lbp)->bsiz;
	if (b_top - lbp > blk_size - sizeof(rec_hdr) - sizeof(block_id))
	{
		util_out_print("Error:  not enough free space in block for a star record.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	rsize = sizeof(rec_hdr) + sizeof(block_id);
	PUT_SHORT(&((rec_hdr_ptr_t)b_top)->rsiz, rsize);
	((rec_hdr_ptr_t) b_top)->cmpc = 0;
	*((block_id_ptr_t)(b_top + sizeof(rec_hdr))) = blk;
	((blk_hdr_ptr_t)lbp)->bsiz += sizeof(rec_hdr) + sizeof(block_id);

	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + sizeof(blk_hdr), (int)((blk_hdr_ptr_t)lbp)->bsiz - sizeof(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		util_out_print("Error: bad blk build.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	t_write(patch_curr_blk, (unsigned char *)bs1, 0, 0, bp, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE);
	BUILD_AIMG_IF_JNL_ENABLED(cs_addrs, cs_data, non_tp_jfb_buff_ptr, cse);
	t_end(&dummy_hist, 0);

	free(lbp);
	return;
}
