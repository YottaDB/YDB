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
#include "mlk_prcblk_add.h"
#include "mlk_shrclean.h"

error_def(ERR_LOCKSPACEFULL);
error_def(ERR_LOCKSPACEINFO);

void mlk_prcblk_add(gd_region *reg,
		    mlk_ctldata_ptr_t ctl,
		    mlk_shrblk_ptr_t d,
		    uint4 pid)
{
	mlk_prcblk_ptr_t	pr;
        ptroff_t		*prpt;
	int			lcnt;

	for (prpt = (ptroff_t *)&d->pending, lcnt = ctl->max_prccnt; *prpt && lcnt; prpt = (ptroff_t *) &pr->next, lcnt--)
	{
		pr = (mlk_prcblk_ptr_t)R2A(*prpt);
		if (pr->process_id == pid)
		{
			pr->ref_cnt++;
			return;
		}
	}
	if (1 > ctl->prccnt)
	{	/* No process slot available. */
		mlk_shrclean(reg, ctl, (mlk_shrblk_ptr_t)R2A(ctl->blkroot));
		if (1 > ctl->prccnt)
		{	/* Process cleanup did not help. Issue error to syslog. */
			send_msg(VARLSTCNT(4) ERR_LOCKSPACEFULL, 2, DB_LEN_STR(reg));
			if (ctl->subtop > ctl->subfree)
				send_msg(VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(reg),
					 (ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
					 (ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt, LEN_AND_LIT(" not "));
			else
				send_msg(VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(reg),
					 (ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
					 (ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt, LEN_AND_LIT(" "));
			return;
		}
	}
	/* Process slot is available. Add process to the queue. */
	ctl->prccnt--;
	pr = (mlk_prcblk_ptr_t)R2A(ctl->prcfree);
	if (0 == pr->next)
	{
		assert(0 == ctl->prccnt);
		ctl->prcfree = 0;
	} else
		A2R(ctl->prcfree, R2A(pr->next));
	A2R(*prpt, pr);
	pr->process_id = pid;
	pr->ref_cnt = 1;
	pr->next = pr->unlock = 0;
	return;
}
