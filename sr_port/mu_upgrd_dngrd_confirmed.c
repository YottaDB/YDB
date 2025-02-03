/****************************************************************
 *								*
 * Copyright (c) 2005-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef UNIX
#include "gtm_stdio.h"
#else
#include <descrip.h>
#endif
#include "gtm_string.h"
#include "gtm_ctype.h"
#include "gtmmsg.h"
#include "util.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "op.h"

LITREF	mval		literal_notimeout;

#define CONTINUEMSG "Are you ready to continue the operation [y/n] ? "

/* Asks user for confirmation, before doing the operation.
 * Returns: TRUE if confirmed, FALSE if not confirmed */
boolean_t mu_upgrd_dngrd_confirmed(void)
{
	mval		dummy = { 0 }, *input_line;

	util_out_print("!AD", TRUE, LEN_AND_LIT("You must have a backup before you proceed!!"));
	util_out_print("!AD", TRUE, LEN_AND_LIT("An abnormal termination may damage the database files during the operation !!"));
	util_out_print("!_!_!AD", TRUE, LEN_AND_LIT(CONTINUEMSG));
	input_line = push_mval(&dummy);
	op_read(input_line, (mval *)&literal_notimeout);
	if (!input_line->str.len)
		return FALSE;
	return ('y' == input_line->str.addr[0] || 'Y' == input_line->str.addr[0]);
}
