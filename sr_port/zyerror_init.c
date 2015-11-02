/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

void zyerror_init(void)
{
	int4		status;
	mstr		val, tn;
	char		buf[1024];

	error_def(ERR_LOGTOOLONG);
	error_def(ERR_TRNLOGFAIL);

	val.addr = ZYERROR;
	val.len = SIZEOF(ZYERROR) - 1;
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &tn, buf, SIZEOF(buf), dont_sendmsg_on_log2long)))
	{
		dollar_zyerror.mvtype = MV_STR;
		dollar_zyerror.str.len = tn.len;
		dollar_zyerror.str.addr = buf;
		s2pool(&dollar_zyerror.str);
	} else if (SS_NOLOGNAM == status)
		dollar_zyerror.str.len = 0;
#	ifdef UNIX
	else if (SS_LOG2LONG == status)
		rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.len, val.addr, SIZEOF(buf) - 1);
#	endif
	else
		rts_error(VARLSTCNT(5) ERR_TRNLOGFAIL, 2, LEN_AND_LIT(ZYERROR), status);
	return;
}
