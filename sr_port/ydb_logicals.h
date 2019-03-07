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

#ifndef YDB_LOGICALS_H_INCLUDED
#define YDB_LOGICALS_H_INCLUDED

#define	IGNORE_ERRORS_FALSE	0
#define	IGNORE_ERRORS_TRUE	1
#define	IGNORE_ERRORS_NOSENDMSG	2	/* like IGNORE_ERRORS_TRUE but additionally, no send_msg is done in "trans_log_name" */

typedef enum
{
#	define YDBENVINDX_TABLE_ENTRY(ydbenvindx, ydbenvname, gtmenvname)	ydbenvindx,
#	include "ydb_logicals_tab.h"
#	undef YDBENVINDX_TABLE_ENTRY
	YDBENVINDX_MAX_INDEX		/* Total number of env vars in ydbenvindx table */
} ydbenvindx_t;

LITREF	char	*ydbenvname[YDBENVINDX_MAX_INDEX];
LITREF	char	*gtmenvname[YDBENVINDX_MAX_INDEX];

#define	YDB_GBLDIR	ydbenvname[YDBENVINDX_GBLDIR]
#define	GTM_GBLDIR	gtmenvname[YDBENVINDX_GBLDIR]

#endif
