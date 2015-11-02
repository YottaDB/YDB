/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
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

/* needed for spanning nodes */
#include "op.h"
#include "op_tcommit.h"
#include "error.h"
#include "tp_frame.h"
#include "tp_restart.h"
#include "gtmimagename.h"

LITREF	mstr		nsb_dummy;

GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	uint4			dollar_tlevel;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			t_err;

error_def(ERR_GVDATAGETFAIL);
error_def(ERR_GVKILLFAIL);
error_def(ERR_GVPUTFAIL);

enum cdb_sc gvcst_dataget(mint *dollar_data, mval *val)
{
	boolean_t	gotit, gotspan, gotpiece, gotdummy, sn_tpwrapped, check_rtsib;
	mint		dollar_data_ctrl, dollar_data_piece, dollar_data_null, dg_info;
	mval		val_ctrl, val_piece;
	int		gblsize, chunk_size, i, total_len, oldend, tmp_numsubs;
	unsigned short	numsubs;
	sm_uc_ptr_t	sn_ptr;
	enum cdb_sc	status;
	int		save_dollar_tlevel;

	dg_info = *dollar_data;
	assert((DG_GETONLY == dg_info) || (DG_DATAGET == dg_info));
	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	assert(dollar_tlevel);
	if (cdb_sc_normal != (status = gvcst_dataget2(dollar_data, val, NULL)))
	{
		assert(save_dollar_tlevel == dollar_tlevel);
		return status;
	}
#	ifdef UNIX
	gotit = *dollar_data % 10;
	if (gotit && IS_SN_DUMMY(val->str.len, val->str.addr))
	{	/* We just found a dummy nodelet. Need to look for chunks of spanning node */
		IF_SN_DISALLOWED_AND_NO_SPAN_IN_DB(return status);
		oldend = gv_currkey->end;
		APPEND_HIDDEN_SUB(gv_currkey);
		/* Search for control subscript */
		dollar_data_ctrl = dg_info;
		if (cdb_sc_normal != (status = gvcst_dataget2(&dollar_data_ctrl, &val_ctrl, NULL)))
		{
			RESTORE_CURRKEY(gv_currkey, oldend);
			assert(save_dollar_tlevel == dollar_tlevel);
			return status;
		}
		gotspan = dollar_data_ctrl % 10;
		if (gotspan)
		{	/* Spanning node indeed, as expected. Piece it together. Recompute dollar_data. */
			if (val_ctrl.str.len == 6)
			{
				GET_NSBCTRL(val_ctrl.str.addr, numsubs, gblsize);
			} else
			{
				SSCANF(val_ctrl.str.addr, "%d,%d", &tmp_numsubs, &gblsize);
				numsubs = tmp_numsubs;
			}
			ENSURE_STP_FREE_SPACE(gblsize + cs_addrs->hdr->blk_size); /* give leeway.. think about more */
			DBG_MARK_STRINGPOOL_UNUSABLE;
			sn_ptr = stringpool.free;
			total_len = 0;
			val->str.addr = (char *)sn_ptr;
			for (i = 0; i < numsubs; i++)
			{
				NEXT_HIDDEN_SUB(gv_currkey, i);
				/* We only need to do a rtsib on the last chunk. Only there can we check for descendants */
				check_rtsib = (((i + 1) == numsubs) && (DG_DATAGET == dg_info));
				dollar_data_piece = (check_rtsib) ? DG_GETSNDATA : DG_GETONLY;
				if (cdb_sc_normal != (status = gvcst_dataget2(&dollar_data_piece, &val_piece, sn_ptr)))
				{
					RESTORE_CURRKEY(gv_currkey, oldend);
					DBG_MARK_STRINGPOOL_USABLE;
					assert(save_dollar_tlevel == dollar_tlevel);
					return status;
				}
				gotpiece = dollar_data_piece % 10;
				if (gotpiece)
				{
					sn_ptr += val_piece.str.len;
					total_len += val_piece.str.len;
				}
				assert(total_len < (gblsize + cs_addrs->hdr->blk_size));
				if (!gotpiece || (total_len > gblsize))
					break;
			}
			if ((total_len != gblsize) || (i != numsubs))
			{	/* Fetched value either too small or too big compared to what control subscript says */
				RESTORE_CURRKEY(gv_currkey, oldend);
				DBG_MARK_STRINGPOOL_USABLE;
				assert(save_dollar_tlevel == dollar_tlevel);
				return cdb_sc_spansize;
			}
			assert(val->mvtype == MV_STR);
			val->str.len = gblsize;
			stringpool.free += gblsize;
			DBG_MARK_STRINGPOOL_USABLE;
			if (check_rtsib)
			{
				*dollar_data = dollar_data_piece;
				if ((11 != dollar_data_piece) && cs_data->std_null_coll)
				{	/* Check for a null-subscripted descendant. Append null sub to gv_currkey and check $data */
					RESTORE_CURRKEY(gv_currkey, oldend);
					gv_currkey->end = oldend + 2;
					gv_currkey->base[oldend + 0] = 1;
					gv_currkey->base[oldend + 1] = 0;
					gv_currkey->base[oldend + 2] = 0;
					dollar_data_null = DG_DATAONLY;
					gvcst_dataget2(&dollar_data_null, &val_piece, NULL);
					if (dollar_data_null)
						*dollar_data = 11; /* Child found */
					RESTORE_CURRKEY(gv_currkey, oldend);
				}
			}
		}
		RESTORE_CURRKEY(gv_currkey, oldend);
	}
	if (DG_GETONLY == dg_info)
		*dollar_data = (mint)gotit; /* Just return 1 if it exists and 0 if it doesn't */
	assert(save_dollar_tlevel == dollar_tlevel);
#	endif
	return status;
}

