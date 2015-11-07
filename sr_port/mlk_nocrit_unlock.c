/****************************************************************
 *								*
 * Copyright (c) 2001, 2015 Fidelity National Information	*
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
#include "mlkdef.h"
#include "mlk_unlock.h"

GBLREF	int4 		process_id;

/* This function is similar to "mlk_unlock" except that it does not get crit. So does what can be safely done
 * and leaves the rest to be done by the next guy who has crit and wants this lock. Note that this means processes
 * that are waiting on this lock might not be woken up right away but will eventually wake up in their sleep-poll wait loop.
 * But that is considered okay given the merits of this quick nocrit unlock (speedy process exit, no crit contention).
 */
void mlk_nocrit_unlock(mlk_pvtblk *p)
{
	mlk_shrblk_ptr_t	d;
	sgmnt_addrs		*csa;
#	ifdef DEBUG
	mlk_ctldata_ptr_t	ctl;
#	endif

	assert(p->region->dyn.addr->acc_meth != dba_usr);
	DEBUG_ONLY(ctl = p->ctlptr;)
	assert((ctl->max_blkcnt > 0) && (ctl->max_prccnt > 0) && ((ctl->subtop - ctl->subbase) > 0));
	csa = &FILE_INFO(p->region)->s_addrs;
	d = p->nodptr;
	if ((d->owner == process_id) && (p->sequence == d->sequence))
	{
		d->sequence = csa->hdr->trans_hist.lock_sequence++;	/* bump sequence so waiters realize this lock is released */
		d->owner = 0;	/* Setting this marks the lock as available */
		/* Note: The key unlock operation is setting d->owner to 0. The shared sequence increment can happen
		 * before or after that. It is only a fast way to signal lock waiters of this unlock. Even if the sequence
		 * increment actually happens way after the d->owner=0 (due to out-of-order executions), the worst is the
		 * lock waiter might have waited a little more than necessary. No correctness issues.
		 */
	}
	return;
}
