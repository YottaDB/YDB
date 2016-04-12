/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "lockconst.h"
#include "copy.h"
#include "interlock.h"
#include "relqueopi.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_action_pending.h"

GBLREF	relque			action_que;
GBLREF	gd_region		*action_que_dummy_reg;
GBLREF	node_local_ptr_t	locknl;

/* gtcm_action_pending - insert action into action queue and set flag so the same action cannot be inserted more than once.
 * N.B.  gtcm_action_pending should only be invoked from an AST (or with ASTs disabled).
 */

long gtcm_action_pending(connection_struct *c)
{
	long		status = 0;

	/* assert (lib$ast_in_prog()); */
	if (!c->waiting_in_queue)
	{
		UNIX_ONLY(DEBUG_ONLY(locknl = FILE_INFO(action_que_dummy_reg)->s_addrs.nl;))	/* for DEBUG_ONLY LOCK_HIST macro */
		status = INSQTI(c, &action_que);
		UNIX_ONLY(DEBUG_ONLY(locknl = NULL;))	/* restore "locknl" to default value */
		if (INTERLOCK_FAIL == status)
			return status;
		c->waiting_in_queue = TRUE;
	}
	return status;
}
