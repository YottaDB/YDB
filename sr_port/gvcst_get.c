/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

#include "t_end.h"		/* prototypes */
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_protos.h"	/* for gvcst_search,gvcst_get prototype */

GBLREF	gv_namehead	*gv_target;
GBLREF	gv_key		*gv_currkey;
GBLREF	spdesc		stringpool;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	short		dollar_tlevel;
GBLREF	unsigned int	t_tries;

#ifdef DEBUG
GBLREF	boolean_t	in_op_gvget;
GBLREF	boolean_t	ready2signal_gvundef;		/* TRUE if GET operation is about to signal a GVUNDEF */
#endif

bool	gvcst_get(mval *v)
{
	srch_blk_status	*s;
	enum cdb_sc	status;
	int		key_size, data_len;
	unsigned short	rsiz;
	rec_hdr_ptr_t	rp;
#	ifdef DEBUG
	boolean_t	in_op_gvget_lcl;
#	endif

	DEBUG_ONLY(
		/* Store global variable in_op_gvget in a local variable and reset the global right away to ensure that the global
		 * value does not incorrectly get carried over to the next call of gvcst_get (e.g. it if was from "op_fngvget").
		 */
		in_op_gvget_lcl = in_op_gvget;
		in_op_gvget = FALSE;
	)
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVGETFAIL);
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	for (;;)
	{
		if (cdb_sc_normal == (status = gvcst_search(gv_currkey, NULL)))
		{
			if ((key_size = gv_currkey->end + 1) == gv_target->hist.h[0].curr_rec.match)
			{
				rp = (rec_hdr_ptr_t)(gv_target->hist.h[0].buffaddr + gv_target->hist.h[0].curr_rec.offset);
				GET_USHORT(rsiz, &rp->rsiz);
				data_len = rsiz + rp->cmpc - SIZEOF(rec_hdr) - key_size;
				if (data_len < 0  || (sm_uc_ptr_t)rp + rsiz >
					gv_target->hist.h[0].buffaddr + ((blk_hdr_ptr_t)gv_target->hist.h[0].buffaddr)->bsiz)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_rmisalign1;
				} else
				{
					if (stringpool.top - stringpool.free < data_len)
						stp_gcol(data_len);
					assert(stringpool.top - stringpool.free >= data_len);
					memcpy(stringpool.free, (sm_uc_ptr_t)rp + rsiz - data_len, data_len);
					if (0 == dollar_tlevel)
					{
						if ((trans_num)0 == t_end(&gv_target->hist, NULL))
							continue;
					} else
					{
						status = tp_hist(NULL);
						if (cdb_sc_normal != status)
						{
							t_retry(status);
							continue;
						}
					}
					v->mvtype = MV_STR;
					v->str.addr = (char *)stringpool.free;
					v->str.len = data_len;
					stringpool.free += data_len;
					INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_get, 1);
					return TRUE;
				}
			} else
			{
				DEBUG_ONLY(ready2signal_gvundef = in_op_gvget_lcl;)
				if (0 == dollar_tlevel)
				{
					if ((trans_num)0 == t_end(&gv_target->hist, NULL))
					{
						assert(FALSE == ready2signal_gvundef); /* t_end should have reset this */
						continue;
					}
				} else
				{
					status = tp_hist(NULL);
					if (cdb_sc_normal != status)
					{
						assert(FALSE == ready2signal_gvundef); /* tp_hist should have reset this */
						t_retry(status);
						continue;
					}
				}
				assert(FALSE == ready2signal_gvundef);	/* t_end/tp_hist should have reset this right away */
				INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_get, 1);
				return FALSE;
			}
		}
		t_retry(status);
	}
}
