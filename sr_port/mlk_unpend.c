/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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

/* Include prototypes */
#include "mlk_prcblk_delete.h"
#include "mlk_unpend.h"
#include "interlock.h"
#include "rel_quant.h"

GBLREF int4 process_id;
GBLREF short	crash_count;

void mlk_unpend(mlk_pvtblk *p)
{
	bool			was_crit;
	sgmnt_addrs		*csa;

	csa = &FILE_INFO(p->region)->s_addrs;
	GRAB_LOCK_CRIT(csa, p->region, was_crit);
	mlk_prcblk_delete(p->ctlptr, p->nodptr, process_id);
	p->ctlptr->wakeups++;
	REL_LOCK_CRIT(csa, p->region, was_crit);
	return;
}
