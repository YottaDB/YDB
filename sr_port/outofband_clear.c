/****************************************************************
 *								*
 * Copyright (c) 2006-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "xfer_enum.h"
#include "have_crit.h"
#include "mdq.h"
#include "deferred_events.h"
#include "deferred_events_queue.h"
#include "ztimeout_routines.h"

GBLREF	int			(* volatile xfer_table[])();
GBLREF	void			(*tp_timeout_clear_ptr)(void);
GBLREF	volatile int4		outofband;
GBLREF	volatile boolean_t	tp_timeout_set_xfer;

void outofband_clear(void)
{	/* called by .c files: op_fetchintrrpt, op_forintrrpt, op_startintrrpt */
	boolean_t status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGDFRDEVNT((stderr, "%d %s: outofband_clear oob:%d\n", __LINE__, __FILE__, outofband));
	if (ctrlc == outofband)
	{
		assert(!tp_timeout_set_xfer);	/* TP timeout should not have been the primary event */
		(*tp_timeout_clear_ptr)();
		ztimeout_clear_timer();
		TAREF1(save_xfer_root, ctrap).param_val = 0;
		EMPTY_XFER_QUEUE_ENTRIES;
	} else
	{
		status = xfer_reset_if_setter(outofband);
		assert(TRUE == status);
	}
}
