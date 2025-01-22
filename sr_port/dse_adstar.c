/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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
#include "gtmmsg.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF cw_set_element	cw_set[];
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF srch_hist	dummy_hist;
GBLREF uint4		update_array_size;

error_def(ERR_AIMGBLKFAIL);
error_def(ERR_DBRDONLY);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEFAIL);
error_def(ERR_DSEINVALBLKID);

void dse_adstar(void)
{
	blk_segment	*bs1, *bs_ptr;
	block_id	blk, blk_ptr;
	boolean_t	long_blk_id;
	int4		blk_seg_cnt, blk_size;
	long		blk_id_size;
	short		rsize;
	srch_blk_status	blkhist;
	uchar_ptr_t	b_top, lbp;

	DSE_DB_IS_TOO_OLD(cs_addrs, cs_data, gv_cur_region);
	if (gv_cur_region->read_only)
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSENOBML, DSEBLKCUR)))		/* WARNING: assignment */
		return;
	if (CLI_PRESENT != cli_present("POINTER"))
	{
		util_out_print("Error: block pointer must be specified.", TRUE);
		return;
	}
	if (BADDSEBLK == (blk_ptr = dse_getblk("POINTER", DSENOBML, DSEBLKNOCUR)))		/* WARNING: assignment */
		return;
	t_begin_crit(ERR_DSEFAIL);
	blk_size = cs_addrs->hdr->blk_size;
	blkhist.blk_num = blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	lbp = (uchar_ptr_t)malloc(blk_size);
	memcpy(lbp, blkhist.buffaddr, blk_size);
	if (!((blk_hdr_ptr_t)lbp)->levl)
	{
		util_out_print("Error: cannot add a star record to a data block.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	if (((blk_hdr_ptr_t)lbp)->bver > BLK_ID_32_VER) /* Check blk version to see if using 32 or 64 bit block_id */
	{
#		ifdef BLK_NUM_64BIT
		long_blk_id = TRUE;
		blk_id_size = SIZEOF(block_id_64);
#		else
		t_abort(gv_cur_region, cs_addrs);
		free(lbp);
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#		endif
	} else
	{
		long_blk_id = FALSE;
		blk_id_size = SIZEOF(block_id_32);
	}
	if (((blk_hdr_ptr_t)lbp)->bsiz > blk_size)
		b_top = lbp + blk_size;
	else if (((blk_hdr_ptr_t)lbp)->bsiz < SIZEOF(blk_hdr))
		b_top = lbp + SIZEOF(blk_hdr);
	else
		b_top = lbp + ((blk_hdr_ptr_t)lbp)->bsiz;
	if ((b_top - lbp) > (blk_size - SIZEOF(rec_hdr) - blk_id_size))
	{
		util_out_print("Error: not enough free space in block for a star record.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	assert((SIZEOF(rec_hdr) + blk_id_size) == (short)(SIZEOF(rec_hdr) + blk_id_size));
	rsize = (short)(SIZEOF(rec_hdr) + blk_id_size);
	PUT_SHORT(&((rec_hdr_ptr_t)b_top)->rsiz, rsize);
	SET_CMPC((rec_hdr_ptr_t)b_top, 0);
	if (long_blk_id == TRUE)
	{
#		ifdef BLK_NUM_64BIT
		PUT_BLK_ID_64((b_top + SIZEOF(rec_hdr)), blk_ptr);
#		else
		t_abort(gv_cur_region, cs_addrs);
		free(lbp);
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#		endif
	} else
	{
		if (blk_ptr != (block_id_32)blk_ptr){
			util_out_print("Error: attempting to add a 64-bit pointer to pre-V7 block.", TRUE);
			free(lbp);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		PUT_BLK_ID_32((b_top + SIZEOF(rec_hdr)), blk_ptr);
	}
	((blk_hdr_ptr_t)lbp)->bsiz += (unsigned int)(SIZEOF(rec_hdr) + blk_id_size);
	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + SIZEOF(blk_hdr), (int)((blk_hdr_ptr_t)lbp)->bsiz - SIZEOF(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_AIMGBLKFAIL, 3, &blk, DB_LEN_STR(gv_cur_region));
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
