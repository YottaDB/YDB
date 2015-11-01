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

#include "cli.h"
#include "iosp.h"
#include "timers.h"

#define ONE_HOUR	(10 * 100 * 60 * 60)	/* (decisec / millisec) * (decisec / sec) * (sec /min) * (min / hour) */


void change_fhead_timer(char *timer_name, sm_int_ptr_t timer_address, int default_time, bool zero_is_ok)
/* default_time is in milliseconds */
{
	uint4 		status, value;

	error_def(ERR_TIMRBADVAL);

	default_time = default_time * TIMER_SCALE;
	timer_address[1] = 0;
	status = cli_present((char *)timer_name);
	if (status == CLI_NEGATED)
		timer_address[0] = zero_is_ok ? 0 : default_time;
	else if (status == CLI_PRESENT)
	{
		status = cli_get_time((char *)timer_name, &value);
		if (TRUE == status)
		{
			if ((ONE_HOUR < value) || ((0 == value) && (FALSE == zero_is_ok)))
				rts_error(VARLSTCNT(1) ERR_TIMRBADVAL);
			else	/* the above error is of type GTM-I- */
				timer_address[0] = value;

               } else
		   rts_error(VARLSTCNT(1) ERR_TIMRBADVAL);
	}
	return;
}
