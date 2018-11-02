/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "libyottadb.h"
#include "callg.h"

/* Routine to drive a variadic plist function with the given plist. It uses the "no-count" version of
 * callg() to supress the first parameter being an overall count of the arguments as sometimes, that's
 * not what is needed.
 */
int ydb_call_variadic_plist_func_s(ydb_vplist_func cgfunc, uintptr_t cvplist)
{
	return (int)callg_nc((callgncfnptr)cgfunc, (gparam_list *)cvplist);
}
