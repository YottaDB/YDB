/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gt_timer.h"
#include "wake_alarm.h"

GBLREF bool	out_of_time;

void wake_alarm(void)
{
	out_of_time = TRUE;
	GT_WAKE;
}
