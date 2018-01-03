/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "mutex.h"
#include "repl_msg.h"		/* needed for jnlpool_addrs_ptr_t */
#include "gtmsource.h"		/* needed for jnlpool_addrs_ptr_t */
#include "secshr_db_clnup.h"
#ifdef DEBUG
#include "caller_id.h"	/* for CRIT_TRACE macro */
#endif

GBLREF	short			crash_count;
GBLREF	volatile int4		crit_count;
GBLREF	uint4			process_id;
#ifdef DEBUG
GBLREF	node_local_ptr_t	locknl;
GBLREF	jnl_gbls_t		jgbl;
#endif

/* Routine to release crit on a db or jnlpool region ("reg" parameter) called from "secshr_db_clnup".
 * "is_repl_reg" is TRUE if "reg" is a jnlpool region and FALSE if it is a db region.
 * "is_exiting" is TRUE if the caller is secshr_db_clnup(NORMAL_TERMINATION)
 *	and FALSE if the caller is secshr_db_clnup(COMMIT_INCOMPLETE).
 */
void secshr_rel_crit(gd_region *reg, boolean_t is_exiting, boolean_t is_repl_reg)
{
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;
	int			crashcnt;

#	ifdef DEBUG
	if (!is_repl_reg)
	{
		assert(NULL != reg);
		assert(reg->open);
	}
#	endif
	if (is_repl_reg && ((NULL == reg) || !reg->open))
		return;
	csa = REG2CSA(reg);
	assert(NULL != csa);
	/* ONLINE ROLLBACK can come here holding crit, only due to commit errors (COMMIT_INCOMPLETE) but NOT during
	 * process exiting as "secshr_db_clnup" during process exiting is always preceded by "mur_close_files" which
	 * does the "rel_crit" anyways. Assert that.
	 */
	assert(!csa->now_crit || !csa->hold_onto_crit || !jgbl.onlnrlbk || !is_exiting);
	cnl = csa->nl;
	if (!csa->hold_onto_crit || is_exiting)
	{	/* Release crit but since it involves modifying more than one field, make sure we prevent interrupts while
		 * in this code. The global variable "crit_count" does this for us. See similar usage in rel_crit.c.
		 */
		assert(0 == crit_count);
		crit_count++;	/* prevent interrupts */
		CRIT_TRACE(csa, crit_ops_rw); /* see gdsbt.h for comment on placement */
		if (cnl->in_crit == process_id)
			cnl->in_crit = 0;
		csa->hold_onto_crit = FALSE;
		DEBUG_ONLY(locknl = cnl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		crashcnt = (is_repl_reg ? 0 : crash_count);
		mutex_unlockw(reg, crashcnt);	/* roll forward Step (CMT15) */
		assert(!csa->now_crit);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		crit_count = 0;
	}
}
