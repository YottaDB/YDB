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

#include "gtm_string.h"

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
#ifdef UNIX			/* needed for frame_pointer in GVCST_ROOT_SEARCH_AND_PREP macro */
# include "repl_msg.h"
# include "gtmsource.h"
# include "rtnhdr.h"
# include "stack_frame.h"
# include "wbox_test_init.h"
#endif

#include "t_end.h"		/* prototypes */
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search,gvcst_search_blk,gvcst_order prototype */

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

error_def(ERR_DBROLLEDBACK);
error_def(ERR_GVORDERFAIL);
error_def(ERR_TPRETRY);

DEFINE_NSB_CONDITION_HANDLER(gvcst_order_ch)

bool	gvcst_order(void)
{	/* See gvcst_query.c */
	bool		found, is_hidden, sn_tpwrapped;
	boolean_t	est_first_pass;
	char		save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key		*save_gv_currkey;
	int		end, prev, oldend;
	int		save_dollar_tlevel;

	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	found = gvcst_order2();
#	ifdef UNIX
	assert(save_dollar_tlevel == dollar_tlevel);
	CHECK_HIDDEN_SUBSCRIPT_AND_RETURN(found, gv_altkey, is_hidden);
	assert(found && is_hidden);
	IF_SN_DISALLOWED_AND_NO_SPAN_IN_DB(return found);
	SAVE_GV_CURRKEY_LAST_SUBSCRIPT(gv_currkey, prev, oldend);
	if (!dollar_tlevel)
	{
		sn_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
		ESTABLISH_NORET(gvcst_order_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		sn_tpwrapped = FALSE;
	INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_order, (gtm_uint64_t) -1);
	found = gvcst_order2();
	if (found)
	{
		CHECK_HIDDEN_SUBSCRIPT(gv_altkey, is_hidden);
		if (is_hidden)
		{	/* Replace last subscript to be the highest possible hidden subscript so another
			 * gvcst_order2 will give us the next non-hidden subscript.
			 */
			end = gv_altkey->end;
			gv_currkey->base[end - 4] = 2;
			gv_currkey->base[end - 3] = 0xFF;
			gv_currkey->base[end - 2] = 0xFF;
			gv_currkey->base[end - 1] = 1;
			gv_currkey->base[end + 0] = 0;
			gv_currkey->base[end + 1] = 0;
			gv_currkey->end = end + 1;
			/* fix up since it should only be externally counted as one $order */
			INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_order, (gtm_uint64_t) -1);
			found = gvcst_order2();
		}
	}
	if (sn_tpwrapped)
	{
		op_tcommit();
		REVERT; /* remove our condition handler */
	}
	RESTORE_GV_CURRKEY_LAST_SUBSCRIPT(gv_currkey, prev, oldend);
	assert(save_dollar_tlevel == dollar_tlevel);
#	endif
	return found;
}

bool	gvcst_order2(void)
{
	blk_hdr_ptr_t	bp;
	boolean_t	found, two_histories;
	enum cdb_sc	status;
	rec_hdr_ptr_t	rp;
	unsigned short	rec_size;
	srch_blk_status	*bh;
	srch_hist	*rt_history;
	sm_uc_ptr_t	c1, c2, ctop, alt_top;
	int		tmp_cmpc;

	T_BEGIN_READ_NONTP_OR_TP(ERR_GVORDERFAIL);
	for (;;)
	{
		assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
		two_histories = FALSE;
#if defined(DEBUG) && defined(UNIX)
		if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_GVORDERFAIL == gtm_white_box_test_case_number))
		{
			status = cdb_sc_blknumerr;
			t_retry(status);
			continue;
		}
#endif
		if (cdb_sc_normal == (status = gvcst_search(gv_currkey, NULL)))
		{
			found = TRUE;
			bh = gv_target->hist.h;
			rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
			bp = (blk_hdr_ptr_t)bh->buffaddr;
			if ((rec_hdr_ptr_t)CST_TOB(bp) <= rp)
			{
				two_histories = TRUE;
				rt_history = gv_target->alt_hist;
				status = gvcst_rtsib(rt_history, 0);
				if (cdb_sc_normal == status)
				{
					bh = rt_history->h;
			       		if (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, bh)))
					{
						t_retry(status);
						continue;
					}
					rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
					bp = (blk_hdr_ptr_t)bh->buffaddr;
				} else
				{
			  	     	if (cdb_sc_endtree == status)
					{
						found = FALSE;
						two_histories = FALSE;		/* second history not valid */
					} else
					{
						t_retry(status);
						continue;
					}
				}
			}
			if (found)
			{
				assert(gv_altkey->top == gv_currkey->top);
				assert(gv_altkey->top == gv_keysize);
				assert(gv_altkey->end < gv_altkey->top);
				/* store new subscipt */
				c1 = gv_altkey->base;
				alt_top = gv_altkey->base + gv_altkey->top - 1;
					/* Make alt_top one less than gv_altkey->top to allow double-null at end of a key-name */
				/* 4/17/96
				 * HP compiler bug work-around.  The original statement was
				 * c2 = (unsigned char *)CST_BOK(rp) + bh->curr_rec.match - rp->cmpc;
				 *
				 * ...but this was sometimes compiled incorrectly (the lower 4 bits
				 * of rp->cmpc, sign extended, were subtracted from bh->curr_rec.match).
				 * I separated out the subtraction of rp->cmpc.
				 *
				 * -VTF.
				 */
				c2 = (sm_uc_ptr_t)CST_BOK(rp) + bh->curr_rec.match;
				memcpy(c1, gv_currkey->base, bh->curr_rec.match);
				c1 += bh->curr_rec.match;
				c2 -= EVAL_CMPC(rp);
				GET_USHORT(rec_size, &rp->rsiz);
				ctop = (sm_uc_ptr_t)rp + rec_size;
				for (;;)
				{
					if (c2 >= ctop  ||  c1 >= alt_top)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_rmisalign;
						goto restart;	/* goto needed because of nested FOR loop */
					}
 					if (0 == (*c1++ = *c2++))
					{
						*c1 = 0;
						break;
					}
				}
				gv_altkey->end = c1 - gv_altkey->base;
				assert(gv_altkey->end < gv_altkey->top);
			}
                        if (!dollar_tlevel)
			{
				if ((trans_num)0 == t_end(&gv_target->hist, two_histories ? rt_history : NULL, TN_NOT_SPECIFIED))
					continue;
			} else
			{
				status = tp_hist(two_histories ? rt_history : NULL);
				if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				}
			}
			assert(cs_data == cs_addrs->hdr);
			INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_order, 1);
			return (found && (bh->curr_rec.match >= gv_currkey->prev));
		}
restart:	t_retry(status);
	}
}
