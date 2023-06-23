/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "timersp.h"

/* TIM_DEFER_DBSYNC is the defer-time used in wcs_clean_dbsync_timer() to flush the filehdr (and epoch if before imaging).
 * TIMER_SCALE corresponds to 1 msec and is appropriately defined for Unix and VMS in timersp.h.
 */

#define	TIM_DEFER_DBSYNC		(uint8)(NANOSECS_IN_SEC)*(5 * TIMER_SCALE)	/* 5 sec */

typedef enum
{	tim_wcs_starved = 10,
	tim_wcs_flu_mod,
	n_timers
}timers;

void change_fhead_timer_common(char *timer_name, uint8 *timer_address, uint8 default_time,
	bool zero_is_ok, bool is_nano);
void change_fhead_timer_ns(char *timer_name, uint8 *timer_address, uint8 default_time,
	bool zero_is_ok);
void change_fhead_timer_ms(char *timer_name, sm_int_ptr_t timer_address, int default_time,
	bool zero_is_ok);
