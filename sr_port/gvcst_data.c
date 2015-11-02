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
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search,gvcst_search_blk,gvcst_data prototype */

/* needed for spanning nodes */
#include "op.h"
#include "op_tcommit.h"
#include "error.h"
#include "tp_frame.h"
#include "tp_restart.h"
#include "gtmimagename.h"

LITREF	mval		literal_batch;

GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;

error_def(ERR_DBROLLEDBACK);
error_def(ERR_GVDATAFAIL);
error_def(ERR_TPRETRY);

DEFINE_NSB_CONDITION_HANDLER(gvcst_data_ch)

mint	gvcst_data(void)
{
	bool		found, sn_tpwrapped;
	boolean_t	est_first_pass;
	int		oldend;
	mint		val;
	int		save_dollar_tlevel;

	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	val = gvcst_data2();
#	ifdef UNIX
	if (-1 != val)
	{
		assert(save_dollar_tlevel == dollar_tlevel);
		return val;
	}
	oldend = gv_currkey->end;
	if (!dollar_tlevel)
	{
		sn_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
		ESTABLISH_NORET(gvcst_data_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		sn_tpwrapped = FALSE;
	/* fix up since it should only be externally counted as one $data */
	INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_data, (gtm_uint64_t) -1);
	val = gvcst_data2();
	if (-1 == val)
	{	/* -1 implies node exists. Need to see if a proper descendant exists */
		val = 1;
		/* 0 1 0 0 <-- append that to gv_currkey */
		gv_currkey->end = oldend + 2;
		gv_currkey->base[oldend + 0] = 1;
		gv_currkey->base[oldend + 1] = 0;
		gv_currkey->base[oldend + 2] = 0;
		found = gvcst_query(); /* want to save gv_altkey? */
		if (found && (0 == memcmp(gv_currkey->base, gv_altkey->base, oldend)))
			val += 10;
	}
	if (sn_tpwrapped)
	{
		op_tcommit();
		REVERT; /* remove our condition handler */
	}
	RESTORE_CURRKEY(gv_currkey, oldend);
	assert(save_dollar_tlevel == dollar_tlevel);
#	endif
	return val;
}

mint	gvcst_data2(void)
{
	blk_hdr_ptr_t	bp;
	boolean_t	do_rtsib, is_dummy;
	enum cdb_sc	status;
	mint		val;
	rec_hdr_ptr_t	rp;
	unsigned short	match, rsiz;
	srch_blk_status *bh;
	srch_hist	*rt_history;
	sm_uc_ptr_t	b_top;
	int		tmp_cmpc;
	int		data_len, cur_val_offset, realval = 0;

	VMS_ONLY(assert((gv_target->root < cs_addrs->ti->total_blks) || dollar_tlevel));
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVDATAFAIL);
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	for (;;)
	{
		/* The following code is duplicated in gvcst_dataget. Any changes here might need to be reflected there as well */
		rt_history = gv_target->alt_hist;
		rt_history->h[0].blk_num = 0;
#if defined(DEBUG) && defined(UNIX)
		if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_GVDATAFAIL == gtm_white_box_test_case_number))
		{
			t_retry(cdb_sc_blknumerr);
			continue;
		}
#endif
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
		realval = 0;
		if (gv_currkey->end + 1 == match)
		{
			val = 1;
			GET_USHORT(rsiz, &rp->rsiz);
#			ifdef UNIX
			/* check for spanning node dummy value: a single zero byte */
			cur_val_offset = SIZEOF(rec_hdr) + match - EVAL_CMPC((rec_hdr_ptr_t)rp);
			data_len = rsiz - cur_val_offset;
			is_dummy = IS_SN_DUMMY(data_len, (sm_uc_ptr_t)rp + cur_val_offset);
			if (is_dummy)
			{
				realval = -1;
				IF_SN_DISALLOWED_AND_NO_SPAN_IN_DB(realval = 0); /* resume since this is not a spanning node */
			}
#			endif
			rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rsiz);
			if ((sm_uc_ptr_t)rp > b_top)
			{
				t_retry(cdb_sc_rmisalign);
				continue;
			} else if ((sm_uc_ptr_t)rp == b_top)
				do_rtsib = TRUE;
			else if (EVAL_CMPC(rp) >= gv_currkey->end)
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
		return (0 != realval) ? realval : val;
	}
}
