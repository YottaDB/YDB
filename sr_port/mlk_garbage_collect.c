/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mlkdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlk_garbage_collect.h"
#include "mlk_shrclean.h"
#include "mlk_shrsub_garbage_collect.h"
#include "have_crit.h"

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	uint4		process_id;

void mlk_garbage_collect(mlk_pvtblk *p,
			 uint4 size,
			 boolean_t force)
{
	mlk_ctldata_ptr_t	ctl;

	assert(LOCK_CRIT_HELD(p->pvtctl.csa) && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	ctl = p->pvtctl.ctl;
	if (force || ctl->gc_needed || (ctl->subtop - ctl->subfree < size))
		mlk_shrsub_garbage_collect(&p->pvtctl);

	if (force || ctl->gc_needed || (ctl->blkcnt < p->subscript_cnt) || (ctl->subtop - ctl->subfree < size))
	{
		mlk_shrclean(&p->pvtctl);
		ctl = p->pvtctl.ctl;			// mlk_shrclean() can drop lock crit, so grab current ctl to be safe.
		mlk_shrsub_garbage_collect(&p->pvtctl);
	}
	ctl->gc_needed = FALSE;
	return;
}
