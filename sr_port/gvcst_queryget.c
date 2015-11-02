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

#include "gtm_string.h"

#include "stringpool.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cdb_sc.h"
#include "copy.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "gdscc.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "gdskill.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* needed for T_BEGIN_READ_NONTP_OR_TP macro */

#include "gvcst_protos.h"	/* for gvcst_queryget,gvcst_search,gvcst_rtsib,gvcst_search_blk prototype */
#include "gvcst_expand_key.h"
#include "t_begin.h"
#include "t_retry.h"
#include "t_end.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF spdesc		stringpool;
GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;

error_def(ERR_GVQUERYGETFAIL);

boolean_t gvcst_queryget(mval *val)
{
	blk_hdr_ptr_t	bp;
	boolean_t	found, two_histories;
	enum cdb_sc	status;
	int		rsiz, key_size, data_len;
	rec_hdr_ptr_t	rp;
	srch_blk_status	*bh;
	srch_hist	*rt_history;
	unsigned short	temp_ushort;
	DEBUG_ONLY(unsigned char *save_strp = NULL);

	T_BEGIN_READ_NONTP_OR_TP(ERR_GVQUERYGETFAIL);
	assert((CDB_STAGNATE > t_tries) || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
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
			/* !found indicates that the end of tree has been reached (see call to
			 *  gvcst_rtsib).  If there is no more tree, don't bother doing expansion.
			 */
			if (found)
			{
				status = gvcst_expand_key((blk_hdr_ptr_t)bh->buffaddr, (int4)((sm_uc_ptr_t)rp - bh->buffaddr),
						gv_altkey);
				if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				}
				key_size = gv_altkey->end + 1;
				GET_RSIZ(rsiz, rp);
				data_len = rsiz + rp->cmpc - SIZEOF(rec_hdr) - key_size;
				if (data_len < 0  || (sm_uc_ptr_t)rp + rsiz > (sm_uc_ptr_t)bp + ((blk_hdr_ptr_t)bp)->bsiz)
				{
					assert(CDB_STAGNATE > t_tries);
					t_retry(cdb_sc_rmisalign1);
					continue;
				}
				ENSURE_STP_FREE_SPACE(data_len);
				DEBUG_ONLY (
				if (!save_strp)
					save_strp = stringpool.free);
				assert(stringpool.top - stringpool.free >= data_len);
				memcpy(stringpool.free, (sm_uc_ptr_t)rp + rsiz - data_len, data_len);
				/* Assumption: t_end/tp_hist will never cause stp_gcol() call */
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
			if (found)
			{
				DEBUG_ONLY(assert(save_strp == stringpool.free));
				/* Process val first. Already copied to string pool. */
				val->mvtype = MV_STR;
				val->str.addr = (char *)stringpool.free;
				val->str.len = data_len;
				stringpool.free += data_len;
				INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_get, 1);
			}
			return found;
		}
		t_retry(status);
	}
}
