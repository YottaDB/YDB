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
#include "gdsroot.h"
#include "ccp.h"
#include "crit_wake.h"

void ccp_pndg_proc_wake( ccp_wait_head *list)
{
	ccp_wait *e, *ex;

	for (e = list->first ; e ; e = ex)
	{
		crit_wake(&e->pid);
		ex = e->next;
		free(e);
	}
	list->first = 0;
	list->last = &list->first;
	return;
}
