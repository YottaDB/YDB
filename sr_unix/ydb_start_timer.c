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

#include "gtmxc_types.h"

/* Simple YottaDB wrapper for gtm_free() */
void	ydb_start_timer(ydb_tid_t tid, unsigned long long time_to_expir, void (*handler)(), ydb_int_t hdata_len, void *hdata)
{
	gtm_start_timer(tid, time_to_expir, handler, hdata_len, hdata);
}
