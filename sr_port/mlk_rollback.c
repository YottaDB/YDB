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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "mlkdef.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_rollback.h"
#include "mlk_unlock.h"

GBLREF 	mlk_pvtblk	*mlk_pvt_root;

/* rollback mlocks to a given newlevel in tp */

void mlk_rollback(short newlevel)
{
	mlk_pvtblk	**prior;
	mlk_tp		*oldlock, *next_oldlock;
	for (prior = &mlk_pvt_root;  *prior;)
	{
		if (!(*prior)->granted)
			mlk_pvtblk_delete(prior);
		else  if ((*prior)->tp)				/* if this was a pre-existing lock */
		{
			for(oldlock = (*prior)->tp; oldlock && newlevel < oldlock->tplevel; oldlock = next_oldlock)
			{
				next_oldlock = oldlock->next;
				free(oldlock);
			}
			if (!oldlock)
			{
				mlk_unlock(*prior);
				mlk_pvtblk_delete(prior);
			} else
			{
				(*prior)->level = oldlock->level;
				(*prior)->zalloc = oldlock->zalloc;
				prior = &((*prior)->next);
			}
		} else
		{
			mlk_unlock(*prior);
			mlk_pvtblk_delete(prior);
		}
	}
}
