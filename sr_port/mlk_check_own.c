/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <sys/shm.h>

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "filestruct.h"
#include "lockdefs.h"
#include "mlk_check_own.h"
#include "is_proc_alive.h"
#include "interlock.h"
#include "do_shmat.h"
#include "mlk_ops.h"
#include "mlk_wake_pending.h"
#include "have_crit.h"

GBLREF	intrpt_state_t  intrpt_ok_state;
#ifdef DEBUG
GBLREF	unsigned int	t_tries;
#endif

/*
 * ------------------------------------------------
 * Check if owner process of the lock is still alive
 * If the process is not alive, clear the lock.
 *
 * Return:
 *	TRUE - cleared the owner
 *	FALSE - otherwise
 * ------------------------------------------------
 */
boolean_t	mlk_check_own(mlk_pvtblk *x)
{
	boolean_t	ret_val, was_crit;
	int4		icount, status, time[2];
	intrpt_state_t	prev_intrpt_state;
	sgmnt_addrs	*csa;

	if (!x->blocked)
		return FALSE;
	csa = x->pvtctl.csa;
	GRAB_LOCK_CRIT_AND_SYNC(&x->pvtctl, was_crit);
	DEFER_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
	assert((csa->lock_crit_with_db) || !csa->now_crit || (CDB_STAGNATE <= t_tries));
	ret_val = FALSE;
	if (x->blocked->owner)
	{	/* There is an owner for the blocking node */
		if (x->blocked->sequence != x->blk_sequence)
		{	/* The node we were blocking on has been reused for something else so
			   we are no longer blocked on it and can pretend that the process
			   holding the lock went away */
			ret_val = TRUE;
		} else if (BLOCKING_PROC_DEAD(x, time, icount, status))
		{	/* process that owned lock has died, free lock. */
			x->blocked->owner = 0;
			csa->hdr->trans_hist.lock_sequence++;
			ret_val = TRUE;
		}
	} else if (x->blocked->pending)
		mlk_wake_pending(&x->pvtctl, x->blocked);
	else
		ret_val = TRUE;	/* There is no owner. Take credit for freeing it.. */
	REL_LOCK_CRIT(&x->pvtctl, was_crit);
	ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
	return ret_val;
}
