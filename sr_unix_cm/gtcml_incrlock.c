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
#include "mlkdef.h"
#include "locklits.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcmlkdef.h"

LITREF uint4 starve_time[2];
GBLREF connection_struct *curr_entry;
GBLREF mlk_pvtblk *mlk_cm_root;
GBLREF unsigned short cm_cmd_lk_ct;

char gtcml_incrlock(reg)
cm_region_list *reg;
{
	mlk_pvtblk *x, *y;
	bool blocked;
	unsigned int wakeup, status;
	unsigned short locks_done, locks_bckout;
	void gtcml_lkstarve();
	error_def(CMERR_CMSYSSRV);

	for (blocked = FALSE ; !blocked; )
	{
		for (x = mlk_cm_root, locks_done = 0; locks_done < cm_cmd_lk_ct; x = x->next, locks_done++)
		{
			if (!(wakeup = mlk_lock(x,(uint4) curr_entry,
				curr_entry->state == CMMS_L_LKACQUIRE ? FALSE : TRUE)))
			{	x->level += x->translev;
				x->granted = TRUE;
			}
			else
			{
				blocked = TRUE;
				break;
			}
		}
		if (!blocked)
		{
			break;
		}

		for (y = mlk_cm_root, locks_bckout = 0; locks_bckout < locks_done ; y = y->next, locks_bckout++)
		{
			assert(y->granted && y != x);
			mlk_bckout(y,INCREMENTAL);
		}

		if (!x->nodptr)
		{
			sys_settimer(curr_entry, 1, gtcml_lkstarve);
			return STARVED;
		}
		/* insert in blocked structure */
		gtcml_blklck(reg, x, wakeup);
		return BLOCKED;
	}
	return GRANTED;
}
