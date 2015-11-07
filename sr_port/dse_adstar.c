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

void dse_adstar(void)
{
	uchar_ptr_t	lbp, b_top;
	block_id	blk;
	blk_segment	*bs1, *bs_ptr;
	int4		blk_seg_cnt, blk_size;
	short		rsize;
	int		tmp_cmpc;
	srch_blk_status	blkhist;

	error_def(ERR_DBRDONLY);
	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DSEFAIL);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&blk))
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
	if (!cli_get_hex("POINTER", (uint4 *)&blk))
		return;
	t_begin_crit(ERR_DSEFAIL);
	blk_size = cs_addrs->hdr->blk_size;
	blkhist.blk_num = patch_curr_blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	lbp = (uchar_ptr_t)malloc(blk_size);
	memcpy(lbp, blkhist.buffaddr, blk_size);

	if (!((blk_hdr_ptr_t)lbp)->levl)
	{
		util_out_print("Error: cannot add a star record to a data block.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	if (((blk_hdr_ptr_t) lbp)->bsiz > blk_size)
		b_top = lbp + blk_size;
	else if (((blk_hdr_ptr_t) lbp)->bsiz < SIZEOF(blk_hdr))
		b_top = lbp + SIZEOF(blk_hdr);
	else
		b_top = lbp + ((blk_hdr_ptr_t) lbp)->bsiz;
	if (b_top - lbp > blk_size - SIZEOF(rec_hdr) - SIZEOF(block_id))
	{
		util_out_print("Error:  not enough free space in block for a star record.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	rsize = SIZEOF(rec_hdr) + SIZEOF(block_id);
	PUT_SHORT(&((rec_hdr_ptr_t)b_top)->rsiz, rsize);
	SET_CMPC((rec_hdr_ptr_t)b_top, 0);
	PUT_LONG((block_id_ptr_t)(b_top + SIZEOF(rec_hdr)), blk);
	((blk_hdr_ptr_t)lbp)->bsiz += (unsigned int)(SIZEOF(rec_hdr) + SIZEOF(block_id));

	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + SIZEOF(blk_hdr), (int)((blk_hdr_ptr_t)lbp)->bsiz - SIZEOF(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		util_out_print("Error: bad blk build.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	t_write(&blkhist, (unsigned char *)bs1, 0, 0, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
	BUILD_AIMG_IF_JNL_ENABLED(cs_data, cs_addrs->ti->curr_tn);
	t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);

	free(lbp);
	return;
}
