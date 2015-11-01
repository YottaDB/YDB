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


#include "xfer_enum.h"
#include "deferred_events.h"
#include "outofband.h"
boolean_t xfer_reset_handlers(int4 event_type);

GBLREF int 		(* volatile xfer_table[])();
GBLREF void		(*tp_timeout_clear_ptr)(void);
GBLREF volatile int4	ctrap_action_is, outofband;

void outofband_clear(void)
{

	boolean_t status;

	status = xfer_reset_handlers(outofband_event);
	assert(TRUE == status);

	/* Note there may be a race condition here:
	 * Ideally, the flags below should be reset only
	 * due to this call, but an outofband event right
	 * here would also cause it.
	 */
	if (ctrlc == outofband || ctrly == outofband)
		(*tp_timeout_clear_ptr)();
	if (outofband)
		outofband = ctrap_action_is = 0;
}
