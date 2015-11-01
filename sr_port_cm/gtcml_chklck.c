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

#ifdef VMS
#include <ssdef.h>
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "lockdefs.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "is_proc_alive.h"
#include "gtcml.h"

GBLREF gd_region	*gv_cur_region;
GBLREF uint4            process_id;
GBLREF short		crash_count;

void gtcml_chklck(cm_lckblkreg *reg, bool timed)
{
	cm_lckblklck	*lck, *lckroot, *lcktofree;
	cm_lckblkprc 	*prc, *prc1;
	mlk_shrblk_ptr_t d;
	boolean_t	stop_waking, timeout, was_crit;
	sgmnt_addrs	*csa;
	int4		icount, status, time[2];

	timeout = TRUE;
	lckroot = reg->lock;
	lck = lckroot->next;
	lcktofree = NULL;

	/* it appears that the design assumes that lck should never be null, but we have empirical that it happens.
	 * because we do not (at the moment of writing) have resources to pursue the discrepancy, we simply protect
	 * against it, and hope that we don't encounter other symptoms
	*/
	while (lck != NULL)
	{
		if (timed)
		{
			if (timeout = gtcml_lcktime(lck))
			{
				prc = prc1 = lck->prc;
				if (prc)
				{	/* Check if blocking processes exists. */
					do
					{	/* Note that previous code in this section would manipulate the sequence
						   numbers in the shared blocks rather than in the private blocks. This was
						   deemed to not always be safe, especially in the case of parent/child
						   locking since when a lock it held, the sequence number at its release
						   MUST match the sequence number at it's inception or the lock cannot and
						   will not be released by mlk_unlock. By always manipulating the sequence
						   number in private storage instead, we avoid this problem totally. se 8/2001 */
						if ((d = prc1->blocked))
						{	/* Blocking process shrblk exists. Check it under crit lock */
							csa = &FILE_INFO(gv_cur_region)->s_addrs;
							if (!(was_crit = csa->now_crit))
								grab_crit(gv_cur_region);
							if (d->sequence != prc1->blk_sequence)
							{	/* Blocking structure no longer ours - do artificial wakeup */
								lck->sequence = csa->hdr->trans_hist.lock_sequence++;
							} else if (d->owner)
							{	/* Blocking struct still has owner. Check if alive */
								if (PROC_ALIVE(d, time, icount, status))
								{	/* process that owned lock has died, free lock */
									d->owner = 0;
									lck->sequence = csa->hdr->trans_hist.lock_sequence++;
								}
							} else
							{	/* No longer any owner (lke stole?). Wake up */
								lck->sequence = csa->hdr->trans_hist.lock_sequence++;
							}
							if (!was_crit)
								rel_crit(gv_cur_region);
						}
						prc1 = prc1->next;
					} while (prc1 != prc);
				}
			}
		}
		if (timeout)
		{
			gtcml_chkprc(lck);
			if (lck->prc == 0)
			{
				lck->next->last = lck->last;
				lck->last->next = lck->next;
				if (lck->next == lckroot && lck->last == lckroot)
				{  /* no more lcks for reg */
					reg->lock = 0;
					free(lck);
					free(lckroot);    /* list header */
					break;
				}
				lcktofree = lck;   /* not yet */
			}
		}

		lck = lck->next;
		if (NULL != lcktofree)
		{
			free(lcktofree);
			lcktofree = NULL;
		}
		if (lck == lckroot)
			break;		/* at end of list */
	}
}
