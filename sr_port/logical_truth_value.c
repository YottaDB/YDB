/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "gtm_string.h"
#include "gtm_strings.h" 	/* for STRNCASECMP */
#include "trans_log_name.h"
#include "iosp.h"
#include "logical_truth_value.h"

/* returns the truth value based on the sense indicated by 'negate'.
 * If negate is FALSE (i.e. in regular mode),
 * 	returns TRUE if the env variable/logical log is defined and evaluates to "TRUE" (or part thereof),
 * 	or "YES" (or part thereof), or a non zero integer
 * 	returns FALSE otherwise
 * If negate is TRUE(i.e. in negative mode),
 * 	returns TRUE if the env variable/logical log is defined and evaluates to "FALSE" (or part thereof),
 * 	or "NO" (or part thereof), or a zero integer
 * 	returns FALSE otherwise
 */
boolean_t logical_truth_value(mstr *log, boolean_t negate, boolean_t *is_defined)
{
	int4		status;
	mstr		tn;
	char		buf[1024];
	boolean_t	zero, is_num;
	int		index;

	error_def(ERR_LOGTOOLONG);
	error_def(ERR_TRNLOGFAIL);

	tn.addr = buf;
	if (NULL != is_defined)
		*is_defined = FALSE;
	if (SS_NORMAL == (status = TRANS_LOG_NAME(log, &tn, buf, SIZEOF(buf), dont_sendmsg_on_log2long)))
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
			return (!is_num ? (0 == STRNCASECMP(buf, LOGICAL_TRUE, MIN(STR_LIT_LEN(LOGICAL_TRUE), tn.len)) ||
		    		   0 == STRNCASECMP(buf, LOGICAL_YES, MIN(STR_LIT_LEN(LOGICAL_YES), tn.len)))
				: !zero);
		} else
		{ /* negative mode */
			return (!is_num ? (0 == STRNCASECMP(buf, LOGICAL_FALSE, MIN(STR_LIT_LEN(LOGICAL_FALSE), tn.len)) ||
		    		   0 == STRNCASECMP(buf, LOGICAL_NO, MIN(STR_LIT_LEN(LOGICAL_NO), tn.len)))
				: zero);
		}
	} else if (SS_NOLOGNAM == status)
		return (FALSE);
#	ifdef UNIX
	else if (SS_LOG2LONG == status)
	{
		rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, log->len, log->addr, SIZEOF(buf) - 1);
		return (FALSE);
	}
#	endif
	else
	{
		rts_error(VARLSTCNT(5) ERR_TRNLOGFAIL, 2, log->len, log->addr, status);
		return (FALSE);
	}
}
