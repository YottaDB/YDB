/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "io.h"
#include "term_setup.h"
#include "gtm_signal.h"
#include "sig_init.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "interlock.h"
#include "mdq.h"
#include "compiler.h"


GBLREF	boolean_t	ctrlc_on, hup_on;	/* TRUE in cenable mode; FALSE in nocenable mode */
GBLREF	io_pair		io_std_device;		/* standard device */
GBLREF	void		(*ctrlc_handler_ptr)();
GBLREF	volatile int4	outofband;		/* enumerated event ID*/

void  term_setup(boolean_t ctrlc_enable)
{
	int4			event_type;
	save_xfer_entry		*entry;
	struct sigaction	act;
	#define D_EVENT(a,b) (void *)&b
	void *set_event_table[] =
	{
		#include "outofband.h"
	};
	#undef D_EVENT
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	outofband = no_event;
	ctrlc_on = (tt == io_std_device.in->type) ? ctrlc_enable : FALSE;
	if (hup_on && (tt == io_std_device.in->type))
	{	/* if $PRINCIPAL, enable the hup_handler - similar to iop_hupenable code in iott_use.c */
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = ctrlc_handler_ptr;
		sigaction(SIGHUP, &act, 0);
	}
	for (event_type = 1; event_type < DEFERRED_EVENTS; event_type++)
	{	/* setup deferred_events */
		entry = &(TAREF1(save_xfer_root, event_type));
		entry->outofband = event_type;
		entry->param_val = 0;
		entry->set_fn = (void(*)(int))set_event_table[event_type];
	}
	TREF(save_xfer_root_ptr) = &(TREF(save_xfer_root));
	dqinit(TREF(save_xfer_root_ptr), ev_que);
}
