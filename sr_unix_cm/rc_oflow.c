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
 *  rc_oflow.c ---
 *
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "gtcm.h"
#include "rc.h"
#include "rc_oflow.h"


rc_oflow *
rc_oflow_alc()
{
    rc_oflow	*rv;

    rv = (rc_oflow *)malloc(sizeof(rc_oflow));
    rv->page   = 0;
    rv->buff   = (char *)0;
    rv->top    = rv->size   = rv->dsid   = rv->offset = rv->zcode  = 0;

    return rv;

}


void
rc_oflow_fin(fop)
    rc_oflow *fop;
{

    if (!fop)
	return;

    if (fop->buff)
	free(fop->buff);

    free(fop);

    return;

}
