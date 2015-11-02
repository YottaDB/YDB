/****************************************************************
 *                                                              *
 *      Copyright 2001, 2009 Fidelity Information Services, Inc  *
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

void gtm_env_xlate_init(void)
{
	int4		status;
	mstr		val, tn;
	char		buf[GTM_PATH_MAX];

	error_def(ERR_TRNLOGFAIL);
	error_def(ERR_LOGTOOLONG);

	val.addr = GTM_ENV_XLATE;
	val.len =  STR_LIT_LEN(GTM_ENV_XLATE);
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &tn, buf, SIZEOF(buf), dont_sendmsg_on_log2long)))
	{
		UNIX_ONLY(
			env_gtm_env_xlate.len = tn.len;
			env_gtm_env_xlate.addr = (char *)malloc(tn.len);
			memcpy(env_gtm_env_xlate.addr, buf, tn.len);
		)
		VMS_ONLY(
			/* In op_gvextnam, the logical name is used in VMS, rather than its value (by lib$find_image_symbol),
			 * so only whether the logical name translates is checked here.
			 */
			env_gtm_env_xlate.len = val.len;
			env_gtm_env_xlate.addr = val.addr;
		)
	} else if (SS_NOLOGNAM == status)
		env_gtm_env_xlate.len = 0;
#	ifdef UNIX
	else if (SS_LOG2LONG == status)
		rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, LEN_AND_LIT(GTM_ENV_XLATE), SIZEOF(buf) - 1);
#	endif
	else
		rts_error(VARLSTCNT(5) ERR_TRNLOGFAIL, 2, LEN_AND_LIT(GTM_ENV_XLATE), status);
	return;
}
