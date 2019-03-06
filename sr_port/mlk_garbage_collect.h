/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MLK_GARBAGE_COLLECT_DEFINED

#include "mdef.h"
#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "is_proc_alive.h"
#include "interlock.h"
#include "sleep.h"

/**
 * Attempts to get latch for garbage collection.
 *
 * Before calling this function, make sure to release the LOCK latch if held; this will spin sleep
 *  until it get the latch for the GC. After this function, reacquire the LOCK latch.
 */
static inline void prepare_for_gc(mlk_pvtctl_ptr_t pctl)
{
	GBLREF uint4		process_id;
	boolean_t		died;
	pid_t			old_gc;
	mlk_ctldata_ptr_t	ctl;

	assert(!LOCK_CRIT_HELD(pctl->csa));

	ctl = pctl->ctl;
	while (TRUE)
	{
		died = FALSE;
		old_gc = ctl->lock_gc_in_progress.u.parts.latch_pid;
		if (old_gc == process_id)
			break;
		while (old_gc && !died)
		{
			SLEEP_USEC(1, FALSE);
			old_gc = ctl->lock_gc_in_progress.u.parts.latch_pid;
			assert(old_gc != process_id);
			if (old_gc)
				died = !is_proc_alive(old_gc, 0);
		}
		if (died)
			COMPSWAP_UNLOCK(&ctl->lock_gc_in_progress, old_gc, 0, LOCK_AVAILABLE, 0);
		if((died || old_gc == 0) && GET_SWAPLOCK(&ctl->lock_gc_in_progress))
			break;
	}
}


/* Declare parms for mlk_garbage_collect.c */

void mlk_garbage_collect(mlk_pvtblk *p,
			 uint4 size,
			 boolean_t force);

#define MLK_GARBAGE_COLLECT_DEFINED

#endif
