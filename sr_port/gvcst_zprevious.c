/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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
{	/* See gvcst_query.c */
	boolean_t	found, is_hidden, sn_tpwrapped;
	boolean_t	est_first_pass;
	gv_key		save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	int		prev, oldend;
	int		save_dollar_tlevel;

	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	found = gvcst_zprevious2();
#	ifdef UNIX
	assert(save_dollar_tlevel == dollar_tlevel);
	CHECK_HIDDEN_SUBSCRIPT_AND_RETURN(found, gv_altkey, is_hidden);
	assert(found && is_hidden);
	IF_SN_DISALLOWED_AND_NO_SPAN_IN_DB(return found);
	SAVE_GV_CURRKEY_LAST_SUBSCRIPT(save_currkey, prev, oldend);
	if (!dollar_tlevel)
	{
		sn_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
		ESTABLISH_NORET(gvcst_zprevious_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		sn_tpwrapped = FALSE;
	INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_zprev, (gtm_uint64_t) -1);
	found = gvcst_zprevious2();
	if (found)
	{
		CHECK_HIDDEN_SUBSCRIPT(gv_altkey, is_hidden);
		if (is_hidden)
		{	/* Replace last subscript to be the lowest possible hidden subscript so another
			 * gvcst_zprevious2 will give us the previous non-hidden subscript.
			 */
			REPLACE_HIDDEN_SUB_TO_LOWEST(gv_altkey, gv_currkey);	/* uses gv_altkey to modify gv_currkey */
			/* fix up since it should only be externally counted as one $zprevious */
			INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_zprev, (gtm_uint64_t) -1);
			found = gvcst_zprevious2();
		}
	}
	if (sn_tpwrapped)
	{
		op_tcommit();
		REVERT; /* remove our condition handler */
	}
	RESTORE_GV_CURRKEY_LAST_SUBSCRIPT(save_currkey, prev, oldend);
	assert(save_dollar_tlevel == dollar_tlevel);
#	endif
	return found;
}

boolean_t	gvcst_zprevious2(void)
{
	boolean_t	found, two_histories;
	enum cdb_sc	status;
	srch_blk_status	*bh;
	srch_hist	*lft_history;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVORDERFAIL);
	TREF(expand_prev_key) = TRUE;
	/* Note that "t_retry" usage below could transfer control out of this function if dollar_tlevel > 0. If so,
	 * we need to remember to reset TREF(expand_prev_key) to FALSE since this zprevious action has terminated.
	 * We do that reset in tp_restart.
	 */
	for (;;)
	{
		assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
		two_histories = FALSE;
		if (cdb_sc_normal == (status = gvcst_search(gv_currkey, NULL)))
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
			if ((gv_altkey->end < gv_currkey->end) && (gv_altkey->end <= gv_currkey->prev))
				found = FALSE;
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
			return (found && (bh->prev_rec.match >= gv_currkey->prev));
		}
		t_retry(status);
	}
}
