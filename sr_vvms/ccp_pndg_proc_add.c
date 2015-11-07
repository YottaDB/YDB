/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "ccp.h"

void ccp_pndg_proc_add(list, pid)
ccp_wait_head *list;
int4 pid;
{
	/* add a processes id to a ccp_wait list.
	   we add at the end of the list so that wakeup's
	   will be delivered to VMS in FIFO order
	*/
	ccp_wait *e;

	for (e = list->first; e; e = e->next)
	{	if (e->pid == pid)
			return;
	}
	e = malloc(SIZEOF(*e));
	e->pid = pid;
	e->next = 0;
	*(list->last) = &(e->next);
	list->last = e;
	return;
}
