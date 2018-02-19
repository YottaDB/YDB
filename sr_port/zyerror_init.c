/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_logicals.h"
#include "zyerror_init.h"
#include "trans_log_name.h"
#include "stringpool.h"

GBLREF	mval	dollar_zyerror;

error_def(ERR_LOGTOOLONG);
error_def(ERR_TRNLOGFAIL);

void zyerror_init(void)
{
	int4		status;
	mstr		val, tn;
	char		buf[1024];

	val.addr = ZYERROR;
	val.len = SIZEOF(ZYERROR) - 1;
	dollar_zyerror.str.len = 0; /* default */
	if (SS_NORMAL != (status = TRANS_LOG_NAME(&val, &tn, buf, SIZEOF(buf), dont_sendmsg_on_log2long)))
	{
		if (SS_NOLOGNAM == status)
			return;
		else if (SS_LOG2LONG == status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.len, val.addr, SIZEOF(buf) - 1);
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TRNLOGFAIL, 2, LEN_AND_LIT(ZYERROR), status);
	}
	if (0 == tn.len)
		return;
	dollar_zyerror.mvtype = MV_STR;
	dollar_zyerror.str.len = tn.len;
	dollar_zyerror.str.addr = buf;
	s2pool(&dollar_zyerror.str);
	return;
}
