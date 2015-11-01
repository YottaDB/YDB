/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "muprec.h"
#include "util.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"

GBLREF gd_region        *gv_cur_region;
GBLREF char		*update_array, *update_array_ptr;
GBLREF int		update_array_size;
GBLREF srch_hist	dummy_hist;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF cw_set_element   cw_set[];
GBLREF unsigned char    *non_tp_jfb_buff_ptr;
/* Modified on the similar lines of dse AIMG record logic, needed for recover to write journal records */

void mur_put_aimg_rec(jnl_record *rec)
{
	sm_uc_ptr_t	aimg_blk_ptr, bp;
	int4		blk_seg_cnt, blk_size;
	block_id	blk_num;
	blk_segment	*bs1, *bs_ptr;
	cw_set_element  *cse;
	void		gvcst_blk_build(), t_begin_crit();
	int		t_end();

	error_def(ERR_MURAIMGFAIL);

	/* Applying an after image record should use t_begin/t_end mechanisms instead of just copying over
	 * the aimg block into the t_qread buffer. This is because there are lots of other things like
	 * making the cache-record become dirty in case of BG and some others to do in case of MM.
	 * Therefore, it is best to call t_end().
	 */
	assert(update_array);
	/* reset new block mechanism */
	update_array_ptr = update_array;
	assert(!cs_addrs->now_crit);

	t_begin_crit(ERR_MURAIMGFAIL);
	blk_num = rec->val.jrec_aimg.blknum;
	blk_size = cs_addrs->hdr->blk_size;
	if (NULL == (bp = t_qread(blk_num, &dummy_hist.h[0].cycle, &dummy_hist.h[0].cr)))
		GTMASSERT;
	aimg_blk_ptr = (sm_uc_ptr_t)&rec->val.jrec_aimg.blk_contents[0];
	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)aimg_blk_ptr + sizeof(blk_hdr), (int)((blk_hdr_ptr_t)aimg_blk_ptr)->bsiz - sizeof(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		util_out_print("Error: bad blk build.", TRUE);
		T_ABORT(gv_cur_region, cs_addrs);
		return;
	}
	t_write(blk_num, (unsigned char *)bs1, 0, 0, (sm_uc_ptr_t)bp, ((blk_hdr_ptr_t)aimg_blk_ptr)->levl, TRUE, FALSE);
	if (JNL_ENABLED(cs_data))
	{
		cse = (cw_set_element *)(&cw_set[0]);
		cse->new_buff = non_tp_jfb_buff_ptr;
		gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, cs_addrs->ti->curr_tn);
		cse->done = TRUE;
	}
	/* Call t_end till it succeeds or aborts (error will be reported) */
	while (t_end(&dummy_hist, 0) == 0)
		;
	assert(!cs_addrs->now_crit);
	return;
}
