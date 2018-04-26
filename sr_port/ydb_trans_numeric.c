/****************************************************************
 *								*
 * Copyright 2004, 2009 Fidelity Information Services, Inc	*
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

#include "gtm_ctype.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"

#include "iosp.h"
#include "ydb_trans_numeric.h"
#include "ydb_trans_log_name.h"

uint4 ydb_trans_numeric(ydbenvindx_t envindx, boolean_t *is_defined, boolean_t ignore_errors, boolean_t *is_ydb_env_match)
{
	/* return
	 * - 0 on error if ignore_errors is set (otherwise error is raised and no return is made) or
	 *   if logical/envvar is undefined.
	 * - an unsigned int containing the numeric value (or as much as could be determined) from
	 *   the logical/envvar string value (up to the first non-numeric digit. Characters accepted
	 *   are those read by the strtoul() function.
	 */
	int4		status;
	uint4		value;
	mstr		tn;
	char		buf[MAX_TRANS_NAME_LEN], *endptr;

	assert((YDBENVINDX_MIN_INDEX < envindx) && (YDBENVINDX_MAX_INDEX > envindx));
	*is_defined = FALSE;
	if (SS_NORMAL == (status = ydb_trans_log_name(envindx, &tn, buf, SIZEOF(buf), ignore_errors, is_ydb_env_match)))
	{	/* Translation was successful */
		*is_defined = TRUE;
		assert(tn.len < SIZEOF(buf));
		endptr = tn.addr + tn.len;
		*endptr = '\0';
		value = (uint4)STRTOUL(buf, &endptr, 0);	/* Base 0 allows base 10, 0x or octal input */
		/* At this point, if '\0' == *endptr, the entire string was successfully consumed as
		   a numeric string. If not, endptr has been updated to point to the errant chars. We
		   currently have no clients who care about this so there is no expansion on this but
		   this could be added at this point. For now we just return whatever numeric value
		   (if any) was gleened..
		*/
		return value;
	} else if (SS_NOLOGNAM == status)	/* Not defined */
		return 0;
	assert(ignore_errors);	/* "ydb_trans_log_name" should have done the rts_error in the "!ignore_errors" case */
	return 0;
}
