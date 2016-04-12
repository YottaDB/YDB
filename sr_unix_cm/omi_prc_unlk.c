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

/*
 *  omi_prc_unlk.c ---
 *
 *	Process an UNLOCK request.
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


int omi_prc_unlk(omi_conn *cptr, char *xend, char *buff, char *bend)
{
	GBLREF int		 omi_pid;
	GBLREF mlk_pvtblk	*mlk_pvt_root;

	omi_li		 li;
	int			 rv;
	omi_si		 si;
	mlk_pvtblk		*next, **prior;

	/*  Global Ref */
	OMI_LI_READ(&li, cptr->xptr);
	/*  Condition handler for DBMS operations */
	ESTABLISH_RET(omi_dbms_ch,0);

	/*  Clean up dead locks in private list */
	for (prior = &mlk_pvt_root; *prior; )
	{
		if (!(*prior)->granted || !(*prior)->nodptr || (*prior)->nodptr->owner != omi_pid)
			mlk_pvtblk_delete(prior);
		else
		{
			(*prior)->trans = 0;
			prior = &(*prior)->next;
		}
	}

	rv = omi_lkextnam(cptr, li.value, cptr->xptr, cptr->xptr + li.value);
	/*  If true, there was an error finding the global reference in the DBMS */
	if (rv <= 0) {
		REVERT;
		return rv;
	}
	cptr->xptr += li.value;

	/*  Client's $JOB */
	OMI_SI_READ(&si, cptr->xptr);
	cptr->xptr += si.value;

	/*  Bounds checking */
	if (cptr->xptr > xend) {
		REVERT;
		return -OMI_ER_PR_INVMSGFMT;
	}

	mlk_pvt_root->trans = 0;
	if (mlk_pvt_root->nodptr && mlk_pvt_root->nodptr->owner == omi_pid)
	{
		assert(mlk_pvt_root->nodptr->auxowner == (UINTPTR_T)cptr);
		if (--mlk_pvt_root->level == 0)
			mlk_unlock(mlk_pvt_root);
	} else
		mlk_pvt_root->level = 0;

	/*  Clean up after the (now dead) lock */
	if (!mlk_pvt_root->level)
	{
		next = mlk_pvt_root->next;
		free(mlk_pvt_root);
		mlk_pvt_root = next;
	}

	REVERT;
	return 0;
}
