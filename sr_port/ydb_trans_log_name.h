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

#ifndef __YDB_TRANS_LOG_NAME_H__
#define __YDB_TRANS_LOG_NAME_H__

#include "trans_log_name.h"
#include "ydb_logicals.h"	/* for "ydbenvindx_t" */

int4 ydb_trans_log_name(ydbenvindx_t envindx, mstr *trans, char *buffer, int4 buffer_len, boolean_t ignore_errors,
											boolean_t *is_ydb_env_match);

#endif
