/****************************************************************
 *								*
 * Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
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
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_strings.h" 	/* for STRNCASECMP */

#include "min_max.h"
#include "iosp.h"
#include "ydb_logical_truth_value.h"
#include "ydb_trans_log_name.h"

/* returns the truth value based on the sense indicated by 'negate'.
 * If negate is FALSE (i.e. in regular mode),
 * 	returns TRUE if the ydb/gtm env variable corresponding to envindx is defined and evaluates to "TRUE" (or part thereof),
 * 	or "YES" (or part thereof), or a non zero integer
 * 	returns FALSE otherwise
 * If negate is TRUE(i.e. in negative mode),
 * 	returns TRUE if the ydb/gtm env variable corresponding to envindx is defined and evaluates to "FALSE" (or part thereof),
 * 	or "NO" (or part thereof), or a zero integer
 * 	returns FALSE otherwise
 */
boolean_t ydb_logical_truth_value(ydbenvindx_t envindx, boolean_t negate, boolean_t *is_defined)
{
	int4		status;
	mstr		tn;
	char		buf[1024];
	boolean_t	zero, is_num;
	int		index;

	tn.addr = buf;
	if (NULL != is_defined)
		*is_defined = FALSE;
	if (SS_NORMAL == (status = ydb_trans_log_name(envindx, &tn, buf, SIZEOF(buf), IGNORE_ERRORS_TRUE, NULL)))
	{
		if (NULL != is_defined)
			*is_defined = TRUE;
		if (tn.len <= 0)
			return FALSE;
		for (is_num = TRUE, zero = TRUE, index = 0; index < tn.len; index++)
		{
			if (!ISDIGIT_ASCII(buf[index]))
			{
				is_num = FALSE;
				break;
			}
			zero = (zero && ('0' == buf[index]));
		}
		if (!negate)
		{ /* regular mode */
			return (!is_num
				? (!STRNCASECMP(buf, LOGICAL_TRUE, tn.len) || !STRNCASECMP(buf, LOGICAL_YES, tn.len))
				: !zero);
		} else
		{ /* negative mode */
			return (!is_num
				? (!STRNCASECMP(buf, LOGICAL_FALSE, tn.len) || !STRNCASECMP(buf, LOGICAL_NO, tn.len))
				: zero);
		}
	} else
	{
		assert((SS_NOLOGNAM == status) || (SS_LOG2LONG == status));
		return (FALSE);
	}
}
