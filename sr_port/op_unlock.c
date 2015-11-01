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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "tp_frame.h"
#include "op.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_unlock.h"
#include "gvcmx.h"

GBLREF boolean_t	gtcm_connection;
GBLREF unsigned char	cm_action;
GBLREF mlk_pvtblk	*mlk_pvt_root;
GBLREF tp_frame		*tp_pointer;

void op_unlock(void)
{
	mlk_pvtblk **prior;
	error_def(ERR_TPLOCK);

	/* if there were any old locks before TSTART, they can't be  unlocked */
	if (mlk_pvt_root && tp_pointer && tp_pointer->old_locks)
		rts_error(VARLSTCNT(1) ERR_TPLOCK);

	/* must deal with cm */
	if (gtcm_connection)
	{
		cm_action = 0;
		gvcmx_unlock(cm_action, FALSE, FALSE);
	}

	for (prior = &mlk_pvt_root ; *prior ; )
	{
		if (!(*prior)->granted)
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
