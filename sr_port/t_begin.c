/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h, cws_insert.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "t_begin.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"

GBLREF short 		crash_count;
GBLREF trans_num	start_tn;
GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth, cw_map_depth;
GBLREF unsigned int	t_tries;
GBLREF uint4		t_err;
GBLREF bool		update_trans;
GBLREF gv_namehead      	*gv_target;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF short            	dollar_tlevel;
GBLREF jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF jnl_gbls_t		jgbl;

void t_begin (uint4 err, boolean_t update_transaction) 	/* err --> error code for current gvcst_routine */
{
	srch_blk_status	*s;

	/* If we use a clue then we must consider the oldest tn in the search
	 * 	history to be the start tn for this transaction.
	 */
	CWS_RESET;
	update_trans = update_transaction;
	t_err = err;

        if (0 == dollar_tlevel)			/* start_tn manipulation for TP taken care of in tp_hist */
	{
		if (cs_addrs->critical)
			crash_count = cs_addrs->critical->crashcnt;
		if (gv_target->clue.end)
		{
			start_tn = cs_addrs->ti->curr_tn;
			s = &gv_target->hist.h[0];
			if (start_tn > s[gv_target->hist.depth].tn)
				start_tn = s[gv_target->hist.depth].tn;
			DEBUG_ONLY(for (s = &gv_target->hist.h[0]; s->blk_num; s++)
					assert(start_tn <= s->tn);
			)
		} else
			start_tn = cs_addrs->ti->curr_tn;
		cw_set_depth = 0;
		cw_map_depth = 0;
		t_tries = 0;
		if (non_tp_jfb_ptr)
			non_tp_jfb_ptr->record_size = 0; /* re-initialize it to 0 since TOTAL_NONTPJNL_REC_SIZE macro uses it */
		/* do the following initialization for TP in op_tstart and not here since t_begin is called multiple times */
		jgbl.cumul_jnl_rec_len = 0;
		DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
	}
}
