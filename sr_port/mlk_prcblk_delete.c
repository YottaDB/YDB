/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

void mlk_prcblk_delete(mlk_ctldata_ptr_t ctl,
		       mlk_shrblk_ptr_t d,
		       uint4 pid)
{
	mlk_prcblk_ptr_t	pr;
	ptroff_t		*prpt;

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
				A2R(pr->next, R2A(ctl->prcfree));
				A2R(ctl->prcfree, pr);
				assert(ctl->prcfree >= 0);
				ctl->prccnt++;
				if (0 != pid)
					break;
		} else
				prpt = (ptroff_t *) &pr->next;
	}
	return;
}
