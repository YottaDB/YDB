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
#include "locklits.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_unlock.h"

GBLREF connection_struct	*curr_entry;
GBLREF mlk_pvtblk		*mlk_cm_root;
GBLREF unsigned short		cm_cmd_lk_ct;

void gtcml_decrlock(void)
{
	int		locks_done;
	mlk_pvtblk	**prior;

	gtcml_lckclr();
	for (prior = &mlk_cm_root, locks_done = 0; locks_done < cm_cmd_lk_ct; locks_done++)
	{

		if (!(*prior)->granted || ((*prior)->nodptr->auxowner != (uint4)curr_entry))
			mlk_pvtblk_delete(prior);
		else
		{
			if (!(*prior)->zalloc && (*prior)->level <= (*prior)->translev)
			{
				mlk_unlock(*prior);
				mlk_pvtblk_delete(prior);
			} else
			{
				if ((*prior)->level <= (*prior)->translev)
					(*prior)->level = 0;
				else
					(*prior)->level -= (*prior)->translev;
				prior = &((*prior)->next);
			}
		}
	}
	return;
}
