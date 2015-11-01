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
 *  omi_prc_disc.c ---
 *
 *	Process a disconnection request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "omi.h"


int
omi_prc_disc(cptr, xend, buff, bend)
    omi_conn	*cptr;
    char	*xend;
    char	*buff;
    char	*bend;
{
    omi_li	 li;

/*  Reason string */
    OMI_LI_READ(&li, cptr->xptr);
/*  Log the string? */
    cptr->xptr += li.value;
/*  Bounds checking */
    if (cptr->xptr > xend)
	return -OMI_ER_PR_INVMSGFMT;

/*  Set the connection state */
    cptr->state = OMI_ST_DISC;

/*  The response contains only a header */
    return 0;

}
