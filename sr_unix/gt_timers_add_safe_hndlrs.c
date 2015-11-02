/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gt_timer.h"
#include "gt_timers_add_safe_hndlrs.h"
#include "heartbeat_timer.h"
#include "semwt2long_handler.h"
#include "secshr_client.h"

/* This optional routine adds entries to the safe_handlers[] array. It is separate because while most executables need
 * these timers listed, there is one executable (gtmsecshr) that decidedly does not - gtmsecshr. If these routines are
 * part of gtmsecshr, they cause large numers of other routines that should definitely not be part of a root privileged
 * executable to be pulled in.
 */

void gt_timers_add_safe_hndlrs(void)
{
	add_safe_timer_handler(3, semwt2long_handler, client_timer_handler, heartbeat_timer);
}
