/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Code in this module is based on gvcst_query.c and hence has an
 * FIS copyright even though this module was not created by FIS.
 */

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
#ifdef UNIX			/* needed for frame_pointer in GVCST_ROOT_SEARCH_AND_PREP macro */
# include "repl_msg.h"
# include "gtmsource.h"
# include "rtnhdr.h"
# include "stack_frame.h"
# include "wbox_test_init.h"
#endif

#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_expand_key.h"
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search,gvcst_search_blk,gvcst_reversequery prototype */

/* needed for spanning nodes */
#include "op.h"
#include "op_tcommit.h"
#include "error.h"
#include "tp_frame.h"
#include "tp_restart.h"
#include "gtmimagename.h"

LITREF	mval		literal_batch;

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;

error_def(ERR_GVQUERYFAIL);

DEFINE_NSB_CONDITION_HANDLER(gvcst_reversequery_ch)

/* This function implements reverse $query for global nodes. With Standard NULL collation, a null subscript ("")
 * is represented as the byte 0x01. Whereas a node that spans multiple GDS blocks (spanning node) has multiple nodes
 * with hidden subscripts each of which start with the byte 0x02. So if $query(^x(1,2,3,7),-1) is requested, we
 * might need to go back not just one node, but skip over hidden subscripts too until we find a node with non-hidden
 * subscripts. Below is an example node layout where the subscripts of each node are listed. The result of the
 * $query(^x(1,2,3,7)) operation should be ^x(1,2,3,"","",""). Hence the MAX_GVSUBSCRIPTS for loop below.
 *
 *  ^x(1,2,3,"")
 *  ^x(1,2,3,"","")
 *  ^x(1,2,3,"","","")              <--- reverse $query needs to end up here
 *  ^x(1,2,3,"","","",hidden)
 *  ^x(1,2,3,"","",hidden)
 *  ^x(1,2,3,"",hidden)
 *  ^x(1,2,3,hidden)
 *  ^x(1,2,3,7)                     <--- reverse $query begins here
 *
 * "gv_currkey" already points to the input "gvn" for which reverse $query is sought.
 * "gv_altkey" stores the result of the reverse $query at function end.
 */
boolean_t	gvcst_reversequery(void)
{
	boolean_t	found, is_hidden, sn_tpwrapped;
	boolean_t	est_first_pass;
	gv_key		save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	int		i;
	int		save_dollar_tlevel;

	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	found = gvcst_reversequery2();
	assert(save_dollar_tlevel == dollar_tlevel);
	CHECK_HIDDEN_SUBSCRIPT_AND_RETURN(found, gv_altkey, is_hidden);
	IF_SN_DISALLOWED_AND_NO_SPAN_IN_DB(return found);
	assert(found && is_hidden);
	SAVE_GV_CURRKEY(save_currkey);
	/* Note: This code does: 1) tstart, 2) ESTABLISH, 3) tcommit, 4) REVERT in that order.
	 * One might think either (1 & 2) or (3 & 4) be reversed to keep things straight.
	 * But the order has to the way it is laid out because condition handler "gvcst_reversequery_ch"
	 * needs to take control inside a TP transaction so it has to be after the tstart.
	 * But restarts are possible even inside the tcommit step. And so the REVERT has to happen
	 * after the tcommit.
	 */
	if (!dollar_tlevel)
	{
		sn_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
		ESTABLISH_NORET(gvcst_reversequery_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		sn_tpwrapped = FALSE;
	for (i = 0; i <= MAX_GVSUBSCRIPTS; i++)
	{
		INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_query, (gtm_uint64_t) -1);
		found = gvcst_reversequery2();
		if (found)
		{
			CHECK_HIDDEN_SUBSCRIPT(gv_altkey, is_hidden);
			if (!is_hidden)
				break;
		} else
			break;
		assert(found && is_hidden);
		COPY_KEY(gv_currkey, gv_altkey);
		/* Replace last subscript to be the lowest possible hidden subscript so another
		 * gvcst_reversequery2 will give us the node previous to this. It is possible that is also a hidden
		 * subscript and so we need to loop but the # of iterations is limited to the max # of subscripts.
		 */
		REPLACE_HIDDEN_SUB_TO_LOWEST(gv_altkey, gv_currkey);	/* uses gv_altkey to modify gv_currkey */
	}
	assert(MAX_GVSUBSCRIPTS >= i);
	if (sn_tpwrapped)
	{
		op_tcommit();
		REVERT; /* remove our condition handler */
	}
	RESTORE_GV_CURRKEY(save_currkey);
	assert(save_dollar_tlevel == dollar_tlevel);
	return found;
}

/* This function does a reverse $query on an input key (gv_currkey points to the input key) and returns the
 * immediately previous key (gv_altkey points to this at function end). It is possible that the last subscript in
 * the previous key has a hidden subscript in which case, the caller "gvcst_query2" would invoke this function again
 * until a key with no hidden subscripts is found which is the final $query return value.
 */
boolean_t	gvcst_reversequery2(void)
{
	boolean_t	found, two_histories;
	enum cdb_sc	status;
	srch_blk_status	*bh;
	srch_hist	*lft_history;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVQUERYFAIL);
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	TREF(expand_prev_key) = TRUE;	/* this will cause "gv_altkey" to contain fully expanded previous key */
	/* Note that "t_retry" usage below could transfer control out of this function if dollar_tlevel > 0. If so,
	 * we need to remember to reset TREF(expand_prev_key) to FALSE since this reverse $query action has terminated.
	 * We do that reset in tp_restart.
	 */
	for (;;)
	{
		two_histories = FALSE;
#		ifdef DEBUG
		if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_GVQUERYFAIL == gtm_white_box_test_case_number))
		{
			t_retry(cdb_sc_blknumerr);
			continue;
		}
#		endif
		if (cdb_sc_normal == (status = gvcst_search(gv_currkey, 0)))
		{
			found = TRUE;
			bh = &gv_target->hist.h[0];
			/* Before using "bh->prev_rec", assert that it is usable. Can assert this since bh is a leaf level block
			 * and prev_rec is always initialized for leaf blocks in "gvcst_search".
			 */
			ASSERT_LEAF_BLK_PREV_REC_INITIALIZED(bh);
			if (0 == bh->prev_rec.offset)
			{
				two_histories = TRUE;
				lft_history = gv_target->alt_hist;
				status = gvcst_lftsib(lft_history);
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
					bh = &lft_history->h[0];
					if (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, bh)))
					{
						t_retry(status);
						continue;
					}
				}
			}
			/* At this point, gv_altkey contains the fully expanded key */
			if (!dollar_tlevel)
			{
				if ((trans_num)0 == t_end(&gv_target->hist, !two_histories ? NULL : lft_history, TN_NOT_SPECIFIED))
					continue;
			} else
			{
				status = tp_hist(!two_histories ? NULL : lft_history);
				if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				}
		    	}
			assert(cs_data == cs_addrs->hdr);
			INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_query, 1);
			TREF(expand_prev_key) = FALSE;
#			ifdef DEBUG
			/* Note that "gv_altkey->end" could be 0 in case of reverse $query. So avoid validity check below. */
			if (gv_altkey->end)
				DBG_CHECK_GVKEY_VALID(gv_altkey);
#			endif
			return found;
		}
		t_retry(status);
	}
}
