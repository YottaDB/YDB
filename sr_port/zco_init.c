/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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
#include "ydb_trans_log_name.h"
#include "zco_init.h"

error_def(ERR_LOGTOOLONG);

void zco_init(void)
{
	int4	status;
	mstr	tn;
	char	buf1[MAX_TRANS_NAME_LEN]; /* buffer to hold translated name */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TREF(dollar_zcompile)).addr)
		free((TREF(dollar_zcompile)).addr);
	(TREF(dollar_zcompile)).len = 0; /* default */
	status = ydb_trans_log_name(YDBENVINDX_COMPILE, &tn, buf1, SIZEOF(buf1), IGNORE_ERRORS_FALSE, NULL);
	if (SS_NORMAL != status)
	{
<<<<<<< HEAD
		assert(SS_NOLOGNAM == status);
		return;
=======
		if (SS_NOLOGNAM == status)
			return;
		else if (SS_LOG2LONG == status)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.len, val.addr, SIZEOF(buf1) - 1);
		else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) status);
>>>>>>> 451ab477 (GT.M V7.0-000)
	}
	if (0 == tn.len)
		return;
	(TREF(dollar_zcompile)).len = tn.len;
	(TREF(dollar_zcompile)).addr = (char *) malloc (tn.len);
	memcpy ((TREF(dollar_zcompile)).addr, buf1, tn.len);
}
