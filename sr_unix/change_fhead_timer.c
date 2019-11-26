/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#define ONE_HOUR_MS	(10 * 100 * 60 * 60)	/* (decisec / millisec) * (decisec / sec) * (sec /min) * (min / hour) */
#define ONE_HOUR_NS	((uint8)NANOSECS_IN_SEC * 60 * 60)	/* (nanosec / sec) * (sec /min) * (min / hour) */
#define NANO		TRUE
#define MILLI		FALSE

void change_fhead_timer_common(char *timer_name, uint8 *timer_address, uint8 default_time, bool zero_is_ok, bool is_nano)
{
	uint8 		status, value;
	uint4		value_tmp;

	error_def(ERR_TIMRBADVAL);

	default_time = default_time * TIMER_SCALE;
	status = cli_present((char *)timer_name);
	if (status == CLI_NEGATED)
		*timer_address = zero_is_ok ? 0 : default_time;
	else if (status == CLI_PRESENT)
	{
		if (is_nano)
			status = cli_get_time_ns((char *)timer_name, &value);
		else
		{
			value_tmp = (uint4)value;
			status = cli_get_time_ms((char *)timer_name, &value_tmp);
			value = (uint8)value_tmp;
		}
		if (TRUE == status)
		{
			if (((0 == value) && (FALSE == zero_is_ok)) || (is_nano && (ONE_HOUR_NS < value)) ||
				(!is_nano && (ONE_HOUR_MS < value)))
				rts_error(VARLSTCNT(1) ERR_TIMRBADVAL);
			else	/* the above error is of type YDB-I- */
				*timer_address = value;

               } else
		   rts_error(VARLSTCNT(1) ERR_TIMRBADVAL);
	}
	return;
}

void change_fhead_timer_ms(char *timer_name, sm_int_ptr_t timer_address, int default_time, bool zero_is_ok)
/* default_time is in milliseconds */
{
	uint8		value;

	value = (uint8)timer_address[0];
	change_fhead_timer_common(timer_name, &value, (uint8)default_time, zero_is_ok, MILLI);
	timer_address[1] = 0;
	timer_address[0] = (uint4)value;
	return;
}


void change_fhead_timer_ns(char *timer_name, uint8 *timer_address, uint8 default_time, bool zero_is_ok)
/* default_time is in nanoseconds */
{
	change_fhead_timer_common(timer_name, timer_address, default_time, zero_is_ok, NANO);
}
