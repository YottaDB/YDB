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

#include "mlkdef.h"
#include "iotimer.h"
#include "locklits.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "tp_frame.h"
#include "op.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_unlock.h"
#include "gvcmx.h"
#include "lckclr.h"

GBLREF boolean_t	gtcm_connection;
GBLREF bool		remlkreq;
GBLREF unsigned short	lks_this_cmd;
GBLREF mlk_pvtblk	*mlk_pvt_root;
GBLREF unsigned char	cm_action;
GBLREF tp_frame		*tp_pointer;

error_def(ERR_TPLOCK);

/*
 * -----------------------------------------------
 * Arguments:
 *	timeout	- s.b. negative, indicating no timeout
 *      auxown - auxillary owner field for use by servers
 * -----------------------------------------------
 */

void op_zdealloc2(mval *timeout, UINTPTR_T auxown)
{
	unsigned short	count;
	mlk_pvtblk	**prior;
	bool		specific;

	assert(NO_M_TIMEOUT == timeout->m[1]);
	if (lks_this_cmd)
	{
		specific = TRUE;
		lckclr();
		if (tp_pointer)
		{
			prior = &mlk_pvt_root;
			for (count = 0;  count < lks_this_cmd;  count++)
			{
				/* if there were any old locks before TSTART, they can't be  unlocked */
				if ((*prior)->granted && (*prior)->zalloc && (*prior)->tp && (*prior)->tp->zalloc)
				{
					lks_this_cmd = 0;
					rts_error_csa(NULL, VARLSTCNT(1) ERR_TPLOCK);
				}
				prior = &((*prior)->next);
			}
		}
		prior = &mlk_pvt_root;
		for (count = 0;  count < lks_this_cmd;  count++)
		{
			if (!(*prior)->granted)
			{	/* if entry was never granted, delete list entry */
				mlk_pvtblk_delete(prior);
			} else if ((*prior)->nodptr && (*prior)->nodptr->auxowner != auxown)
			{	/*if its not for this sub-entity skip it*/
				prior = &((*prior)->next);
			}
			else  if (!(*prior)->level)
			{	/*if no lock levels, its history*/
				mlk_unlock(*prior);
				mlk_pvtblk_delete(prior);
			} else
			{	/*just drop the zalloc and move on*/
				(*prior)->zalloc = FALSE;
				prior = &((*prior)->next);
			}
		}
	} else
	{
		/* if there were any old locks before TSTART, they can't be  unlocked */
		if (mlk_pvt_root && tp_pointer && tp_pointer->old_locks)
		{
			lks_this_cmd = 0;
			rts_error_csa(NULL, VARLSTCNT(1) ERR_TPLOCK);
		}
		specific = FALSE;
		for (prior = &mlk_pvt_root;  *prior;)
		{
			if (!(*prior)->granted)
			{	/* if entry was never granted, delete list entry */
				mlk_pvtblk_delete(prior);
			}
			else  if ((*prior)->nodptr && (*prior)->nodptr->auxowner != auxown)
			{	/*if its not for this sub-entity skip it*/
				prior = &((*prior)->next);
			} else  if (!(*prior)->level)
			{	/*if no lock levels, its history*/
				mlk_unlock(*prior);
				mlk_pvtblk_delete(prior);
			} else
			{	/*just drop the zalloc and move on*/
				(*prior)->zalloc = FALSE;
				prior = &((*prior)->next);
			}
		}
	}
	lks_this_cmd = 0;	/* reset so we can check whether an extrinsic is trying to nest a LOCK operation */
	if (gtcm_connection)
	{
		cm_action = CM_ZALLOCATES;
		gvcmx_unlock(cm_action, specific, FALSE);
		remlkreq = FALSE;
	}
	return;
}
