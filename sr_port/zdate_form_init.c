/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_logicals.h"
#include "io.h"
#include "iosp.h"
#include "logical_truth_value.h"
#include "trans_log_name.h"
#include "startup.h"
#include "stringpool.h"
#include "zdate_form_init.h"

GBLREF spdesc	stringpool;
GBLREF int4	zdate_form;

void zdate_form_init(struct startup_vector *svec)
{
	uint4		status;
	mstr		val, tn;
	char		buf[MAX_TRANS_NAME_LEN];
	error_def(ERR_TRNLOGFAIL);

	val.addr = ZDATE_FORM;
	val.len = STR_LIT_LEN(ZDATE_FORM);
	if (SS_NORMAL == (status = trans_log_name(&val, &tn, buf)))
		zdate_form = logical_truth_value(&val);
	else if (SS_NOLOGNAM == status)
		zdate_form = svec->zdate_form;
	else
		rts_error(VARLSTCNT(5) ERR_TRNLOGFAIL, 2, LEN_AND_LIT(SYSID), status);
}
