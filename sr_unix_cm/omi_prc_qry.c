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
 *  omi_prc_qry.c ---
 *
 *	Process a QUERY request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_string.h"

#include "omi.h"
#include "error.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "op.h"
#include "gvsub2str.h"

GBLREF gv_key		*gv_altkey;

int
omi_prc_qry(omi_conn *cptr, char *xend, char *buff, char *bend)
{
    char	*bptr, *eptr;
    int		 rv;
    omi_li	 len;
    mval	 v;
    uns_char	*bgn1, *bgn2, *sbsp;
    char	*grp;
    int		 grl;

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
    eptr        = cptr->xptr;
    cptr->xptr += len.value;

/*  Bounds checking */
    if (cptr->xptr > xend) {
	REVERT;
	return -OMI_ER_PR_INVMSGFMT;
    }

    op_gvquery(&v);

    REVERT;

    if (v.str.len == 0) {
	OMI_LI_WRIT(0, bptr);
	return (int)(bptr - buff);
    }

/*  Put a global reference into the reply */
    bgn1    = (uns_char *)bptr;
    bptr   += OMI_LI_SIZ;

/*  Environment (we give back the request environment) */
    OMI_LI_READ(&len, eptr);
    OMI_LI_WRIT(len.value, bptr);
    (void) memcpy(bptr, eptr, len.value);
    bptr   += len.value;

/*  Global name */
    bgn2    = (uns_char *)bptr++;
    *bptr++ = '^';
    grp     = (char *)gv_altkey->base;
    grl     = STRLEN(grp);
    OMI_SI_WRIT(grl + 1, bgn2);
    (void) strcpy(bptr, grp);
    bptr   += grl;

/*  Subscripts */
    for (grp += grl + 1; *grp; grp += strlen(grp) + 1) {
	bgn2   = (uns_char *)bptr++;
	sbsp   = gvsub2str((uchar_ptr_t)grp, (uchar_ptr_t)bptr, FALSE);
	grl    = (int)(sbsp - (uns_char *)bptr);
	OMI_SI_WRIT(grl, bgn2);
	bptr  += grl;
	sbsp  += grl + 1;
    }

/*  Length of the global reference */
    grl = (int)((uns_char *)bptr - bgn1);
    OMI_LI_WRIT(grl - OMI_LI_SIZ, bgn1);

    return (int)(bptr - buff);

}
