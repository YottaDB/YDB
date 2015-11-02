/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
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
#include "gtm_stat.h"
#include "gtm_time.h"
#include "gtm_rename.h"
#include "eintr_wrappers.h"
#include "iosp.h"

#define TIME_EXT_FMT "_%Y%j%H%M%S"   	/* .yearjuliendayhoursminutesseconds */

/* This appends timestamp from file (fn) last modified status time. Result is returned in same string fn.
 * Return SS_NORMAL for success */
uint4 append_time_stamp(char *fn, int fn_len, int *app_len, uint4 *ustatus)
{
	struct stat	stat_buf;
	struct tm	*tm_struct;
	int		status;
	size_t          tt;

	*ustatus = SS_NORMAL;
	STAT_FILE(fn, &stat_buf, status);
	if (-1 == status) /* if file fn does not exist */
		return errno;
	assert(0 <  MAX_FN_LEN - fn_len - 1);
	tm_struct = localtime(&(stat_buf.st_ctime));
	STRFTIME(&fn[fn_len], MAX_FN_LEN - fn_len - 1, TIME_EXT_FMT, tm_struct, tt);
	*app_len = (int)tt;
	return SS_NORMAL;
}

