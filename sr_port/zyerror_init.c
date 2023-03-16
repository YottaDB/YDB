/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "io.h"
#include "iosp.h"
#include "zyerror_init.h"
#include "ydb_trans_log_name.h"
#include "stringpool.h"

GBLREF	mval	dollar_zyerror;

error_def(ERR_LOGTOOLONG);
error_def(ERR_TRNLOGFAIL);

void zyerror_init(void)
{
	int4		status;
	mstr		tn;
	char		buf[1024];

	dollar_zyerror.str.len = 0; /* default */
	if (SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_ZYERROR, &tn, buf, SIZEOF(buf), IGNORE_ERRORS_FALSE, NULL)))
	{
		assert(SS_NOLOGNAM == status);
		return;
	}
	if (0 == tn.len)
		return;
	dollar_zyerror.mvtype = MV_STR;
	dollar_zyerror.str.len = tn.len;
	dollar_zyerror.str.addr = buf;
	s2pool(&dollar_zyerror.str);
	return;
}
