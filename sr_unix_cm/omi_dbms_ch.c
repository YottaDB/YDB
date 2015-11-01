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
 *  omi_dbms_ch.c ---
 *
 *	Condition handler for DBMS interaction.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "omi.h"
#include "error.h"
#include "util.h"
#include "gtcm.h"

CONDITION_HANDLER(omi_dbms_ch)
{
    GBLREF int4	 omi_errno;
    int		dummy1, dummy2;

    START_CH;

    if (SEVERITY == SUCCESS || SEVERITY == INFO) {
	CONTINUE;
    }

    PRN_ERROR;
    gtcm_rep_err("",SIGNAL);

    if (SEVERITY == WARNING || SEVERITY == ERROR) {
	omi_errno = OMI_ER_DB_UNRECOVER;
	UNWIND(dmmy1, dummy2);
    }

    NEXTCH;

}
