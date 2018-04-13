/****************************************************************
 *								*
 * Copyright 2002, 2009 Fidelity Information Services, Inc	*
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

#include "gtm_string.h"

#include "startup.h"
#include "dollar_system_init.h"
#include "io.h"
#include "iosp.h"
#include "stringpool.h"
#include "ydb_trans_log_name.h"

GBLREF	mval	dollar_system;
GBLREF spdesc	stringpool;

void dollar_system_init(struct startup_vector *svec)
{
	int4		status;
	mstr		tn;
	char		buf[MAX_TRANS_NAME_LEN];

	error_def(ERR_LOGTOOLONG);
	error_def(ERR_TRNLOGFAIL);

	dollar_system.mvtype = MV_STR;
	dollar_system.str.addr = (char *)stringpool.free;
	dollar_system.str.len = STR_LIT_LEN("47,");
	memcpy(stringpool.free, "47,", dollar_system.str.len);
	stringpool.free += dollar_system.str.len;
	if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_SYSID, &tn, buf, SIZEOF(buf), IGNORE_ERRORS_FALSE, NULL)))
	{
		dollar_system.str.len += tn.len;
		memcpy(stringpool.free, tn.addr, tn.len);
		stringpool.free += tn.len;
	} else
	{
		assert(SS_NOLOGNAM == status);
		dollar_system.str.len += svec->sysid_ptr->len;
		memcpy(stringpool.free, svec->sysid_ptr->addr, svec->sysid_ptr->len);
                stringpool.free += svec->sysid_ptr->len ;
	}
	assert(stringpool.free < stringpool.top);	/* it's process initialization after all */
	return;
}
