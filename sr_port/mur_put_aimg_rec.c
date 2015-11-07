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
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "util.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_abort.h"
#include "t_end.h"
#include "gvcst_blk_build.h"
#include "t_begin_crit.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	uint4			update_array_size;	/* for the BLK_* macros */
GBLREF	srch_hist		dummy_hist;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	cw_set_element		cw_set[];
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
/* Modified on the similar lines of dse AIMG record logic, needed for recover to write journal records */

void mur_put_aimg_rec(jnl_record *rec)
{
	sm_uc_ptr_t	aimg_blk_ptr;
	int4		blk_seg_cnt, blk_size;
	blk_segment	*bs1, *bs_ptr;
	cw_set_element	*cse;
	srch_blk_status	blkhist;

	error_def(ERR_MURAIMGFAIL);

	/* Applying an after image record should use t_begin/t_end mechanisms instead of just copying over
	 * the aimg block into the t_qread buffer. This is because there are lots of other things like
	 * making the cache-record become dirty in case of BG and some others to do in case of MM.
	 * Therefore, it is best to call t_end().
	 */
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	assert(!cs_addrs->now_crit || cs_addrs->hold_onto_crit);

	t_begin_crit(ERR_MURAIMGFAIL);
	assert(cs_addrs->now_crit);
	blk_size = cs_addrs->hdr->blk_size;
	blkhist.blk_num = rec->jrec_aimg.blknum;
	if (NULL == (blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		GTMASSERT;
	aimg_blk_ptr = (sm_uc_ptr_t)&rec->jrec_aimg.blk_contents[0];
	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)aimg_blk_ptr + SIZEOF(blk_hdr), (int)((blk_hdr_ptr_t)aimg_blk_ptr)->bsiz - SIZEOF(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		util_out_print("Error: bad blk build.", TRUE);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	t_write(&blkhist, (unsigned char *)bs1, 0, 0, ((blk_hdr_ptr_t)aimg_blk_ptr)->levl, TRUE, FALSE, GDS_WRITE_PLAIN);
	if (JNL_ENABLED(cs_data))
	{
		cse = (cw_set_element *)(&cw_set[0]);
		cse->new_buff = (unsigned char *)non_tp_jfb_ptr->buff;
		gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, cs_addrs->ti->curr_tn);
		cse->done = TRUE;
	}
	/* Call t_end till it succeeds or aborts (error will be reported) */
	while ((trans_num)0 == t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED))
		;
	assert(!cs_addrs->now_crit || cs_addrs->hold_onto_crit);
	return;
}
