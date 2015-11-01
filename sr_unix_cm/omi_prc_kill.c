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

/*
 *  omi_prc_kill.c ---
 *
 *	Process a KILL request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "omi.h"
#include "error.h"
#include "op.h"


int
omi_prc_kill(omi_conn *cptr, char *xend, char *buff, char *bend)
{
    int		 rv;
    omi_si	 replicate;
    omi_li	 li;

/*  Replicate flag */
    OMI_SI_READ(&replicate, cptr->xptr);

/*  Global Ref */
    OMI_LI_READ(&li, cptr->xptr);
/*  Condition handler for DBMS operations */
    ESTABLISH_RET(omi_dbms_ch,0);
    rv = omi_gvextnam(cptr, li.value, cptr->xptr);
/*  If true, there was an error finding the global reference in the DBMS */
    if (rv < 0) {
	REVERT;
	return rv;
    }
    cptr->xptr += li.value;

/*  Bounds checking */
    if (cptr->xptr > xend) {
	REVERT;
	return -OMI_ER_PR_INVMSGFMT;
    }

/*  Do the KILL */
    op_gvkill();

    REVERT;

/*  The response contains only a header */
    return 0;

}
