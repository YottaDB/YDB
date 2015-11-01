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

/* Include prototypes */
#include "mlk_prcblk_delete.h"
#include "mlk_unpend.h"

GBLREF int4 process_id;
GBLREF short	crash_count;

void mlk_unpend(mlk_pvtblk *p)
{
	bool			was_crit;
	sgmnt_addrs		*sa;

	sa = &FILE_INFO(p->region)->s_addrs;

	if (sa->critical)
		crash_count = sa->critical->crashcnt;

        if(FALSE == (was_crit = sa->now_crit))
		grab_crit(p->region);

	mlk_prcblk_delete(p->ctlptr, p->nodptr, process_id);
	p->ctlptr->wakeups++;

        if(FALSE == was_crit)
		rel_crit(p->region);

	return;
}
