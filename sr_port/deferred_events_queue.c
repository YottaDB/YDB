/****************************************************************
 *								*
 * Copyright (c) 2018 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_sizeof.h"
#include "xfer_enum.h"
#include "fix_xfer_entry.h"
#include "deferred_events_queue.h"
#include "outofband.h"
#include "op.h"
#include "deferred_events.h"
#include "interlock.h"
#include "gtm_c_stack_trace.h"

GBLREF xfer_entry_t			xfer_table[];
GBLREF volatile int4			first_event;
GBLREF volatile int4			outofband;
GBLREF global_latch_t			outofband_queue_latch;
GBLREF uint4				process_id;

void save_xfer_queue_entry(int4 event_type, void (*set_fn)(int4 param), int4 param_val)
{
	volatile save_xfer_entry	*new_node;
	int				lcnt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	new_node = (save_xfer_entry *)malloc(SIZEOF(save_xfer_entry));
	new_node->outofband	= event_type;
	new_node->set_fn	= set_fn;
	new_node->param_val	= param_val;
	new_node->next = NULL;

	DBGDFRDEVNT((stderr, "save_queue_entry: Adding a new node for %d.\n",event_type));
	for (lcnt = 1; FALSE == (GET_SWAPLOCK(&outofband_queue_latch)); lcnt++)
	{
		if (MAXQUEUELOCKWAIT < lcnt)
		{
			GET_C_STACK_FROM_SCRIPT("EVENTQUEADDWAIT", process_id,
					outofband_queue_latch.u.parts.latch_pid, 1);
			assert(FALSE);
		}
		SHORT_SLEEP(1);
	}
	if ((TREF(save_xfer_root)) == NULL) /*First event, add to head*/
	{
		(TREF(save_xfer_root)) = new_node;
		(TREF(save_xfer_root))->next = NULL;
		(TREF(save_xfer_tail)) = (TREF(save_xfer_root));
	} else
	{	/* Add to the list */
		assert((TREF(save_xfer_tail)));
		(TREF(save_xfer_tail))->next = (save_xfer_entry *)new_node;
		(TREF(save_xfer_tail)) = (TREF(save_xfer_tail))->next;
	}
	RELEASE_SWAPLOCK(&outofband_queue_latch);
	DBGDFRDEVNT((stderr, "save_queue_entry: (TREF(save_xfer_root)) : outofband: %d set_fn: %d param_val:%d\n",
		new_node->outofband,new_node->set_fn,new_node->param_val));
}

void pop_reset_xfer_entry(int4* event_type, void (**set_fn)(int4 param), int4* param_val)
{
	volatile save_xfer_entry      *head_save;
	int lcnt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (lcnt = 1; FALSE == (GET_SWAPLOCK(&outofband_queue_latch)); lcnt++)
	{
		if (MAXQUEUELOCKWAIT < lcnt)
		{
			GET_C_STACK_FROM_SCRIPT("EVENTQUEPOPWAIT", process_id,
					outofband_queue_latch.u.parts.latch_pid, 1);
			assert(FALSE);
		}
		SHORT_SLEEP(1);
	}
	if ((TREF(save_xfer_root)))
	{
		*event_type = (TREF(save_xfer_root))->outofband;
		*set_fn	= (TREF(save_xfer_root))->set_fn;
		*param_val = (TREF(save_xfer_root))->param_val;
		head_save = (save_xfer_entry*)(TREF(save_xfer_root));
		(TREF(save_xfer_root)) = (TREF(save_xfer_root))->next;
		if (head_save == (TREF(save_xfer_tail)))
			(TREF(save_xfer_tail)) = NULL;
		RELEASE_SWAPLOCK(&outofband_queue_latch);
		free((void *)head_save);
		DBGDFRDEVNT((stderr, "pop_reset_xfer_entry: reset the xfer entry. Now event_type is: %d\n",*event_type));
	} else
		RELEASE_SWAPLOCK(&outofband_queue_latch);
}

save_xfer_entry* find_queue_entry(void (*set_fn)(int4 param), save_xfer_entry **qprev)
{
	save_xfer_entry *qc;
	int lcnt;
	DCL_THREADGBL_ACCESS;

 	SETUP_THREADGBL_ACCESS;
	qc = (save_xfer_entry *)(TREF(save_xfer_root));
	*qprev = NULL;
	for (lcnt = 1; FALSE == (GET_SWAPLOCK(&outofband_queue_latch)); lcnt++)
	{
		if (MAXQUEUELOCKWAIT < lcnt)
		{
			GET_C_STACK_FROM_SCRIPT("EVENTQUEFINDWAIT", process_id,
					outofband_queue_latch.u.parts.latch_pid, 1);
			assert(FALSE);
		}
		SHORT_SLEEP(1);
	}
	while (qc)
	{
		if (qc->set_fn == set_fn)
		{
			DBGDFRDEVNT((stderr, "find_queue_entry: returning %d\n",qc));
			RELEASE_SWAPLOCK(&outofband_queue_latch);
			return qc;
		}
		*qprev = qc;
		qc = qc->next;
	}
	RELEASE_SWAPLOCK(&outofband_queue_latch);
	DBGDFRDEVNT((stderr, "find_queue_entry: returning 0\n"));
	return NULL;
}

void remove_queue_entry(void (*set_fn)(int4 param))
{
	save_xfer_entry *qprev, *qp;
	int lcnt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (qp = find_queue_entry(set_fn, &qprev))  /* Assignment */
	{
		for (lcnt = 1; FALSE == (GET_SWAPLOCK(&outofband_queue_latch)); lcnt++)
		{
			if (MAXQUEUELOCKWAIT < lcnt)
			{
				GET_C_STACK_FROM_SCRIPT("EVENTQUERMWAIT", process_id,
						outofband_queue_latch.u.parts.latch_pid, 1);
				assert(FALSE);
			}
			SHORT_SLEEP(1);
		}
		if (qprev)
			qprev->next = qp->next;
		else
			(TREF(save_xfer_root)) = qp->next;
		free(qp);
		RELEASE_SWAPLOCK(&outofband_queue_latch);
	}
}

void empty_queue(void)
{
	save_xfer_entry *head_save, *intr;
	int lcnt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (lcnt = 1; FALSE == (GET_SWAPLOCK(&outofband_queue_latch)); lcnt++)
	{
		if (MAXQUEUELOCKWAIT < lcnt)
		{
			GET_C_STACK_FROM_SCRIPT("EVENTQUEEMPTYWAIT", process_id,
					outofband_queue_latch.u.parts.latch_pid, 1);
			assert(FALSE);
		}
		SHORT_SLEEP(1);
	}
	head_save = (save_xfer_entry*)(TREF(save_xfer_root));
	TREF(save_xfer_root) = NULL;
	TREF(save_xfer_tail) = NULL;
	RELEASE_SWAPLOCK(&outofband_queue_latch);
	while(head_save)
	{
		intr = head_save->next;
		free(head_save);
		head_save = intr;
	}
}
