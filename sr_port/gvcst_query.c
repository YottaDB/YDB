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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cdb_sc.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "gdscc.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "gdskill.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* needed for T_BEGIN_READ_NONTP_OR_TP macro */

#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_expand_key.h"
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search,gvcst_search_blk,gvcst_query prototype */

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;

bool gvcst_query(void)
{
	boolean_t	found, two_histories;
	enum cdb_sc	status;
	blk_hdr_ptr_t	bp;
	rec_hdr_ptr_t	rp;
	unsigned char	*c1, *c2;
	srch_blk_status	*bh;
	srch_hist	*rt_history;

	T_BEGIN_READ_NONTP_OR_TP(ERR_GVQUERYFAIL);
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	for (;;)
	{
		two_histories = FALSE;
		if (cdb_sc_normal == (status = gvcst_search(gv_currkey, 0)))
		{
			found = TRUE;
			bh = &gv_target->hist.h[0];
			rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
			bp = (blk_hdr_ptr_t)bh->buffaddr;
			if (rp >= (rec_hdr_ptr_t)CST_TOB(bp))
			{
				two_histories = TRUE;
				rt_history = gv_target->alt_hist;
				status = gvcst_rtsib(rt_history, 0);
				if (cdb_sc_endtree == status)		/* end of tree */
				{
					found = FALSE;
					two_histories = FALSE;		/* second history not valid */
				} else  if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				} else
				{
					bh = &rt_history->h[0];
					if (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, bh)))
					{
						t_retry(status);
						continue;
					}
					rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
					bp = (blk_hdr_ptr_t)bh->buffaddr;
				}
			}
			if (found)
			{	/* !found indicates that the end of tree has been reached (see call to
				 *  gvcst_rtsib).  If there is no more tree, don't bother doing expansion.
				 */
				status = gvcst_expand_key((blk_hdr_ptr_t)bh->buffaddr, (int4)((sm_uc_ptr_t)rp - bh->buffaddr),
						gv_altkey);
				if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				}
			}
			if (!dollar_tlevel)
			{
				if ((trans_num)0 == t_end(&gv_target->hist, !two_histories ? NULL : rt_history, TN_NOT_SPECIFIED))
					continue;
			} else
			{
				status = tp_hist(!two_histories ? NULL : rt_history);
				if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				}
		    	}
			assert(cs_data == cs_addrs->hdr);
			INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_query, 1);
			if (found)
			{
				c1 = &gv_altkey->base[0];
				c2 = &gv_currkey->base[0];
				for (;  *c2;)
				{
					if (*c2++ != *c1++)
						break;
				}
				if (!*c2 && !*c1)
				{
					return TRUE;
				}
			}
			return FALSE;
		}
		t_retry(status);
	}
}
