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
 *  omi_prc_incr.c ---
 *
 *	Process a INCREMENT request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "omi.h"
#include "error.h"


int
omi_prc_incr(cptr, xend, buff, bend)
    omi_conn	*cptr;
    char	*xend;
    char	*buff;
    char	*bend;
{
    char	*bptr;
    int		 rv;
    omi_si	 replicate;
    omi_li	 li;
    omi_si	 si;
    char	*ivptr;

    bptr = buff;

/*  Replicate flag */
    OMI_SI_READ(&replicate, cptr->xptr);

/*  Global Ref */
    OMI_LI_READ(&li, cptr->xptr);
/*  Condition handler for DBMS operations */
    ESTABLISH_RET(omi_dbms_ch, -1);	/* any return value to signify error return */
    rv = omi_gvextnam(cptr, li.value, cptr->xptr);
/*  If true, there was an error finding the global reference in the DBMS */
    if (rv < 0) {
	REVERT;
	return rv;
    }
    cptr->xptr += li.value;

/*  Increment value */
    OMI_SI_READ(&si, cptr->xptr);
    ivptr       = cptr->xptr;
    cptr->xptr += si.value;

/*  Bounds checking */
    if (cptr->xptr > xend || bptr >= bend) {
	REVERT;
	return -OMI_ER_PR_INVMSGFMT;
    }

/*  XXX INCR */

    REVERT;

    return (int)(bptr - buff);
}
