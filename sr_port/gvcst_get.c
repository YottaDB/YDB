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
#include "gvcst_protos.h"	/* for gvcst_search,gvcst_get prototype */

/* needed for spanning nodes */
#include "op.h"
#include "op_tcommit.h"
#include "error.h"
#include "tp_frame.h"
#include "tp_restart.h"
#include "gtmimagename.h"

LITREF	mval		literal_batch;
LITREF	mstr		nsb_dummy;

GBLREF	gv_namehead	*gv_target;
GBLREF	gv_key		*gv_currkey;
GBLREF	spdesc		stringpool;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned int	t_tries;

error_def(ERR_DBROLLEDBACK);
error_def(ERR_GVGETFAIL);
error_def(ERR_TPRETRY);

DEFINE_NSB_CONDITION_HANDLER(gvcst_get_ch)

boolean_t gvcst_get(mval *v)
{	/* To avoid an extra function call, the outer if-check can be brought out into op_gvget (a final optimization) */
	boolean_t	gotit, gotspan, gotpiece, gotdummy, sn_tpwrapped;
	boolean_t	est_first_pass;
	mval		val_ctrl, val_piece;
	int		gblsize, chunk_size, i, total_len, oldend, tmp_numsubs;
	unsigned short	numsubs;
	sm_uc_ptr_t	sn_ptr;
	int		debug_len;
	int		save_dollar_tlevel;

	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	gotit = gvcst_get2(v, NULL);
#	ifdef UNIX
	DEBUG_ONLY(debug_len = (int)v->str.len); /* Ensure v isn't garbage pointer by actually accessing it */
	if (gotit && IS_SN_DUMMY(v->str.len, v->str.addr))
	{	/* Start TP transaction to piece together value */
		IF_SN_DISALLOWED_AND_NO_SPAN_IN_DB(return gotit);
		if (!dollar_tlevel)
		{
			sn_tpwrapped = TRUE;
			op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
			ESTABLISH_NORET(gvcst_get_ch, est_first_pass);
			GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
		} else
			sn_tpwrapped = FALSE;
		oldend = gv_currkey->end;
		/* fix up since it should only be externally counted as one get */
		INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_get, (gtm_uint64_t) -1);
		gotdummy = gvcst_get2(v, NULL);        /* Will be returned if not currently a spanning node */
		APPEND_HIDDEN_SUB(gv_currkey);
		/* fix up since it should only be externally counted as one get */
		INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_get, (gtm_uint64_t) -1);
		gotspan = gvcst_get2(&val_ctrl, NULL); /* Search for control subscript */
		if (gotspan)
		{	/* Spanning node indeed, as expected. Piece it together */
			if (val_ctrl.str.len == 6)
			{
				GET_NSBCTRL(val_ctrl.str.addr, numsubs, gblsize);
			} else
			{	/* Temporarily account for mixture of control node formats between FT04 and FT05.
				 * Note that this only works for block sizes greater than 1000.
				 */
				SSCANF(val_ctrl.str.addr, "%d,%d", &tmp_numsubs, &gblsize);
				numsubs = tmp_numsubs;
			}
			ENSURE_STP_FREE_SPACE(gblsize + cs_addrs->hdr->blk_size); /* give leeway.. think about more */
			sn_ptr = stringpool.free;
			total_len = 0;
			v->str.addr = (char *)sn_ptr;
			for (i = 0; i < numsubs; i++)
			{
				NEXT_HIDDEN_SUB(gv_currkey, i);
				gotpiece = gvcst_get2(&val_piece, sn_ptr);
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
				/* Fetched value either too small or too big compared to what control subscript says */
				t_retry(cdb_sc_spansize);
		}
		RESTORE_CURRKEY(gv_currkey, oldend);
		if (sn_tpwrapped)
		{
			op_tcommit();
			REVERT; /* remove our condition handler */
		}
		if (gotspan)
		{
			v->mvtype = MV_STR;
			/*v->str.addr = (char *)stringpool.free;*/
			v->str.len = gblsize;
			stringpool.free += gblsize;
		}
		gotit = gotspan || gotdummy;
	}
	assert(save_dollar_tlevel == dollar_tlevel);
#	endif
	return gotit;
}

boolean_t gvcst_get2(mval *v, unsigned char *sn_ptr)
{
	blk_hdr_ptr_t	bp;
	enum cdb_sc	status;
	int		key_size, data_len;
	int		tmp_cmpc;
	rec_hdr_ptr_t	rp;
	sm_uc_ptr_t	b_top;
	srch_blk_status *bh;
	unsigned short	rsiz;
#	ifdef DEBUG
	boolean_t	in_op_gvget_lcl;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY(
		/* Store global variable in_op_gvget in a local variable and reset the global right away to ensure that the global
		 * value does not incorrectly get carried over to the next call of gvcst_get (e.g. it if was from "op_fngvget").
		 */
		in_op_gvget_lcl = TREF(in_op_gvget);
		TREF(in_op_gvget) = FALSE;
	)
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVGETFAIL);
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	for (;;)
	{
#if defined(DEBUG) && defined(UNIX)
		if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_GVGETFAIL == gtm_white_box_test_case_number))
		{
			status = cdb_sc_blknumerr;
			t_retry(status);
			continue;
		}
#endif
		if (cdb_sc_normal == (status = gvcst_search(gv_currkey, NULL)))
		{
			bh = gv_target->hist.h;
			if ((key_size = gv_currkey->end + 1) == bh->curr_rec.match)
			{
				/* The following code is duplicated in gvcst_dataget. Any changes here might need
				 * to be reflected there as well.
				 */
				bp = (blk_hdr_ptr_t)bh->buffaddr;
				b_top = bh->buffaddr + bp->bsiz;
				rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
				GET_USHORT(rsiz, &rp->rsiz);
				data_len = rsiz + EVAL_CMPC(rp) - SIZEOF(rec_hdr) - key_size;
				if ((0 > data_len) || ((sm_uc_ptr_t)rp + rsiz > b_top))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_rmisalign1;
				} else
				{
					if (!sn_ptr)
                                        {
                                                ENSURE_STP_FREE_SPACE(data_len);
                                                assert(stringpool.top - stringpool.free >= data_len);
                                                memcpy(stringpool.free, (sm_uc_ptr_t)rp + rsiz - data_len, data_len);
                                        } else
						memcpy(sn_ptr, (sm_uc_ptr_t)rp + rsiz - data_len, data_len);

					if (!dollar_tlevel)
					{
						if ((trans_num)0 == t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED))
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
					v->str.len = data_len;
					if (!sn_ptr)
					{
						v->str.addr = (char *)stringpool.free;
						stringpool.free += data_len;
						INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_get, 1);
					} else
						v->str.addr = (char *)sn_ptr;
					return TRUE;
				}
			} else
			{
				DEBUG_ONLY(TREF(ready2signal_gvundef) = in_op_gvget_lcl;)
				if (!dollar_tlevel)
				{
					if ((trans_num)0 == t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED))
					{
						assert(FALSE == TREF(ready2signal_gvundef)); /* t_end should have reset this */
						continue;
					}
				} else
				{
					status = tp_hist(NULL);
					if (cdb_sc_normal != status)
					{
						assert(FALSE == TREF(ready2signal_gvundef)); /* tp_hist should have reset this */
						t_retry(status);
						continue;
					}
				}
				assert(FALSE == TREF(ready2signal_gvundef));	/* t_end/tp_hist should have reset this up front */
				INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_get, 1);
				return FALSE;
			}
		}
		t_retry(status);
	}
}
