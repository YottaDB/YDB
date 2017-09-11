/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

GBLREF	boolean_t	gtcm_connection;
GBLREF	int		process_exiting;
GBLREF	mlk_pvtblk	*mlk_pvt_root;
GBLREF	tp_frame	*tp_pointer;
GBLREF	unsigned char	cm_action;
GBLREF	unsigned short	lks_this_cmd;

error_def(ERR_TPLOCK);

void op_unlock(void)
{
	mlk_pvtblk 	**prior;
	boolean_t	is_proc_exiting;

	/* if there were any old locks before TSTART, they can't be  unlocked */
	if (mlk_pvt_root && tp_pointer && tp_pointer->old_locks)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TPLOCK);
	lks_this_cmd = 0;
	op_lkinit();
	/* must deal with cm */
	if (gtcm_connection)
	{
		cm_action = 0;
		gvcmx_unlock(cm_action, FALSE, FALSE);
	}
	is_proc_exiting = process_exiting;	/* copy global variable into local to speed up access in loop below */
	for (prior = &mlk_pvt_root ; *prior ; )
	{
		if (!(*prior)->granted)
			mlk_pvtblk_delete(prior);
		else if ((*prior)->zalloc)
		{
			(*prior)->level = 0;
			prior = &((*prior)->next);
		} else
		{	/* If process is dying, try not to get crit to do the unlock. This speeds up process exit. */
			if (!is_proc_exiting)
				mlk_unlock(*prior);
			else
				mlk_nocrit_unlock(*prior);
			mlk_pvtblk_delete(prior);
		}
	}
	return;
}
