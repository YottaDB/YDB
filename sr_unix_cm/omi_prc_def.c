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
 *  omi_prc_def.c ---
 *
 *	Process a DEFINE request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_string.h"

#include "omi.h"
#include "error.h"
#include "stringpool.h"
#include "op.h"


int
omi_prc_def(omi_conn *cptr, char *xend, char *buff, char *bend)
{
    GBLREF bool	 undef_inhibit;

    char	*bptr;
    omi_li	 len;
    int		 rv;
    mval	 vo, vd, vg;

    bptr = buff;

/*  Global Ref */
    OMI_LI_READ(&len, cptr->xptr);
/*  Set up a condition handler */
    ESTABLISH_RET(omi_dbms_ch,0);
    rv = omi_gvextnam(cptr, len.value, cptr->xptr);
/*  If true, there was an error finding the global reference in the DBMS */
    if (rv < 0)
    {
	REVERT;
	return rv;
    }
    cptr->xptr += len.value;

/*  Bounds checking */
    if (cptr->xptr > xend)
    {
	REVERT;
	return -OMI_ER_PR_INVMSGFMT;
    }

/*  We want to make sure there is plenty of space in the string pool
 *  for all three operations ($ORDER, $GET, $DATA) */
    if (cptr->exts & OMI_XTF_NEWOP)
	INVOKE_STP_GCOL(0);

/*  $DATA */
    op_gvdata(&vd);
    if (!(vd.mvtype & MV_INT))
    {
	REVERT;
	return -OMI_ER_DB_UNRECOVER;
    }

    if (cptr->exts & OMI_XTF_NEWOP)
    {
/*	$GET */
	undef_inhibit = TRUE;
	rv = op_gvget(&vg);
/*	$ORDER */
	op_gvorder(&vo);
	OMI_SI_WRIT(vo.str.len, bptr);
	if (vo.str.len)
	{
	    memcpy(bptr, vo.str.addr, vo.str.len);
	    bptr += vo.str.len;
	}
    }

/*  $DATA (buffer write) */
    OMI_SI_WRIT(vd.m[1] / MV_BIAS, bptr);

    if (cptr->exts & OMI_XTF_NEWOP)
    {
/*	$GET (buffer write) */
	OMI_SI_WRIT((rv ? 1 : 0), bptr);
	if (!rv || !vg.str.len)
	    OMI_LI_WRIT(0, bptr);
	else
	{
	    OMI_LI_WRIT(vg.str.len, bptr);
	    memcpy(bptr, vg.str.addr, vg.str.len);
	    bptr += vg.str.len;
	}
    }

    REVERT;

    return (int)(bptr - buff);

}
