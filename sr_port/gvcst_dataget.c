/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "stringpool.h"
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

GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned int	t_tries;
GBLREF	uint4		t_err;

/* This function is the equivalent of invoking gvcst_data & gvcst_get at the same time.
 * One crucial difference is that this function does NOT handle restarts by automatically invoking t_retry.
 * Instead, it returns the restart code to the caller so that it can handle the restart accordingly.
 * This is important in the case of triggers because we do NOT want to call t_retry in case of a implicit tstart
 * wrapped gvcst_put or gvcst_kill trigger-invoking update transaction. Additionally, this function assumes
 * that it is called always inside of TP (i.e. dollar_tlevel is non-zero).
 */
enum cdb_sc gvcst_dataget(mint *dollar_data, mval *val)
{
	blk_hdr_ptr_t	bp;
	boolean_t	do_rtsib;
	enum cdb_sc	status;
	mint		dlr_data;
	rec_hdr_ptr_t	rp;
	unsigned short	match, rsiz;
	srch_blk_status *bh;
	srch_hist	*rt_history;
	sm_uc_ptr_t	b_top;
	int		key_size, data_len;
	uint4		save_t_err;

	error_def(ERR_GVDATAGETFAIL);
	error_def(ERR_GVKILLFAIL);

	/* The following code is lifted from gvcst_data. Any changes here might need to be reflected there as well */
	assert(dollar_tlevel);
	assert((CDB_STAGNATE > t_tries) || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	save_t_err = t_err;
	assert(ERR_GVKILLFAIL == save_t_err);	/* this function should currently be called only from gvcst_kill */
	t_err = ERR_GVDATAGETFAIL;	/* switch t_err to reflect dataget sub-operation (under the KILL operation) */
	/* In case of a failure return, it is ok to return with t_err set to ERR_GVDATAGETFAIL as that gives a better
	 * picture of exactly where in the transaction the failure occurred.
	 */
	rt_history = gv_target->alt_hist;
	rt_history->h[0].blk_num = 0;
	if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))
		return status;
	bh = gv_target->hist.h;
	bp = (blk_hdr_ptr_t)bh->buffaddr;
	rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
	b_top = bh->buffaddr + bp->bsiz;
	match = bh->curr_rec.match;
	key_size = gv_currkey->end + 1;
	do_rtsib = FALSE;
	/* Even if key does not exist, return null string in "val". Caller can use dollar_data to distinguish
	 * whether the key is undefined or defined and set to the null string.
	 */
	val->mvtype = MV_STR;
	val->str.len = 0;
	if (key_size == match)
	{
		dlr_data = 1;
		/* the following code is lifted from gvcst_get. any changes here might need to be reflected there as well */
		GET_USHORT(rsiz, &rp->rsiz);
		data_len = rsiz + rp->cmpc - SIZEOF(rec_hdr) - key_size;
		if ((0 > data_len) || ((sm_uc_ptr_t)rp + rsiz > b_top))
		{
			assert(CDB_STAGNATE > t_tries);
			status = cdb_sc_rmisalign1;
			return status;
		} else
		{
			ENSURE_STP_FREE_SPACE(data_len);
			memcpy(stringpool.free, (sm_uc_ptr_t)rp + rsiz - data_len, data_len);
			val->str.addr = (char *)stringpool.free;
			val->str.len = data_len;
			stringpool.free += data_len;
		}
		/* --------------------- end code lifted from gvcst_get ---------------------------- */
		rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rsiz);
		if ((sm_uc_ptr_t)rp > b_top)
		{
			status = cdb_sc_rmisalign;
			return status;
		} else if ((sm_uc_ptr_t)rp == b_top)
			do_rtsib = TRUE;
		else if (rp->cmpc >= gv_currkey->end)
			dlr_data += 10;
	} else if (match >= gv_currkey->end)
		dlr_data = 10;
	else
	{
		dlr_data = 0;
		if (rp == (rec_hdr_ptr_t)b_top)
			do_rtsib = TRUE;
	}
	if (do_rtsib && (cdb_sc_endtree != (status = gvcst_rtsib(rt_history, 0))))
	{
		if ((cdb_sc_normal != status) || (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, rt_history->h))))
			return status;
		if (rt_history->h[0].curr_rec.match >= gv_currkey->end)
		{
			assert(1 >= dlr_data);
			dlr_data += 10;
		}
	}
	status = tp_hist(0 == rt_history->h[0].blk_num ? NULL : rt_history);
	if (cdb_sc_normal != status)
		return status;
	*dollar_data = dlr_data;
	t_err = save_t_err;	/* restore t_err to what it was at function entry */
	return status;
}
