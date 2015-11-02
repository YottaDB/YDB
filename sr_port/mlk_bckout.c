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
#include "mlk_bckout.h"
#include "mlk_unlock.h"
#include "mlk_unpend.h"

void mlk_bckout(mlk_pvtblk *p,unsigned char action)
{
	if (!p->granted)
	{
		mlk_unpend(p);
	}
	else
	{
		switch(action)
		{
			case INCREMENTAL:
				if ((p->level -= p->translev) <= 0 && !p->zalloc)
				{	mlk_unlock(p);
					p->granted = FALSE;
					p->level = 0;
					p->nodptr = 0;
				}
				break;
			case ZALLOCATED:
				if (!p->level && !(p->old))
				{	mlk_unlock(p);
					p->granted = FALSE;
					p->zalloc = FALSE;
					p->nodptr = 0;
				}
				else if (!p->old)
					p->zalloc = FALSE;
				break;
			case LOCKED:
				if (!p->zalloc)
				{	mlk_unlock(p);
					p->granted = FALSE;
					p->level = 0;
					p->nodptr = 0;
				}
				else
					p->level = 0;
				break;
			default:
				GTMASSERT;
		}
	}
}
