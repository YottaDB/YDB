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

/*
 *  omi_prc_unla.c ---
 *
 *	Process an UNLOCKALL (Agent) request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "omi.h"
#include "error.h"
#include "mlkdef.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_unlock.h"


int
omi_prc_unla(omi_conn *cptr, char *xend, char *buff, char *bend)
{
	GBLREF int		  omi_pid;
	GBLREF mlk_pvtblk	 *mlk_pvt_root;

	mlk_pvtblk		**prior;


	/*  Condition handler for DBMS operations */
	ESTABLISH_RET(omi_dbms_ch,0);

	/*  Loop through all the locks, unlocking ones that belong to this agent */
	for (prior = &mlk_pvt_root; *prior; )
	{
		if (!(*prior)->granted || !(*prior)->nodptr || (*prior)->nodptr->owner != omi_pid)
			mlk_pvtblk_delete(prior);
		else if ((*prior)->nodptr->auxowner == (UINTPTR_T)cptr)
		{
			mlk_unlock(*prior);
			mlk_pvtblk_delete(prior);
		} else
			prior = &(*prior)->next;
	}

	REVERT;

	/*  The response contains only a header */
	return 0;
}
