/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* This is like gbldefs.c but contains all global variables that are needed
 * even by "gtmsecshr_wrapper" (which does not pull in gbldefs.c).
 * Not including "mdef.h" in this module since this is also included in the encryption plugin.
 */

#include "gtm_common_defs.h"

#include "ydb_logicals.h"

/* ydbenvname contains the list of env vars supported by YottaDB */
LITDEF	char	*ydbenvname[YDBENVINDX_MAX_INDEX] =
{
#	define YDBENVINDX_TABLE_ENTRY(ydbenvindx, ydbenvname, gtmenvname)	ydbenvname,
#	include "ydb_logicals_tab.h"
#	undef YDBENVINDX_TABLE_ENTRY
};

/* gtmenvname contains the list of env vars supported by GT.M (and in turn by YottaDB) */
LITDEF	char	*gtmenvname[YDBENVINDX_MAX_INDEX] =
{
#	define YDBENVINDX_TABLE_ENTRY(ydbenvindx, ydbenvname, gtmenvname)	gtmenvname,
#	include "ydb_logicals_tab.h"
#	undef YDBENVINDX_TABLE_ENTRY
};

