/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gdsbgtr.h"
#ifdef UNIX
#include <errno.h>
#define GONE	ESRCH
#else
#include <ssdef.h>
#define GONE	SS$_NONEXPR
#endif
#include "ccp.h"
#include "mlk_wake_pending.h"
#include "crit_wake.h"
#include "ccp_cluster_lock_wake.h"
#include "mlk_prcblk_delete.h"
#include "wbox_test_init.h"

GBLREF uint4 process_id;

#define NODENUMBER 0xFFE00000

void mlk_wake_pending(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t d)
{
	boolean_t		remote_pid;
	int 			crit_wake_res, lcnt;
	mlk_prcblk_ptr_t	next, pr;
	sgmnt_addrs		*csa;
	sm_uint_ptr_t 		empty_slot, ctop;

	csa = pctl->csa;
	if (!d->pending)
		return;
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled && (WBTEST_LCKWAKEOVRFLO == ydb_white_box_test_case_number))
		pctl->ctl->wakeups = (0xFFFFFFFFF > pctl->ctl->wakeups) ? 0xFFFFFFFFF :  0xFFFFFFFFFFFFFFFFLL;
#	endif
	pctl->ctl->wakeups++;
	if (!pctl->ctl->wakeups)	/* mlk_lock returns 0 as success, and otherwise return this for use by gtmcm logic */
		 pctl->ctl->wakeups++;	/* hence guard against wrap, as unlikely as that should be */
	/* Before updating d->sequence ensure there is no process owning this lock, since otherwise when the owner process attempts
	 * to release the lock it will fail as its private copy of "p->sequence" will not match the shared memory "d->sequence".
	*/
	assert(!d->owner);
	d->sequence = csa->hdr->trans_hist.lock_sequence++;	/* This node is being awakened (GTCM) */
	BG_TRACE_PRO_ANY(csa, mlock_wakeups);			/* Record halted slumbers */
	for (pr = (mlk_prcblk_ptr_t)R2A(d->pending), lcnt = pctl->ctl->max_prccnt; lcnt; lcnt--)
	{
		next = (pr->next) ? (mlk_prcblk_ptr_t)R2A(pr->next) : 0;	/* in case it's deleted */
		crit_wake_res = pr->process_id ? crit_wake((sm_uint_ptr_t)&pr->process_id) : GONE;
		if (GONE == crit_wake_res)
		{
			pr->ref_cnt = 1;
			mlk_prcblk_delete(pctl, d, *((sm_uint_ptr_t)&pr->process_id));
		}
		/* Wake one process to keep things orderly, if it loses its way, others will jump in after a timeout */
		if (GONE == crit_wake_res && next)
			pr = next;
		else
			break;
	}
	/* The assertpro is to safeguard us against cycles/loops in the "pending" linked list. This way we dont get into an
	 * infinite loop and yet get a core dump to see how we got ourselves into this out-of-design state.
	 */
	assertpro(lcnt);
	return;
}
