/****************************************************************
 *								*
 *	Copyright 2004, 2009 Fidelity Information Services, Inc	*
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

#include "trans_log_name.h"
#include "iosp.h"
#include "trans_numeric.h"

uint4 trans_numeric(mstr *log, boolean_t *is_defined,  boolean_t ignore_errors)
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

	error_def(ERR_LOGTOOLONG);
	error_def(ERR_TRNLOGFAIL);

	*is_defined = FALSE;
	if (SS_NORMAL == (status = TRANS_LOG_NAME(log, &tn, buf, SIZEOF(buf),
							ignore_errors ? do_sendmsg_on_log2long : dont_sendmsg_on_log2long)))
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

	if (!ignore_errors)
	{	/* Only give errors if we can handle them */
#		ifdef UNIX
		if (SS_LOG2LONG == status)
			rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, log->len, log->addr, SIZEOF(buf) - 1);
		else
#		endif
			rts_error(VARLSTCNT(5) ERR_TRNLOGFAIL, 2, log->len, log->addr, status);
	}
	return 0;
}
