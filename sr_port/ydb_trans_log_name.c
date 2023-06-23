/****************************************************************
 *								*
 * Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"

#include "iosp.h"
#include "ydb_trans_log_name.h"

LITREF	char	*ydbenvname[YDBENVINDX_MAX_INDEX];
LITREF	char	*gtmenvname[YDBENVINDX_MAX_INDEX];

/* This function is similar to "trans_log_name" except that it is given an "envindx" instead of an env var name.
 * For a given index "envindx", this function checks in the ydbenvname[] table if that index has a non-zero string literal
 * and if so checks if an environment variable with that name is defined and if so uses that. If not, it checks in the
 * gtmenvname[] table if the same index has a non-zero string literal and if so uses that as the environment variable
 * to check if it is defined. This way, ydb* takes precedence over gtm* if both exist.
 * If return is not SS_NOLOGNAM and is_ydb_env_match is non-NULL, *is_ydb_env_match is TRUE if the ydb* env var matched
 *	and FALSE if the gtm* env var matched. If return is SS_NOLOGNAM and is_ydb_env_match is non-NULL, *is_ydb_env_match
 *	is uninitialized and caller should not rely on this value.
 * In addition, if the gtm* env var exists but the ydb* env var does not, this routine does a "setenv" of the ydb* env var
 *	to have the exact same value as the gtm* env var.
 */
int4 ydb_trans_log_name(ydbenvindx_t envindx, mstr *trans, char *buffer, int4 buffer_len, boolean_t ignore_errors,
											boolean_t *is_ydb_env_match)
{
	char	*envnamestr[2];
	mstr	envname;
	int	i, status, status2, save_errno, len;
	char	tmpbuff[1024];

	envnamestr[0] = (char *)ydbenvname[envindx];
	envnamestr[1] = (char *)gtmenvname[envindx];
	/* Assert that there is always a non-null ydb* env var. Converse is not always true. */
	assert('\0' != envnamestr[0][0]);
	status = SS_NOLOGNAM;	/* needed mainly to avoid [clang-analyzer-core.uninitialized.UndefReturn] warning */
	for (i = 0; i < 2; i++)
	{
		envname.addr = envnamestr[i];
		envname.len = STRLEN(envname.addr);
		if (!envname.len)
			continue;
		status = trans_log_name(&envname, trans, buffer, buffer_len,
					!ignore_errors ? dont_sendmsg_on_log2long
							: ((IGNORE_ERRORS_NOSENDMSG == ignore_errors)
								? dont_sendmsg_on_log2long
								: do_sendmsg_on_log2long));
		switch(status)
		{
		case SS_NORMAL:
			if (NULL != is_ydb_env_match)
				*is_ydb_env_match = (0 == i) ? TRUE : FALSE;
			if (0 != i)
			{
				/* We matched on a gtm* env var whose corresponding ydb* env var was not defined.
				 * Set the ydb* env var to the same value.
				 */
				assert('\0' == trans->addr[trans->len]);
				status2 = setenv(envnamestr[0] + 1, trans->addr, TRUE);	/* + 1 to skip leading $ */
				if (status2 && !ignore_errors)
				{
					assert(-1 == status2);
					save_errno = errno;
					len = SNPRINTF(tmpbuff, SIZEOF(tmpbuff), "setenv(%s)", envnamestr[0] + 1);
					if (len >= SIZEOF(tmpbuff))	/* Output from SNPRINTF was truncated. */
						len = SIZEOF(tmpbuff);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
									len, tmpbuff, CALLFROM, save_errno);
				}
			}
			return status;
			break;
		case SS_NOLOGNAM:
			/* Env var corresponding to this iteration is not defined.
			 * Check if next iteration of for loop will give us a value. If so, use that.
			 * If no iterations give any env var, then return SS_NOLOGNAM to caller.
			 */
			break;
		case SS_LOG2LONG:
			if (NULL != is_ydb_env_match)
				*is_ydb_env_match = (0 == i) ? TRUE : FALSE;
			if (!ignore_errors)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3,
										envname.len, envname.addr, buffer_len - 1);
			return status;
			break;
		default:
			if (NULL != is_ydb_env_match)
				*is_ydb_env_match = (0 == i) ? TRUE : FALSE;
			if (!ignore_errors)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TRNLOGFAIL, 2, envname.len, envname.addr, status);
			return status;
			break;
		}
	}
	assert(SS_NOLOGNAM == status);
	return status;
}
