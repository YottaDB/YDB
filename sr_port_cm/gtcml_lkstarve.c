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

#if defined(VMS)
#include <ssdef.h>
#endif

#if defined(UNIX)
#include <signal.h>
#endif

#include "mlkdef.h"
#include "locklits.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "gtcm_action_pending.h"

GBLREF relque			action_que;
GBLREF connection_struct	*curr_entry;
GBLREF struct NTD		*ntd_root;

#if defined(VMS)
void gtcml_lkstarve(connection_struct *connection)
#elif defined(UNIX)
void gtcml_lkstarve(TID timer_id, int4 data_len, connection_struct **connection)
#endif
{
        static int		on_queue_p = 0;   /* for debugging */
	connection_struct	*cnx;
	CMI_MUTEX_DECL;

	UNIX_ONLY(cnx = *connection;)
	VMS_ONLY(cnx = connection;)
#ifdef UNIX
	/* no need to disable ASTs on VMS since the timer handler gtcml_lkstarve runs as an AST. On Unix though, timer handlers
	 * block only SIGALRM. Here, we need to block the CMI signals as well. */
	CMI_MUTEX_BLOCK;
#endif
	if (curr_entry != cnx && !cnx->waiting_in_queue)
	{
		gtcm_action_pending(cnx);
		cnx->new_msg = FALSE;
		VMS_ONLY(if (action_que.bl == action_que.fl) GT_WAKE;)	/* if only message on queue kick it */
	} else
		on_queue_p++;
#ifdef UNIX
	CMI_MUTEX_RESTORE;
#endif
}
