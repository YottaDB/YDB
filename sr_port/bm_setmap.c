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

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdsblkops.h"	/* for CHECK_AND_RESET_UPDATE_ARRAY macro */

/* Include prototypes */
#include "t_qread.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin_crit.h"
#include "t_write_map.h"
#include "gvcst_map_build.h"
#include "mm_read.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	cw_set_element		cw_set[];
GBLREF	unsigned char		rdfail_detail;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;

void bm_setmap(block_id bml, block_id blk, int4 busy)
{
	sm_uc_ptr_t	bmp;
	trans_num	ctn;
	srch_hist	alt_hist;
	srch_blk_status	blkhist; /* block-history to fill in for t_write_map which uses "blk_num", "buffaddr", "cr", "cycle" */
	cw_set_element  *cse;
	int		lbm_status;	/* local bitmap status of input "blk" i.e. BUSY or FREE or RECYCLED  */
	int4		reference_cnt;
	uint4		bitnum;

	error_def(ERR_DSEFAIL);

	t_begin_crit(ERR_DSEFAIL);
	ctn = cs_addrs->ti->curr_tn;
	if (!(bmp = t_qread(bml, &blkhist.cycle, &blkhist.cr)))
		t_retry((enum cdb_sc)rdfail_detail);
	blkhist.blk_num = bml;
	blkhist.buffaddr = bmp;
	alt_hist.h[0].blk_num = 0;	/* Need for calls to T_END for bitmaps */
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	bitnum = blk - bml;
	/* Find out current status in order to determine if there is going to be a state transition */
	assert(ROUND_DOWN2(blk, cs_data->bplmap) == bml);
	GET_BM_STATUS(bmp, bitnum, lbm_status);
	switch(lbm_status)
	{
		case BLK_BUSY:
			reference_cnt = busy ? 0 : -1;
			break;
		case BLK_FREE:
		case BLK_MAPINVALID:
		case BLK_RECYCLED:
			assert(BLK_MAPINVALID != lbm_status);
			reference_cnt = busy ? 1 : 0;
			break;
		default:
			assert(FALSE);
			break;
	}
	if (reference_cnt)
	{	/* Initialize update array with non-zero bitnum only if reference_cnt is non-zero. */
		assert(bitnum);
		*((block_id_ptr_t)update_array_ptr) = bitnum;
		update_array_ptr += SIZEOF(block_id);
	}
	/* Terminate update array unconditionally with zero bitnum. */
	*((block_id_ptr_t)update_array_ptr) = 0;
	update_array_ptr += SIZEOF(block_id);
	t_write_map(&blkhist, (uchar_ptr_t)update_array, ctn, reference_cnt);
	if (JNL_ENABLED(cs_data))
        {
                cse = (cw_set_element *)(&cw_set[0]);
                cse->new_buff = (unsigned char *)non_tp_jfb_ptr->buff;
                memcpy(cse->new_buff, bmp, ((blk_hdr_ptr_t)bmp)->bsiz);
                gvcst_map_build((uint4 *)cse->upd_addr, (uchar_ptr_t)cse->new_buff, cse, cs_addrs->ti->curr_tn);
                cse->done = TRUE;
        }
	/* Call t_end till it succeeds or aborts (error will be reported) */
	while ((trans_num)0 == t_end(&alt_hist, NULL, TN_NOT_SPECIFIED))
		;
	return;
}
