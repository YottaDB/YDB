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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "mlk_prcblk_add.h"
#include "mlk_shrclean.h"

void mlk_prcblk_add(gd_region *reg,
		    mlk_ctldata_ptr_t ctl,
		    mlk_shrblk_ptr_t d,
		    uint4 pid)
{
	mlk_prcblk_ptr_t	pr;
        ptroff_t		*prpt;
	int			lcnt;

	error_def(ERR_LOCKSPACEFULL);
	for (prpt = (ptroff_t *)&d->pending, lcnt = FILE_INFO(reg)->s_addrs.hdr->lock_space_size / PRC_FACTOR;
			*prpt && lcnt; prpt = (ptroff_t *) &pr->next, lcnt--)
	{
		pr = (mlk_prcblk_ptr_t)R2A(*prpt);
		if (pr->process_id == pid)
		{
			pr->ref_cnt++;
			return;
		}
	}
	if (!lcnt)
	{
		send_msg(VARLSTCNT(4) ERR_LOCKSPACEFULL, 2, REG_LEN_STR(reg));
		rts_error(VARLSTCNT(4) ERR_LOCKSPACEFULL, 2, REG_LEN_STR(reg));
	}
	if (ctl->prccnt < 1)
	{
		mlk_shrclean(reg, ctl, (mlk_shrblk_ptr_t)R2A(ctl->blkroot));
		if (ctl->prccnt < 1)
			return;
	}
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
