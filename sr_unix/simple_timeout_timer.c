/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gt_timer.h"
#include "error.h"

/* This is a timer routine used by the TIMEOUT_INIT() macro.
 * When fired it simply sets the referenced timedout value to TRUE.
 */
void simple_timeout_timer(TID tid, int4 hd_len, boolean_t **timedout)
{
	**timedout = TRUE;
}

/* This is a condition handler established by the TIMEOUT_INIT() macro and reverted by the TIMEOUT_DONE() macro.
 * The real cleanup work is done in the TIMEOUT_INIT() code, as it has the necessary context, so this handler
 * simply does an unwind so that control can return to the establishment point.
 */
CONDITION_HANDLER(timer_cancel_ch)
{
	START_CH(TRUE);
	UNWIND(NULL, NULL);
}
