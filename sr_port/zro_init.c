/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "mdef.h"

#include "gtm_string.h"

#include "iosp.h"
#include "io.h"
#include "zroutines.h"
#include "ydb_trans_log_name.h"

error_def(ERR_LOGTOOLONG);

#define MAX_NUMBER_FILENAMES	(256 * MAX_TRANS_NAME_LEN)

void zro_init(void)
{
	int4		status;
	mstr		val, tn;
	char		buf1[MAX_NUMBER_FILENAMES]; /* buffer to hold translated name */
	boolean_t	is_ydb_env_match;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TREF(dollar_zroutines)).addr)
		free((TREF(dollar_zroutines)).addr);
	status = ydb_trans_log_name(YDBENVINDX_ROUTINES, &tn, buf1, SIZEOF(buf1), IGNORE_ERRORS_FALSE, NULL);
	assert((SS_NORMAL == status) || (SS_NOLOGNAM == status));
	if ((0 == tn.len) || (SS_NOLOGNAM == status))
	{
		tn.len = 1;
		tn.addr = buf1;
		buf1[0] = '.';
	}
	(TREF(dollar_zroutines)).len = tn.len;
	(TREF(dollar_zroutines)).addr = (char *)malloc (tn.len);
	memcpy ((TREF(dollar_zroutines)).addr, buf1, tn.len);
	zro_load(TADR(dollar_zroutines));
}
