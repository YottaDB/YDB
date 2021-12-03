/****************************************************************
 *								*
 * Copyright (c) 2006-2021 Fidelity National Information	*
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

GBLREF	int			(* volatile xfer_table[])();
GBLREF	void			(*tp_timeout_clear_ptr)(void);
GBLREF	volatile int4		outofband;
GBLREF	volatile boolean_t	tp_timeout_set_xfer;
GBLREF	void			(*ztimeout_clear_ptr)(void);

void outofband_clear(void)
{
	boolean_t status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (ctrlc == outofband)
	{
		assert(!tp_timeout_set_xfer);	/* TP timeout should not have been the primary event */
		(*tp_timeout_clear_ptr)();
		(*ztimeout_clear_ptr)();
		TAREF1(save_xfer_root, ctrap).param_val = 0;
		EMPTY_XFER_QUEUE_ENTRIES;
	}
	status = xfer_reset_handlers(outofband);
	assert(TRUE == status);
}
