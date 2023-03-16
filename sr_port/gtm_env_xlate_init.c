/****************************************************************
 *                                                              *
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_limits.h"

#include "iosp.h"
#include "trans_log_name.h"
#include "error.h"
#include "gtm_env_xlate_init.h"
#include "stringpool.h"
#include "ydb_trans_log_name.h"

GBLREF	mstr	env_ydb_gbldir_xlate;
GBLREF mstr	env_gtm_env_xlate;

error_def(ERR_LOGTOOLONG);
error_def(ERR_TRNLOGFAIL);

void gtm_env_xlate_init(void)
{
	int4		status;
	mstr		tn;
	char		buf[YDB_PATH_MAX];

	env_gtm_env_xlate.len = 0; /* default */
	if (SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_ENV_TRANSLATE, &tn, buf, SIZEOF(buf), IGNORE_ERRORS_FALSE, NULL)))
	{
		assert(SS_NOLOGNAM == status);
		return;
	}
	if (0 == tn.len)
		return;
	env_gtm_env_xlate.len = tn.len;
	env_gtm_env_xlate.addr = (char *)malloc(tn.len);
	memcpy(env_gtm_env_xlate.addr, buf, tn.len);
	return;
}

void ydb_gbldir_xlate_init(void)
{
	int4		status;
	mstr		tn;
	char		buf[YDB_PATH_MAX];

	env_ydb_gbldir_xlate.len = 0; /* default */
	if (SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_GBLDIR_TRANSLATE, &tn, buf, SIZEOF(buf), IGNORE_ERRORS_FALSE, NULL)))
	{
		assert(SS_NOLOGNAM == status);
		return;
	}
	if (0 == tn.len)
		return;
	env_ydb_gbldir_xlate.len = tn.len;
	env_ydb_gbldir_xlate.addr = (char *)malloc(tn.len);
	memcpy(env_ydb_gbldir_xlate.addr, buf, tn.len);
	return;
}
