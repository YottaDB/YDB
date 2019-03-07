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

#ifndef YDB_GETENV_H_INCLUDED
#define YDB_GETENV_H_INCLUDED

#include "ydb_logicals.h"	/* for "ydbenvindx_t" */

#define	NULL_SUFFIX		NULL
#define	NULL_IS_YDB_ENV_MATCH	NULL

char *ydb_getenv(ydbenvindx_t envindx, mstr *suffix, boolean_t *is_ydb_env_match);

#endif
