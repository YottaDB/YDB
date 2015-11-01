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
#include "cmidef.h"
#include "hashdef.h"
#include "cmmdef.h"
#include "mlkdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_unlock.h"
#include "mlk_pvtblk_delete.h"

GBLREF mlk_pvtblk *mlk_cm_root;
GBLREF connection_struct *curr_entry;

void gtcml_unlock(void)
{
	mlk_pvtblk **prior;

	for (prior = &mlk_cm_root ; *prior ; )
	{
		if (!(*prior)->granted	|| ((*prior)->nodptr->auxowner != (uint4) curr_entry))
		{
			mlk_pvtblk_delete(prior);
		}
		else if ((*prior)->zalloc)
		{
			(*prior)->level = 0;
			prior = &((*prior)->next);
		} else
		{
			mlk_unlock(*prior);
			mlk_pvtblk_delete(prior);
		}
	}
	return;
}
