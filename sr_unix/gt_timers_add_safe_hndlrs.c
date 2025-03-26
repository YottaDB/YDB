/****************************************************************
 *								*
 * Copyright (c) 2012-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
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
#include "secshr_client.h"
#include "jnl_file_close_timer.h"
#ifdef DEBUG
#include "fake_enospc.h"
#include "wbox_test_init.h"
#include "ztimeout_routines.h"
#endif

/* This optional routine adds entries to the safe_handlers[] array. It is separate because while most executables need
 * these timers listed, there is one executable (gtmsecshr) that decidedly does not - gtmsecshr. If these routines are
 * part of gtmsecshr, they cause large numbers of other routines that should definitely not be part of a root privileged
 * executable to be pulled in.
 */

void gt_timers_add_safe_hndlrs(void)
{
	add_safe_timer_handler(1, simple_timeout_timer);
#	ifdef DEBUG
	add_safe_timer_handler(2, fake_enospc, handle_deferred_syslog);
	if (WBTEST_ENABLED(WBTEST_DEFERRED_TIMERS))
		add_safe_timer_handler(1, ztimeout_expired); /* Give $ztimeout a pass */
#	endif
}
