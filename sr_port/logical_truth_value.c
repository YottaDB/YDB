/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"

#include "min_max.h"
#include "gtm_string.h" 	/* for STRNCASECMP */
#include "trans_log_name.h"
#include "iosp.h"
#include "logical_truth_value.h"

boolean_t logical_truth_value(mstr *log)
{
	/* return
	 * TRUE if the env variable/logical log is defined and evaluates to "TRUE" (or part thereof), or "YES" (or part thereof),
	 * 	or a non zero integer
	 * FALSE otherwise
	 */
	uint4		status;
	mstr		tn;
	char		buf[1024];
	boolean_t	zero, is_num;
	int		index;

	error_def(ERR_TRNLOGFAIL);

	tn.addr = buf;
	if (SS_NORMAL == (status = trans_log_name(log, &tn, buf)))
	{
		for (is_num = TRUE, zero = TRUE, index = 0; index < tn.len; index++)
		{
			if (!isdigit(buf[index]))
			{
				is_num = FALSE;
				break;
			}
			zero = (zero && ('0' == buf[index]));
		}
		return (!is_num ? (0 == STRNCASECMP(buf, LOGICAL_TRUE, MIN(sizeof(LOGICAL_TRUE) - 1, tn.len)) ||
		    		   0 == STRNCASECMP(buf, LOGICAL_YES, MIN(sizeof(LOGICAL_YES) - 1, tn.len)))
				: !zero);
	} else if (SS_NOLOGNAM == status)
		return (FALSE);
	rts_error(VARLSTCNT(5) ERR_TRNLOGFAIL, 2, log->len, log->addr, status);
	return (FALSE);
}
