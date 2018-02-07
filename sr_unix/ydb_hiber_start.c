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

/* Simple YottaDB wrapper for gtm_hiber_start() */
void	ydb_hiber_start(unsigned long long ussleep)
{
	gtm_hiber_start((ydb_uint_t)(ussleep / NANOSECS_IN_MSEC));
}
