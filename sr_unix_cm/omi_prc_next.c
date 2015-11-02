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
 *  omi_prc_next.c ---
 *
 *	Process a NEXT request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_string.h"

#include "omi.h"
#include "error.h"
#include "op.h"


int
omi_prc_next(omi_conn *cptr, char *xend, char *buff, char *bend)
{
    char	*bptr;
    int		 rv;
    omi_li	 len;
    mval	 v;

    bptr = buff;

/*  Global Ref */
    OMI_LI_READ(&len, cptr->xptr);
/*  Condition handler for DBMS operations */
    ESTABLISH_RET(omi_dbms_ch,0);
    rv = omi_gvextnam(cptr, len.value, cptr->xptr);
/*  If true, there was an error finding the global reference in the DBMS */
    if (rv < 0) {
	REVERT;
	return rv;
    }
    cptr->xptr += len.value;

/*  Bounds checking */
    if (cptr->xptr > xend) {
	REVERT;
	return -OMI_ER_PR_INVMSGFMT;
    }

    op_gvnext(&v);

    REVERT;

    if (!(v.str.len <= 255))
	return -OMI_ER_DB_UNRECOVER;
    OMI_SI_WRIT(v.str.len, bptr);
    if (v.str.len) {
	memcpy(bptr, v.str.addr, v.str.len);
	bptr += v.str.len;
    }

    return (int)(bptr - buff);

}
