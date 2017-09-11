/****************************************************************
 *                                                              *
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_logicals.h"
#include "error.h"
#include "gtm_env_xlate_init.h"
#include "stringpool.h"

GBLREF mstr	env_gtm_env_xlate;

error_def(ERR_LOGTOOLONG);
error_def(ERR_TRNLOGFAIL);

void gtm_env_xlate_init(void)
{
	int4		status;
	mstr		val, tn;
	char		buf[GTM_PATH_MAX];

	val.addr = GTM_ENV_XLATE;
	val.len =  STR_LIT_LEN(GTM_ENV_XLATE);
	env_gtm_env_xlate.len = 0; /* default */
	if (SS_NORMAL != (status = TRANS_LOG_NAME(&val, &tn, buf, SIZEOF(buf), dont_sendmsg_on_log2long)))
	{
		if (SS_NOLOGNAM == status)
			return;
		else if (SS_LOG2LONG == status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3, LEN_AND_LIT(GTM_ENV_XLATE), SIZEOF(buf) - 1);
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TRNLOGFAIL, 2, LEN_AND_LIT(GTM_ENV_XLATE), status);
	}
	if (0 == tn.len)
		return;
	env_gtm_env_xlate.len = tn.len;
	env_gtm_env_xlate.addr = (char *)malloc(tn.len);
	memcpy(env_gtm_env_xlate.addr, buf, tn.len);
	return;
}
