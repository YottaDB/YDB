/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "mlk_lock.h"
#include "mlk_bckout.h"

GBLREF connection_struct 	*curr_entry;
GBLDEF mlk_pvtblk 		*mlk_cm_root = 0;
GBLDEF unsigned short		cm_cmd_lk_ct;

char gtcml_lock_internal(cm_region_list *reg, unsigned char action)
{
	mlk_pvtblk	*x, *y;
	unsigned int	wakeup, status;
	unsigned short	locks_done, locks_bckout;

	for (x = mlk_cm_root, locks_done = 0; locks_done < cm_cmd_lk_ct; x = x->next, locks_done++)
	{
		if (ZALLOCATED == action && x->old && !x->zalloc)
			x->old = FALSE;
		if (!(wakeup = mlk_lock(x, (UINTPTR_T)curr_entry, CMMS_L_LKACQUIRE == curr_entry->state ? FALSE : TRUE)))
		{
			switch (action)
			{
				case LOCKED:
					x->level = 1;
					break;
				case INCREMENTAL:
					x->level += x->translev;
					break;
				case ZALLOCATED:
					x->zalloc = TRUE;
					break;
				default:
					GTMASSERT;
			}
			x->granted = TRUE;
		} else
		{
			if (x->granted)
			{
				x->zalloc = FALSE;
				x->level = 0;
				x->old = FALSE;
				x->granted = FALSE;
			}
			for (y = mlk_cm_root, locks_bckout = 0; locks_bckout < locks_done; y = y->next, locks_bckout++)
			{
				assert(y->granted && y != x);
				mlk_bckout(y, action);
			}
			if (!x->nodptr)
			{
				start_timer((TID)curr_entry, CM_LKSTARVE_TIME, gtcml_lkstarve, SIZEOF(curr_entry),
						(char *)&curr_entry);
				return STARVED;
			}
			/* insert in blocked structure */
			gtcml_blklck(reg, x, wakeup);
			return BLOCKED;
		}
	}
	return GRANTED;
}
