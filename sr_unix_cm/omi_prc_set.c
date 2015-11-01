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
 *  omi_prc_set.c ---
 *
 *	Process a SET request.
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
omi_prc_set(omi_conn *cptr, char *xend, char *buff, char *bend)
{
    int		 rv;
    omi_si	 replicate;
    omi_li	 li;
    mval	 v;

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

/*  Global Data */
    OMI_LI_READ(&li, cptr->xptr);
    v.mvtype    = MV_STR;
    v.str.len   = li.value;
    v.str.addr  = cptr->xptr;

    op_gvput(&v);

    REVERT;

    cptr->xptr += li.value;
/*  Bounds checking */
    if (cptr->xptr > xend)
	return -OMI_ER_PR_INVMSGFMT;

/*  The response contains only a header */
    return 0;

}
