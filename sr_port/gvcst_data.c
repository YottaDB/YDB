/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "gdscc.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "gdskill.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* needed for T_BEGIN_READ_NONTP_OR_TP macro */

#include "t_end.h"		/* prototypes */
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search,gvcst_search_blk,gvcst_data prototype */

GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;

mint	gvcst_data(void)
{
	blk_hdr_ptr_t	bp;
	boolean_t	do_rtsib;
	enum cdb_sc	status;
	mint		val;
	rec_hdr_ptr_t	rp;
	unsigned short	match, rsiz;
	srch_blk_status *bh;
	srch_hist	*rt_history;
	sm_uc_ptr_t	b_top;

	assert((gv_target->root < cs_addrs->ti->total_blks) || dollar_tlevel);
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVDATAFAIL);
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	for (;;)
	{
		/* The following code is duplicated in gvcst_dataget. Any changes here might need to be reflected there as well */
		rt_history = gv_target->alt_hist;
		rt_history->h[0].blk_num = 0;
		if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))
		{
			t_retry(status);
			continue;
		}
		bh = gv_target->hist.h;
		bp = (blk_hdr_ptr_t)bh->buffaddr;
		rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
		b_top = bh->buffaddr + bp->bsiz;
		match = bh->curr_rec.match;
		do_rtsib = FALSE;
		if (gv_currkey->end + 1 == match)
		{
			val = 1;
			GET_USHORT(rsiz, &rp->rsiz);
			rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rsiz);
			if ((sm_uc_ptr_t)rp > b_top)
			{
				t_retry(cdb_sc_rmisalign);
				continue;
			} else if ((sm_uc_ptr_t)rp == b_top)
				do_rtsib = TRUE;
			else if (rp->cmpc >= gv_currkey->end)
				val += 10;
		} else if (match >= gv_currkey->end)
			val = 10;
		else
		{
			val = 0;
			if (rp == (rec_hdr_ptr_t)b_top)
				do_rtsib = TRUE;
		}
		if (do_rtsib && (cdb_sc_endtree != (status = gvcst_rtsib(rt_history, 0))))
		{
			if ((cdb_sc_normal != status) || (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, rt_history->h))))
			{
				t_retry(status);
				continue;
			}
			if (rt_history->h[0].curr_rec.match >= gv_currkey->end)
			{
				assert(1 >= val);
				val += 10;
			}
		}
		if (!dollar_tlevel)
		{
			if ((trans_num)0 == t_end(&gv_target->hist, 0 == rt_history->h[0].blk_num ? NULL : rt_history,
				TN_NOT_SPECIFIED))
				continue;
		} else
		{
			status = tp_hist(0 == rt_history->h[0].blk_num ? NULL : rt_history);
			if (cdb_sc_normal != status)
			{
				t_retry(status);
				continue;
			}
		}
		INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_data, 1);
		return val;
	}
}
