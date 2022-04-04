/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRY_EVENT_POP_INCLUDED
#define TRY_EVENT_POP_INCLUDED

GBLREF	volatile boolean_t		dollar_zininterrupt;
GBLREF	dollar_ecode_type		dollar_ecode;
GBLREF	volatile int4			outofband;

#define TRY_EVENT_POP try_event_pop()

static inline void try_event_pop(void)
{
	int4		event_type, param_val;
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((no_event == outofband)
		&& (no_event != (TREF(save_xfer_root_ptr))->ev_que.fl->outofband))
	{	/* perhaps pop an_event */
		assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
		DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
		POP_XFER_QUEUE_ENTRY(&event_type, &param_val);
		DBGDFRDEVNT((stderr, "%d %s: pop_reset_xfer returned event %d\n", __LINE__, __FILE__, event_type));
		ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
		if (no_event != event_type)
			xfer_set_handlers(event_type, param_val, TRUE);
	}
	return;
}

#endif