/* This function is the equivalent of invoking gvcst_data & gvcst_get at the same time.
 * One crucial difference is that this function does NOT handle restarts by automatically invoking t_retry.
 * Instead, it returns the restart code to the caller so that it can handle the restart accordingly.
 * This is important in the case of triggers because we do NOT want to call t_retry in case of a implicit tstart
 * wrapped gvcst_put or gvcst_kill trigger-invoking update transaction. Additionally, this function assumes
 * that it is called always inside of TP (i.e. dollar_tlevel is non-zero).
 */
enum cdb_sc gvcst_dataget2(mint *dollar_data, mval *val, unsigned char *sn_ptr)
{
	blk_hdr_ptr_t	bp;
	boolean_t	do_rtsib;
	enum cdb_sc	status;
	mint		dlr_data, dg2_info;
	rec_hdr_ptr_t	rp;
	unsigned short	match, rsiz;
	srch_blk_status *bh;
	srch_hist	*rt_history;
	sm_uc_ptr_t	b_top;
	int		key_size, data_len, delta;
	uint4		save_t_err;

	/* The following code is lifted from gvcst_data. Any changes here might need to be reflected there as well */
	assert(dollar_tlevel);
	assert((CDB_STAGNATE > t_tries) || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	save_t_err = t_err;
	assert((ERR_GVKILLFAIL == save_t_err) || (ERR_GVPUTFAIL == save_t_err)); /* called only from gvcst_kill and gvcst_put */
	t_err = ERR_GVDATAGETFAIL;	/* switch t_err to reflect dataget sub-operation (under the KILL operation) */
	/* In case of a failure return, it is ok to return with t_err set to ERR_GVDATAGETFAIL as that gives a better
	 * picture of exactly where in the transaction the failure occurred.
	 */
	dg2_info = *dollar_data;
	assert((DG_GETONLY == dg2_info) || (DG_DATAGET == dg2_info) || (DG_DATAONLY == dg2_info) || (DG_GETSNDATA == dg2_info));
	delta = (DG_GETSNDATA == dg2_info) ? 4 : 0; /* next key doesn't need to match hidden subscript */
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
		data_len = rsiz + EVAL_CMPC(rp) - SIZEOF(rec_hdr) - key_size;
		if ((0 > data_len) || ((sm_uc_ptr_t)rp + rsiz > b_top))
		{
			assert(CDB_STAGNATE > t_tries);
			status = cdb_sc_rmisalign1;
			return status;
		} else if (DG_DATAONLY != dg2_info)
		{
			val->str.len = data_len;
			if (!sn_ptr)
			{
				ENSURE_STP_FREE_SPACE(data_len);
				memcpy(stringpool.free, (sm_uc_ptr_t)rp + rsiz - data_len, data_len);
				val->str.addr = (char *)stringpool.free;
				stringpool.free += data_len;
			} else
			{
				memcpy(sn_ptr, (sm_uc_ptr_t)rp + rsiz - data_len, data_len);
				val->str.addr = (char *)sn_ptr;
			}
		}
		/* --------------------- end code lifted from gvcst_get ---------------------------- */
		rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rsiz);
		if ((sm_uc_ptr_t)rp > b_top)
		{
			status = cdb_sc_rmisalign;
			return status;
		} else if ((sm_uc_ptr_t)rp == b_top)
			do_rtsib = TRUE;
		else if (EVAL_CMPC(rp) + delta >= gv_currkey->end)
			dlr_data += 10;
	} else if (match + delta >= gv_currkey->end)
		dlr_data = 10;
	else
	{
		dlr_data = 0;
		if (rp == (rec_hdr_ptr_t)b_top)
			do_rtsib = TRUE;
	}
	if ((DG_GETONLY != dg2_info) && do_rtsib && (cdb_sc_endtree != (status = gvcst_rtsib(rt_history, 0))))
	{	/* only do rtsib and search_blk if full data information is desired */
		if ((cdb_sc_normal != status) || (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, rt_history->h))))
			return status;
		if (rt_history->h[0].curr_rec.match + delta >= gv_currkey->end)
		{
			assert(1 >= dlr_data);
			dlr_data += 10;
		}
	}
	status = tp_hist(0 == rt_history->h[0].blk_num ? NULL : rt_history);
	if (cdb_sc_normal != status)
		return status;
	*dollar_data = (DG_GETONLY != dg2_info) ? dlr_data : (dlr_data % 10);
	t_err = save_t_err;	/* restore t_err to what it was at function entry */
	return status;
}
