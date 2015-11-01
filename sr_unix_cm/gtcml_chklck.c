/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcmlkdef.h"

GBLREF gd_region	*gv_cur_region;
GBLREF uint4		process_id;
GBLREF short		crash_count;

void gtcml_chklck(cm_lckblkreg *reg, bool timed)
{
	boolean_t		found, timeout, gtcml_lcktime(cm_lckblklck *);
	cm_lckblklck		*lck, *lckroot;
	cm_lckblkprc		*prc, *prc1;
	int4			icount, time[2], status;
	mlk_ctldata		*ctl;
	mlk_shrblk_ptr_t	d;
	sgmnt_addrs		*csa;

	found = FALSE;
	timeout = TRUE;
	lckroot = lck = reg->lock;
	lck = lck->next;

	while (!found)
	{
		if (timed)
		{
			if (timeout = gtcml_lcktime(lck))
			{
				prc = prc1 = lck->prc;
				if (prc)
				{
					do
					{
						d = prc1->blocked; /* check blocking lock's owner */
						if (d)
						{
							if (d->owner)
							{
								grab_crit(gv_cur_region);
								csa = &FILE_INFO(gv_cur_region)->s_addrs;
								ctl = (mlk_ctldata *)csa->lock_addrs[0];
								if (!is_proc_alive(d->owner))
								{	/* process that owned lock has died, free lock */
										d->owner = 0;
										d->sequence = csa->hdr->trans_hist.lock_sequence++;
								}
								rel_crit(gv_cur_region);
							}
							if (!d->owner)
								lck->node->sequence = d->sequence;
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
				{
					reg->lock = 0;
					found = TRUE;
				}
				free(lck);
			}
		}
		if (found || lck->next == lckroot)
			break;
		lck = lck->next;
	}
	if (found)
		free(lckroot);
}
