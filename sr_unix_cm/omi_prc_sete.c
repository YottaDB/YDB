/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  omi_prc_sete.c ---
 *
 *	Process a SET EXTRACT request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "omi.h"
#include "error.h"


int
omi_prc_sete(cptr, xend, buff, bend)
    omi_conn	*cptr;
    char	*xend;
    char	*buff;
    char	*bend;
{
    int		 rv;
    omi_si	 replicate;
    omi_li	 li;
    char	*gdptr;
    omi_li	 start;
    omi_li	 end;

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

/*  Global Data */
    OMI_LI_READ(&li, cptr->xptr);
    gdptr       = cptr->xptr;
    cptr->xptr += li.value;

/*  Start position */
    OMI_LI_READ(&start, cptr->xptr);
/*  End position */
    OMI_LI_READ(&end, cptr->xptr);

/*  Bounds checking */
    if (cptr->xptr > xend) {
	REVERT;
	return -OMI_ER_PR_INVMSGFMT;
    }

/*  XXX $EXTRACT */

    REVERT;

/*  The response contains only a header */
    return 0;

}
