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
#include "rc.h"
#include "error.h"
#include "util.h"


CONDITION_HANDLER(rc_dbms_ch)
{
    GBLREF int4	 rc_errno;
    int		dummy1, dummy2;

    START_CH;

    if (SEVERITY == SUCCESS || SEVERITY == INFO) {
	CONTINUE;
    }

    if (SEVERITY == WARNING || SEVERITY == ERROR) {
	rc_errno = RC_GLOBERRUNSPEC;
	util_out_print(0,1,0);		/* flush output */
	UNWIND(dummy1, dummy2);
    }

    NEXTCH;

}
