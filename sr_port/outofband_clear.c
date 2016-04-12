/****************************************************************
 *								*
 *	Copyright 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "xfer_enum.h"
#include "deferred_events.h"
#include "outofband.h"

GBLREF	int			(* volatile xfer_table[])();
GBLREF	void			(*tp_timeout_clear_ptr)(void);
GBLREF	volatile int4		ctrap_action_is, outofband;
GBLREF	volatile boolean_t	tp_timeout_set_xfer;

void outofband_clear(void)
{
	boolean_t status;

	if (ctrlc == outofband || ctrly == outofband)
	{
		assert(!tp_timeout_set_xfer);	/* TP timeout should not have been the primary event */
		(*tp_timeout_clear_ptr)();
	}
	status = xfer_reset_handlers(outofband_event);
	assert(TRUE == status);
	assert(0 == outofband);
}
