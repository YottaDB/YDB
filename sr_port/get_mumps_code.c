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

#include "gtm_string.h"

#include "matchc.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "mvalconv.h"
#include "op.h"
#include "dollar_zlevel.h"
#include "gtm_caseconv.h"
#include "get_command_line.h"
#include "gtmmsg.h"
#include "get_mumps_code.h"

/*
 * -----------------------------------------------
 * get_mumps_code(stackpointer, place, mumpscode)
 *
 * Parameters:
 *	stackpointer	- pointer to frame for which code is fetched
 *	place		- pointer to mval that receives the $ZPOSITION
 *      mumpscode	- pointer to mval that receives the source code
 * -----------------------------------------------
 */

GBLREF	spdesc		stringpool;
GBLREF	unsigned char	*error_last_mpc_err;
GBLREF	stack_frame	*error_last_frame_err;

/* pos_str will contain label+offset^routine
 * 8 characters for label, 3 for offset and 8 for routine
 * (plus 3 characters for separators)
 * gives a maximum of 22. Allow for some leeway in case
 * someone might need more than 3 digits for the offset...
 */
#define MAX_POS_LEN 32

void get_mumps_code(stack_frame *fp, mval *place, mval *mumpscode)
{
	unsigned char	pos_str[MAX_POS_LEN];
	int		ips;
	int		offset;
	int		pos_len;
	int		s1;
	int		s2;
	mval		label;
	mval		routine;
	unsigned char	*mpc;
	error_def(ERR_TEXT);

	mpc = (fp == error_last_frame_err) ? error_last_mpc_err : fp->mpc;
	pos_len = symb_line(mpc, &pos_str[0], 0, fp->rvector) - (uchar_ptr_t)&pos_str[0];
	/* return $Text(pos_str) */
	if (mumpscode)
	{
		label.mvtype = MV_STR;
		routine.mvtype = MV_STR;
		mumpscode->mvtype = MV_STR;
		label.str.len = pos_len;
		label.str.addr = (char *)&pos_str[0];
		routine.str.len = 0;
		for (ips = 0, s1 = s2 = -1; ips < pos_len; ips++)
		{
			if ('+' == pos_str[ips])
				s1 = ips;
			if ('^' == pos_str[ips])
				s2 = ips;
		}
		if (s2 >= 0)
		{
			routine.str.addr = (char *)&pos_str[s2 + 1];
			routine.str.len = pos_len - s2 - 1;
			label.str.len = s2;
		}
		offset = 0;
		if (s1 >= 0)
		{
			label.str.len = s1;
			if (s2 < 0)
				s2 = pos_len;
			for (ips = s1 + 1; ips < s2; ips++)
				offset = offset * 10 - '0' + pos_str[ips];
		}
		op_fntext(&label, offset, &routine, mumpscode);
	}
	if (place)
	{
		place->mvtype = MV_STR;
		place->str.len = pos_len;
		if (stringpool.top - stringpool.free < pos_len)
			stp_gcol(pos_len);
		place->str.addr = (char *)stringpool.free;
		memcpy(place->str.addr, pos_str, pos_len);
		stringpool.free += pos_len;
	}
}
