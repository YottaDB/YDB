/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mlkdef.h"
#include "locklits.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_unlock.h"
#include "mlk_pvtblk_delete.h"

GBLREF mlk_pvtblk *mlk_cm_root;
GBLREF unsigned short cm_cmd_lk_ct;
GBLREF connection_struct *curr_entry;

void gtcml_zdeallocate(void)
{
	int locks_done;
	mlk_pvtblk	**prior;

	gtcml_lckclr();

	for (prior = &mlk_cm_root, locks_done = 0; locks_done < cm_cmd_lk_ct ||
		 (!cm_cmd_lk_ct && *prior); locks_done++)
	{
		if (!(*prior)->granted	|| ((*prior)->nodptr->auxowner != (UINTPTR_T)curr_entry))
			mlk_pvtblk_delete(prior);
		else
		{
			if (!((*prior)->level))
			{
				mlk_unlock(*prior);
				mlk_pvtblk_delete(prior);
			} else
			{
				(*prior)->zalloc = FALSE;
				prior = &((*prior)->next);
			}
		}
	}
	return;
}
