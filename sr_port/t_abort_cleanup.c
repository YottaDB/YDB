/****************************************************************
 *								*
 * Copyright (c) 2004-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "filestruct.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "hashtab_int4.h"	/* needed for cws_insert.h */
#include "cws_insert.h"		/* for CWS_RESET macro */
#include "gdsblkops.h"		/* for RESET_UPDATE_ARRAY macro */
#include "t_abort.h"		/* for prototype of "t_abort" */

GBLREF	unsigned char	cw_set_depth;
GBLREF	unsigned int	t_tries;
GBLREF	uint4		update_trans;
GBLREF	boolean_t	need_kip_incr;
GBLREF	uint4		dollar_tlevel;
GBLREF	inctn_opcode_t	inctn_opcode;
GBLREF	char		*update_array;
GBLREF	boolean_t	mu_reorg_process;

/* This function does cleanup that is common to "t_abort" and "preemptive_db_clnup" */
void	t_abort_cleanup(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Clear "inctn_opcode" global variable now that any in-progress transaction is aborted at this point.
	 * Not doing so would cause future calls to "t_end" to get confused and skip writing logical jnl recs
	 * and instead incorrectly write an INCTN record (GTM-8425).
	 * MUPIP REORG sets "inctn_opcode" once and expects it to stay that way for all of its non-tp transactions
	 * even though it calls "t_abort" many times in between. So skip clearing this global just for that.
	 * A better fix would be for "inctn_opcode" to be set to "inctn_mu_reorg" at the start of each reorg transaction.
	 * But that requires more work and not clear if that is worth the time.
	 */
	if (!mu_reorg_process)
		inctn_opcode = inctn_invalid_op;
	/* If called from "preemptive_db_clnup" (because of an error in t_end (e.g. GBLOFLOW)), we don't want "need_kip_incr"
	 * to get carried over to the next non-TP transaction that this process does (e.g. inside an error trap).
	 * If called from "t_abort" (e.g. if we get a "kill -15" in t_end that was finishing phase1 of an M-kill), we don't want
	 * this global variable to get carried over to the next non-TP transaction that this process does (e.g. removing ^%YGS
	 * node from a statsdb as part of exit handling logic). So reset it in both cases.
	 */
	need_kip_incr = FALSE;
	TREF(expand_prev_key) = FALSE;	/* reset global (in case it is TRUE) so it does not get carried over to future operations */
	if (!dollar_tlevel)
	{
		/* "secshr_db_clnup/t_commit_cleanup" assume an active non-TP transaction if cw_set_depth is non-zero or if
		 * update_trans has the UPDTRNS_TCOMMIT_STARTED_MASK bit set. Now that the transaction is aborted, reset them.
		 */
		cw_set_depth = 0;
		if (update_trans)
		{	/* It's possible we hit an error in the middle of an update transaction, at which point we have
			 * a valid clue and non-NULL cse. However, this causes problems for subsequent
			 * transactions (see comment in t_begin). In particular we could end up pinning buffers
			 * unnecessarily. So clear the cse of any histories that may have been active during the update.
			 */
			CLEAR_CSE(gv_target);
			if ((NULL != gv_target) && (NULL != gv_target->gd_csa))
			{
				CLEAR_CSE(gv_target->gd_csa->dir_tree);
				GTMTRIG_ONLY(CLEAR_CSE(gv_target->gd_csa->hasht_tree));
			}
			/* Resetting this is needed to not fail an assert in t_begin that it is 0 at the start of a transaction */
			update_trans = 0;
		}
		/* It is possible no CWS_INIT was done in which case CWS_RESET should not be called.
		 * cw_stagnate.size would be 0 in that case. So check before calling CWS_RESET.
		 */
		if (cw_stagnate.size)
			CWS_RESET;
		/* Reset update_array_ptr to update_array.
		 * Do not use CHECK_AND_RESET_UPDATE_ARRAY since cw_set_depth can be non-zero.
		 * It is possible "update_array" could be NULL (no transaction was ever started in this process)
		 * so check that before invoking the macro which assumes it is non-NULL.
		 */
		if (NULL != update_array)
			RESET_UPDATE_ARRAY;
		t_tries = 0;
	}
}
