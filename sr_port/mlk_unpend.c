/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <sys/shm.h>

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "do_shmat.h"

/* Include prototypes */
#include "mlk_ops.h"
#include "mlk_prcblk_delete.h"
#include "mlk_unpend.h"
#include "interlock.h"
#include "rel_quant.h"

GBLREF uint4	process_id;
GBLREF short	crash_count;

void mlk_unpend(mlk_pvtblk *p)
{
	boolean_t		was_crit;
	sgmnt_addrs		*csa;

	csa = p->pvtctl.csa;
	GRAB_LOCK_CRIT_AND_SYNC(p->pvtctl, was_crit);
	mlk_prcblk_delete(&p->pvtctl, p->nodptr, process_id);
	/* p->pvtctl.ctl->wakeups++; removed, which might matter to gtcml_chkreg, seems unneeded, certainly within LOCK crit */
	REL_LOCK_CRIT(p->pvtctl, was_crit);
	return;
}
