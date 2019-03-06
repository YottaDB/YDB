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

#include "mdef.h"

#include "gtm_string.h"

#include "mlkdef.h"
#include "mlk_prcblk_delete.h"
#include "min_max.h"

void mlk_prcblk_delete(mlk_pvtctl_ptr_t pctl,
		       mlk_shrblk_ptr_t d,
		       uint4 pid)
{
	mlk_prcblk_ptr_t	pr;
	ptroff_t		*prpt;
	float			ls_free;	/* Free space in bottleneck subspace */
	mlk_ctldata_ptr_t	ctl;

	ctl = pctl->ctl;
	for (prpt = (ptroff_t *)&d->pending; *prpt; )
	{
		pr = (mlk_prcblk_ptr_t)R2A(*prpt);
		if ((pr->process_id == pid) && (--pr->ref_cnt <= 0))
		{
				pr->ref_cnt = 0;
				if (pr->next == 0)
					*prpt = 0;
				else
					A2R(*prpt, R2A(pr->next));
				memset(pr, 0, SIZEOF(*pr));
				pr->next = 0;
				if (ctl->prcfree)
					A2R(pr->next, R2A(ctl->prcfree));
				A2R(ctl->prcfree, pr);
				assert(ctl->prcfree >= 0);
				ctl->prccnt++;
				if (0 != pid)
					break;
		} else
				prpt = (ptroff_t *) &pr->next;
	}
	ls_free = MIN(((float)ctl->blkcnt) / ctl->max_blkcnt, ((float)ctl->prccnt) / ctl->max_prccnt);
	if (ls_free >= LOCK_SPACE_FULL_SYSLOG_THRESHOLD)
		ctl->lockspacefull_logged = FALSE; /* Allow syslog writes if enough free space is established. */
	return;
}
