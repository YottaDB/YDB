/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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
#include "mlk_prcblk_add.h"
#include "mlk_shrclean.h"
#include "interlock.h"

#include "mlk_garbage_collect.h"

GBLREF	uint4		process_id;

error_def(ERR_LOCKSPACEFULL);
error_def(ERR_LOCKSPACEINFO);

boolean_t mlk_prcblk_add(gd_region *reg, mlk_ctldata_ptr_t ctl, mlk_shrblk_ptr_t d, uint4 pid, uint4 pstarttime)
{
	mlk_prcblk_ptr_t	pr;
	ptroff_t		*prpt;
	int			lcnt, crash_count;
	boolean_t		was_crit = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (prpt = (ptroff_t *)&d->pending, lcnt = ctl->max_prccnt; *prpt && lcnt; prpt = (ptroff_t *) &pr->next, lcnt--)
	{
		pr = (mlk_prcblk_ptr_t)R2A(*prpt);
		if (pr->process_id == pid)
		{
			pr->ref_cnt++;
			return TRUE;
		}
	}
	if (1 > ctl->prccnt)
	{	/* No process slot available. */
		if (!ctl->lockspacefull_logged)
		{ 	/* Needed to create a shrblk but no space was available. Resource starve. Print LOCKSPACEFULL to syslog
			 * and prevent printing LOCKSPACEFULL until (free space)/(lock space) ratio is above
			 * LOCK_SPACE_FULL_SYSLOG_THRESHOLD.
			 */
			ctl->lockspacefull_logged = TRUE;
			send_msg_csa(CSA_ARG(REG2CSA(reg)) VARLSTCNT(4) ERR_LOCKSPACEFULL, 2, DB_LEN_STR(reg));
			send_msg_csa(CSA_ARG(REG2CSA(reg)) VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(reg),
					(ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
					(ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt,
					(ctl->subfree - ctl->subbase), (ctl->subtop - ctl->subbase));
		}
		return FALSE;
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
	pr->process_start = pstarttime;
	pr->ref_cnt = 1;
	pr->next = 0;
	return TRUE;
}
