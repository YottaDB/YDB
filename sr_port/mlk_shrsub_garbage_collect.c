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

#include <stddef.h>
#include "mdef.h"
#include "gtm_string.h"
#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrsub_garbage_collect.h"
#include "have_crit.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	uint4		process_id;

void mlk_shrsub_garbage_collect(mlk_pvtctl_ptr_t pctl)
{
	int4 			delta, size;
	mlk_ctldata_ptr_t	ctl;
	mlk_shrblk_ptr_t	prior_bckp;
	mlk_shrsub_ptr_t	psub, sbase;
	ptroff_t                bckp;
	sm_uc_ptr_t		sfree;

	assert(LOCK_CRIT_HELD(pctl->csa) && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	ctl = pctl->ctl;
	sbase = (mlk_shrsub_ptr_t)R2A(ctl->subbase);
	assert((ctl->subbase - sizeof(ctl->subbase) <= ctl->subfree) && (ctl->subfree <= ctl->subtop));
	delta = 0;
	for (psub = sbase, sfree = (sm_uc_ptr_t)R2A(ctl->subfree);
		psub < (mlk_shrsub_ptr_t)sfree ; psub = (mlk_shrsub_ptr_t)((sm_uc_ptr_t) psub + size))
	{
		assertpro((psub >= sbase) && (psub < (mlk_shrsub_ptr_t)sfree));
		size = (int4)MLK_SHRSUB_SIZE(psub);
		bckp = psub->backpointer;
		if (0 == bckp)
			delta += size;
		else if (0 != delta)
		{
			prior_bckp = (mlk_shrblk_ptr_t)((sm_uc_ptr_t)&psub->backpointer + bckp);
			prior_bckp->value -= delta;
			bckp += delta;
			psub->backpointer = bckp;
			memmove((sm_uc_ptr_t)psub - delta, psub, size);
		}
	}
	ctl->subfree -= delta;
	return;
}
