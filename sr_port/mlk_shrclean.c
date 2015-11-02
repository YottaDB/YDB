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

#ifdef VMS
#include <ssdef.h>
#endif
#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "lockdefs.h"

/* Include prototypes */
#include "mlk_prcblk_delete.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "mlk_shrclean.h"
#include "is_proc_alive.h"

GBLREF uint4	process_id;

void mlk_shrclean(gd_region *region,
		  mlk_ctldata_ptr_t ctl,
		  mlk_shrblk_ptr_t d)
{
	mlk_shrblk_ptr_t	d0, d1;
	mlk_prcblk_ptr_t 	p;
	int4			status, lcnt = 0, max_loop_tries;
	unsigned int		time[2],icount;
	bool			delete_status;
	sgmnt_addrs		*csa;

	max_loop_tries = (int4)(((sm_uc_ptr_t)R2A(ctl->subtop) - (sm_uc_ptr_t)ctl) / SIZEOF(mlk_shrblk));
		/* although more than the actual, it is better than underestimating */

	for (d = d0 = (mlk_shrblk_ptr_t)R2A(d->rsib), d1 = NULL; d != d1 && max_loop_tries > lcnt; lcnt++)
	{
		delete_status = FALSE;
		if (d0->children)
			mlk_shrclean(region, ctl, (mlk_shrblk_ptr_t)R2A(d0->children));
		d1 = (mlk_shrblk_ptr_t)R2A(d0->rsib);
		if (d0->pending)
		{
			for (p = (mlk_prcblk_ptr_t)R2A(d0->pending); ; p = (mlk_prcblk_ptr_t)R2A(p->next))
			{
				if (PENDING_PROC_ALIVE(p,time,icount,status))
				{	/* process pending does not exist, free prcblk */
					p->process_id = 0;
					p->ref_cnt = 0;
				}
				if (p->next == 0)
					break;
			}
		}
		mlk_prcblk_delete(ctl, d0, 0);
		if (d0->owner)
		{
			if (PROC_ALIVE(d0,time,icount,status))
			{	/* process that owned lock has left image, free lock */
				csa = &FILE_INFO(region)->s_addrs;
				d0->owner = 0;
				d0->sequence = csa->hdr->trans_hist.lock_sequence++;
				delete_status = mlk_shrblk_delete_if_empty(ctl,d0);
			}
		}else
			delete_status = mlk_shrblk_delete_if_empty(ctl,d0);

		if (delete_status && d0 == d)
		{
			d = d0 = (d0 == d1) ? NULL : d1;
			d1 = NULL;
		}
		else
			d0 = d1;
	}
	return;
}
