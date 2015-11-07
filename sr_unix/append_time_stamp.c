/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_time.h"
#include "gtm_rename.h"

#include "eintr_wrappers.h"
#include "iosp.h"

/* Append the formatted timestamp to the file name (fn); *fn_len contains the current length of the filename and at exit from this
 * function, it is updated to reflect the new length.
 */
uint4 append_time_stamp(char *fn, int *fn_len, jnl_tm_t now)
{
	struct tm	*tm_struct;
	time_t		tt_now;
	size_t          tm_str_len;

	assert(0 <  MAX_FN_LEN - *fn_len - 1);
	tt_now = (time_t)now;
	GTM_LOCALTIME(tm_struct, &tt_now);
	STRFTIME(&fn[*fn_len], MAX_FN_LEN - *fn_len - 1, JNLSWITCH_TM_FMT, tm_struct, tm_str_len);
	*fn_len += (int)tm_str_len;
	return SS_NORMAL;
}

