/****************************************************************
 *								*
 * Copyright 2004 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRANS_NUMERIC_H_INCLUDED
#define TRANS_NUMERIC_H_INCLUDED

#include "ydb_logicals.h"	/* for "ydbenvindx_t" */

uint4 ydb_trans_numeric(ydbenvindx_t envindx, boolean_t *is_defined, boolean_t ignore_errors, boolean_t *is_ydb_env_match);
gtm_uint8 ydb_trans_numeric_64(ydbenvindx_t envindx, boolean_t *is_defined, boolean_t ignore_errors, boolean_t *is_ydb_env_match);

#endif
