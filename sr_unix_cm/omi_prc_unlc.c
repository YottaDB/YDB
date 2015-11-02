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
 *  omi_prc_unlc.c ---
 *
 *	Process an UNLOCKCLIENT request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_string.h"

#include "omi.h"
#include "error.h"
#include "mlkdef.h"
#include "mlk_pvtblk_delete.h"
#include "mlk_unlock.h"

int
omi_prc_unlc(omi_conn *cptr, char *xend, char *buff, char *bend)
{
	GBLREF int		omi_pid;
	GBLREF mlk_pvtblk	*mlk_pvt_root;

	int			rv;
	omi_si		  	si;
	char		 	*jid;
	mlk_pvtblk		**prior;

	/*  Client's $JOB */
	OMI_SI_READ(&si, cptr->xptr);
	jid         = cptr->xptr;
	cptr->xptr += si.value;

	/*  Bounds checking */
	if (cptr->xptr > xend)
		return -OMI_ER_PR_INVMSGFMT;

	/*  Condition handler for DBMS operations */
	ESTABLISH_RET(omi_dbms_ch,0);

	/*  Loop through all the locks, unlocking ones that belong to this client */
	for (prior = &mlk_pvt_root; *prior; ) {
		if (!(*prior)->granted || !(*prior)->nodptr
		    || (*prior)->nodptr->owner != omi_pid)
			mlk_pvtblk_delete(prior);
		else if ((*prior)->nodptr->auxowner == (UINTPTR_T)cptr
			 && si.value == (*prior)->value[(*prior)->total_length]
			 && memcmp(&(*prior)->value[(*prior)->total_length+1],
				   jid, si.value) == 0) {
			mlk_unlock(*prior);
			mlk_pvtblk_delete(prior);
		} else
			prior = &(*prior)->next;
	}

	REVERT;

	return 0;
}
