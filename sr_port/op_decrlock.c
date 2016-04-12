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
#include "iotimer.h"
#include "locklits.h"
#include "tp_frame.h"
#include "op.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_unlock.h"
#include "gvcmx.h"
#include "lckclr.h"

GBLREF boolean_t	gtcm_connection;
GBLREF unsigned short	lks_this_cmd;
GBLREF mlk_pvtblk	*mlk_pvt_root;
GBLREF bool		remlkreq;
GBLREF unsigned char	cm_action;
GBLREF tp_frame		*tp_pointer;

int	op_decrlock(int timeout)
{
	int		count;
	mlk_pvtblk	**prior;
	void		lckclr(void);
	error_def(ERR_TPLOCK);

	lckclr();
	if (tp_pointer)
	{
		prior = &mlk_pvt_root;
		for (count = 0; count < lks_this_cmd; count++)
		{
			/* if there were any old locks before TSTART, they can't be  unlocked */
			if ((*prior)->granted && (*prior)->tp &&
				(*prior)->tp->level > (*prior)->level - (*prior)->translev)
				rts_error(VARLSTCNT(1) ERR_TPLOCK);
			prior = &((*prior)->next);
		}
	}

	prior = &mlk_pvt_root;
	for (count = 0; count < lks_this_cmd; count++)
	{
		if (prior)
		{
			if (!(*prior)->granted)
			{	/* if entry was never granted, delete list entry */
				mlk_pvtblk_delete(prior);
			}
			else
			{
				(*prior)->level -= (*prior)->translev > (*prior)->level ? (*prior)->level : (*prior)->translev;

				if (!(*prior)->zalloc && (0 == (*prior)->level))
				{
					mlk_unlock(*prior);
					mlk_pvtblk_delete(prior);
				}
				else
					prior = &((*prior)->next);
			}
		}
	}

	if (gtcm_connection && remlkreq)
	{
		cm_action = INCREMENTAL;
		gvcmx_unlock(0, TRUE, INCREMENTAL);
		remlkreq = FALSE;
	}
	return TRUE;	/* return TRUE unconditionally (since this is a timed unlock) so $TEST gets set to 1 as per M-standard */
}
