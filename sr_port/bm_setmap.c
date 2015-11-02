/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF char		*update_array, *update_array_ptr;
GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	rdfail_detail;
GBLREF unsigned char    *non_tp_jfb_buff_ptr;

void bm_setmap(block_id bml, block_id blk, int4 busy)
{
	sm_uc_ptr_t	bmp;
	trans_num	ctn;
	srch_hist	alt_hist;
	srch_blk_status	blkhist; /* block-history to fill in for t_write_map which uses "blk_num", "buffaddr", "cr", "cycle" */
	cw_set_element  *cse;

	error_def(ERR_DSEFAIL);

	t_begin_crit(ERR_DSEFAIL);
	ctn = cs_addrs->ti->curr_tn;
	if (!(bmp = t_qread(bml, &blkhist.cycle, &blkhist.cr)))
		t_retry((enum cdb_sc)rdfail_detail);
	blkhist.blk_num = bml;
	blkhist.buffaddr = bmp;
	alt_hist.h[0].blk_num = 0;	/* Need for calls to T_END for bitmaps */
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	*((block_id_ptr_t)update_array_ptr) = blk;
	update_array_ptr += sizeof(block_id);
	*((block_id_ptr_t)update_array_ptr) = 0;
	t_write_map(&blkhist, (uchar_ptr_t)update_array, ctn);
	cw_set[0].reference_cnt = busy;	/* Set the block busy */
	if (JNL_ENABLED(cs_data))
        {
                cse = (cw_set_element *)(&cw_set[0]);
                cse->new_buff = non_tp_jfb_buff_ptr;
                memcpy(non_tp_jfb_buff_ptr, bmp, ((blk_hdr_ptr_t)bmp)->bsiz);
                gvcst_map_build((uint4 *)cse->upd_addr, (uchar_ptr_t)cse->new_buff, cse, cs_addrs->ti->curr_tn);
                cse->done = TRUE;
        }
	/* Call t_end till it succeeds or aborts (error will be reported) */
	while ((trans_num)0 == t_end(&alt_hist, 0))
		;
	return;
}
