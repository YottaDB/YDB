/****************************************************************
 *								*
 * Copyright (c) 2018 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "stringpool.h"
#include "time.h"
#include "gt_timer.h"
#include "ztimeout_routines.h"
#include "deferred_events.h"
#include "min_max.h"

GBLREF spdesc           stringpool;

void get_ztimeout(mval *result)
{
	unsigned char	*cp;
	char		*curr_end_ptr, *arr_end_ptr;
	char		full_ztimeout[MAX_STRLEN];
	int		req_len;
	long int 	ms;
	ABS_TIME 	cur_time;
	DCL_THREADGBL_ACCESS;

 	SETUP_THREADGBL_ACCESS;
	memset(full_ztimeout, '\0', MAX_STRLEN);
	if ((((TREF(dollar_ztimeout)).ztimeout_seconds.m[1] / MV_BIAS) == -1) && (!(TREF(dollar_ztimeout)).ztimeout_vector.str.addr))
	{
		SNPRINTF(full_ztimeout, MAX_STRLEN, "%d", ((TREF(dollar_ztimeout)).ztimeout_seconds.m[1] / MV_BIAS));
		curr_end_ptr = full_ztimeout + STRLEN(full_ztimeout);
	}
	else
	{
		sys_get_curr_time(&cur_time);
		cur_time = sub_abs_time(&(TREF(dollar_ztimeout)).end_time, &cur_time);
		if (0 <= cur_time.at_sec)
		{
			DBGDFRDEVNT((stderr,"cur_time.at_usec is: %d\n", cur_time.at_usec));
			ms = DIVIDE_ROUND_DOWN(cur_time.at_usec, MICROSECS_IN_MSEC);
			SNPRINTF(full_ztimeout, MAX_STRLEN, "%ld.%ld", cur_time.at_sec, ms);
		}
		else
		{
			ms = 0;
			SNPRINTF(full_ztimeout, MAX_STRLEN, "%ld", ms);
		}
		curr_end_ptr = full_ztimeout + STRLEN(full_ztimeout);
		if ((TREF(dollar_ztimeout)).ztimeout_vector.str.addr)
		{
			req_len = STRLEN(":") + (TREF(dollar_ztimeout)).ztimeout_vector.str.len;
			arr_end_ptr = full_ztimeout + MAX_STRLEN - 1;
			assert(arr_end_ptr >= (curr_end_ptr + req_len));
			MEMCPY_LIT(curr_end_ptr , ":");
			curr_end_ptr += STRLEN(":");
			assert(curr_end_ptr <= arr_end_ptr);
			req_len -= STRLEN(":");
			memcpy(curr_end_ptr, (TREF(dollar_ztimeout)).ztimeout_vector.str.addr,
					MIN(req_len, (arr_end_ptr - curr_end_ptr)));
		}
	}
	cp = stringpool.free;
	STRNLEN(full_ztimeout, MAX_STRLEN, req_len);
	assert(req_len < MAX_STRLEN);
	ENSURE_STP_FREE_SPACE(req_len);
	stringpool.free += req_len;
	result->str.addr = (char *)cp;
	result->str.len = req_len;
	result->mvtype = MV_STR;
	memcpy(cp, full_ztimeout, req_len);
	assert(IS_AT_END_OF_STRINGPOOL(cp, req_len));
        return;

}
