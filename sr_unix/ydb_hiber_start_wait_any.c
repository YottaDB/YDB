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

#include "gt_timer.h"
#include "libyottadb.h"

/* Simple YottaDB wrapper for gtm_hiber_start_wait_any() */
void	ydb_hiber_start_wait_any(unsigned long long ussleep)
{
	hiber_start_wait_any((ydb_uint_t)(ussleep / NANOSECS_IN_MSEC));
}
