/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search,gvcst_search_blk,gvcst_query prototype */

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

error_def(ERR_DBROLLEDBACK);
error_def(ERR_GVQUERYFAIL);
error_def(ERR_TPRETRY);

DEFINE_NSB_CONDITION_HANDLER(gvcst_query_ch)

bool gvcst_query(void)
{	/* Similar to gvcst_order and gvcst_zprevious. In each case we skip over hidden subscripts as needed.
	 *
	 *     1  2  3  NULL                           <--- order/zprev...
	 *     1  2  3  NULL  NULL
	 *     1  2  3  NULL  NULL  NULL                                <--- query from here...
	 *     1  2  3  NULL  NULL  NULL  hidden
	 *     1  2  3  NULL  NULL  hidden
	 *     1  2  3  NULL  hidden
	 *     1  2  3  hidden                         <--- ... skip this guy and go to bottom/top, respectively
	 *     1  2  3  7                                               <--- ... needs to end up here
	 */
	bool		found, is_hidden, sn_tpwrapped;
	boolean_t	est_first_pass;
	char		save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key		*save_gv_currkey;
	int		end, i;
	int		save_dollar_tlevel;

	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	found = gvcst_query2();
#	ifdef UNIX
	assert(save_dollar_tlevel == dollar_tlevel);
	CHECK_HIDDEN_SUBSCRIPT_AND_RETURN(found, gv_altkey, is_hidden);
	IF_SN_DISALLOWED_AND_NO_SPAN_IN_DB(return found);
	assert(found && is_hidden);
	SAVE_GV_CURRKEY;
	if (!dollar_tlevel)
	{
		sn_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
		ESTABLISH_NORET(gvcst_query_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		sn_tpwrapped = FALSE;
	for (i = 0; i <= MAX_GVSUBSCRIPTS; i++)
	{
		INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_query, (gtm_uint64_t) -1);
		found = gvcst_query2();
		CHECK_HIDDEN_SUBSCRIPT_AND_BREAK(found, gv_altkey, is_hidden);
		assert(found && is_hidden);
		/* Replace last subscript to be the highest possible hidden subscript so another
		 * gvcst_query2 will give us the next non-hidden subscript.
		 */
		end = gv_altkey->end;
		gv_currkey->base[end - 4] = 2;
		gv_currkey->base[end - 3] = 0xFF;
		gv_currkey->base[end - 2] = 0xFF;
		gv_currkey->base[end - 1] = 1;
		gv_currkey->base[end + 0] = 0;
		gv_currkey->base[end + 1] = 0;
		gv_currkey->end = end + 1;
	}
	if (sn_tpwrapped)
	{
		op_tcommit();
		REVERT; /* remove our condition handler */
	}
	RESTORE_GV_CURRKEY;
	assert(save_dollar_tlevel == dollar_tlevel);
#	endif
	return found;
}

bool gvcst_query2(void)
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
#if defined(DEBUG) && defined(UNIX)
		if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_GVQUERYFAIL == gtm_white_box_test_case_number))
		{
			t_retry(cdb_sc_blknumerr);
			continue;
		}
#endif
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
