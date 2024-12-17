/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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

#include <stdarg.h>
#include "gtm_putmsg_list.h"
#include "util.h"
#include "util_out_print_vaparm.h"
#include "gtmmsg.h"

void dec_err(uint4 argcnt, ...)
{
	va_list		var;

	util_out_print(NULL, RESET, NULL);	    /* reset the buffer */
	VAR_START(var, argcnt);
	gtm_putmsg_list(NULL, argcnt, var);
	util_out_print(NULL, FLUSH);

}
