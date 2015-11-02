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

GBLREF uint4 process_id;

#define NODENUMBER 0xFFE00000
#define DO_CRIT_WAKE										\
{												\
	crit_wake_res = pr->process_id ? crit_wake((sm_uint_ptr_t)&pr->process_id) : GONE;	\
	if (GONE == crit_wake_res)								\
	{											\
		pr->ref_cnt = 1;								\
		mlk_prcblk_delete(ctl, d, *((sm_uint_ptr_t)&pr->process_id));			\
	}											\
}

void mlk_wake_pending(mlk_ctldata_ptr_t ctl,
		      mlk_shrblk_ptr_t d,
		      gd_region *reg)
{
	mlk_prcblk_ptr_t	next, pr;
	sm_uint_ptr_t 		empty_slot, ctop;
	sgmnt_addrs		*csa;
	boolean_t		remote_pid;
	int 			crit_wake_res; /* also used in macro DO_CRIT_WAKE */
	int 			lcnt;

	csa = &FILE_INFO(reg)->s_addrs;
	if (!d->pending)
		return;
	ctl->wakeups++;
	/* Before updating d->sequence ensure there is no process owning this lock, since otherwise when the owner process attempts
	 * to release the lock it will fail as its private copy of "p->sequence" will not match the shared memory "d->sequence".
	*/
	assert(!d->owner);
	d->sequence = csa->hdr->trans_hist.lock_sequence++;	/* This node is being awakened (GTCM) */
	BG_TRACE_PRO_ANY(csa, mlock_wakeups);			/* Record halted slumbers */
	if (reg->dyn.addr->acc_meth == dba_bg &&
		csa->hdr->clustered)
	{
		remote_pid = FALSE;
		for (empty_slot = ctl->clus_pids,
			ctop = &ctl->clus_pids[NUM_CLST_LCKS-1];
			*empty_slot && empty_slot <= ctop; empty_slot++)
			;
		for (pr = (mlk_prcblk_ptr_t)R2A(d->pending), lcnt = ctl->max_prccnt; lcnt; lcnt--)
		{
			next = (pr->next) ? (mlk_prcblk_ptr_t)R2A(pr->next) : 0;	/* in case it's deleted */
			if ((pr->process_id & NODENUMBER)  ==  (process_id & NODENUMBER))
			{
				DO_CRIT_WAKE;
			} else if (empty_slot <= ctop)
			{
				remote_pid = TRUE;
				*empty_slot = pr->process_id;
				empty_slot++;
			}
			if (next)
				pr = next;
			else
				break;
		}
		if (remote_pid)
			ccp_cluster_lock_wake(reg);
	} else
	{
		for (pr = (mlk_prcblk_ptr_t)R2A(d->pending), lcnt = ctl->max_prccnt; lcnt; lcnt--)
		{
			next = (pr->next) ? (mlk_prcblk_ptr_t)R2A(pr->next) : 0;	/* in case it's deleted */
			DO_CRIT_WAKE;

			/* Wake one process to keep things orderly, if it loses its way, others
			 * will jump in after a timout */
			if (GONE == crit_wake_res && next)
				pr = next;
			else
				break;
		}
	}
	/* The assertpro is to safeguard us against cycles/loops in the "pending" linked list. This way we dont get into an
	 * infinite loop and yet get a core dump to see how we got ourselves into this out-of-design state.
	 */
	assertpro(lcnt);
	return;
}
