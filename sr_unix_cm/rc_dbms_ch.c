/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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

	ASSERT_IS_LIBGTCM;
    START_CH(TRUE);

    if (SEVERITY == WARNING || SEVERITY == ERROR) {
	rc_errno = RC_GLOBERRUNSPEC;
	util_out_print(0,1,0);		/* flush output */
	UNWIND(dummy1, dummy2);
    }

    NEXTCH;
}
