/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "setup_error.h"
#include "gtm_putmsg_list.h"

/* Routine to setup an error in util_outbuff as if rts_error had put it there. Used when we morph ERR_TPRETRY
 * to ERR_TPRESTNESTERR. Requires a va_list var containing the args so do this in this separate routine.
 */
void setup_error(sgmnt_addrs *csa, int argcnt, ...)
{
	va_list		var;

	VAR_START(var, argcnt);
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
}

