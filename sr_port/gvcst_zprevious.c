/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
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
#ifdef UNIX			/* needed for frame_pointer in GVCST_ROOT_SEARCH_AND_PREP macro */
# include "repl_msg.h"
# include "gtmsource.h"
# include "rtnhdr.h"
# include "stack_frame.h"
#endif

#include "t_end.h"		/* prototypes */
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_protos.h"	/* for gvcst_lftsib,gvcst_search,gvcst_search_blk,gvcst_zprevious prototype */

/* needed for spanning nodes */
#include "op.h"
#include "op_tcommit.h"
#include "error.h"
#include "tp_frame.h"
#include "tp_restart.h"
#include "gtmimagename.h"

LITREF	mval		literal_batch;

GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF int4		gv_keysize;
GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;

error_def(ERR_GVORDERFAIL);

DEFINE_NSB_CONDITION_HANDLER(gvcst_zprevious_ch)

boolean_t	gvcst_zprevious(void)
{
	boolean_t	found, is_hidden;

#	ifdef DEBUG
	CHECK_HIDDEN_SUBSCRIPT(gv_currkey, is_hidden);
	assert(!is_hidden);
#	endif
	found = gvcst_zprevious2();
	if (found)
	{	/* If the previous subscript found is a hidden subscript, it is possible there is a null subscript
		 * ("" in standard null collation representation) before this hidden subscript in which case that needs
		 * to be returned. But that will require one more call to "gvcst_zprevious2". But even if the null
		 * subscript does exist and we return "found" as TRUE with the null subscript, the caller (op_zprevious)
		 * is going to return an empty string in both cases (whether or not a null subscript was found behind
		 * the hidden subscript). Therefore, we avoid the second call and return "found" as FALSE in both cases.
		 */
		CHECK_HIDDEN_SUBSCRIPT(gv_altkey, is_hidden);
		if (is_hidden)
			return FALSE;
		else
			return TRUE;
	} else
		return FALSE;
}

boolean_t	gvcst_zprevious2(void)
{
	boolean_t	found, two_histories;
	enum cdb_sc	status;
	srch_blk_status	*bh;
	srch_hist	*lft_history;
	unsigned int	currkey_prev, currkey_end, altkey_end, prev_rec_match;
	unsigned char	*c, *ctop;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVORDERFAIL);
	TREF(expand_prev_key) = TRUE;	/* this will cause "gv_altkey" to contain fully expanded previous key */
	/* Note that "t_retry" usage below could transfer control out of this function if dollar_tlevel > 0. If so,
	 * we need to remember to reset TREF(expand_prev_key) to FALSE since this zprevious action has terminated.
	 * We do that reset in tp_restart.
	 */
	for (;;)
	{
		assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
		two_histories = FALSE;
		if (cdb_sc_normal == (status = gvcst_search(gv_currkey, NULL)))	/* will set "gv_altkey" to contain previous key */
		{
			found = TRUE;
			bh = gv_target->hist.h;
			if (0 == bh->prev_rec.offset)
			{
				two_histories = TRUE;
				lft_history = gv_target->alt_hist;
				status = gvcst_lftsib(lft_history);
				if (cdb_sc_normal == status)
				{
					bh = lft_history->h;
					if (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, bh)))
					{
						t_retry(status);
						continue;
					}
				} else  if (cdb_sc_endtree == status)
				{
					found = FALSE;
					two_histories = FALSE;		/* second history not valid */
				} else
				{
					t_retry(status);
					continue;
				}
			}
			assert(gv_altkey->top == gv_currkey->top);
			assert(gv_altkey->top == gv_keysize);
			assert(gv_currkey->end < gv_currkey->top);
			assert(gv_altkey->end < gv_altkey->top);
			currkey_prev   = gv_currkey->prev;
			currkey_end    = gv_currkey->end;
			altkey_end     = gv_altkey->end;
			prev_rec_match = bh->prev_rec.match;
			if (((altkey_end < currkey_end) && (altkey_end <= currkey_prev)) || (prev_rec_match < currkey_prev))
				found = FALSE;
			else
			{	/* Truncate gv_altkey to same subscript level/depth as gv_currkey */
				c = gv_altkey->base;
				ctop = c + altkey_end;
				c += prev_rec_match;
				for (;;)
				{
					if (c >= ctop)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_rmisalign;
						break;
					}
 					if (0 == *c++)
					{
						*c = 0;
						break;
					}
				}
				if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				}
				gv_altkey->end = c - gv_altkey->base;
				assert(gv_altkey->end < gv_altkey->top);
			}
			if (!dollar_tlevel)
			{
				if ((trans_num)0 == t_end(&gv_target->hist, two_histories ? lft_history : NULL, TN_NOT_SPECIFIED))
					continue;
			} else
			{
				status = tp_hist(two_histories ? lft_history : NULL);
				if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				}
			}
			assert(cs_data == cs_addrs->hdr);
			INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_zprev, 1);
			TREF(expand_prev_key) = FALSE;
			return found;
		}
		t_retry(status);
	}
}
