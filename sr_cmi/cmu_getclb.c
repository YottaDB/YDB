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

#include "cmihdr.h"
#include "cmidef.h"

GBLREF struct NTD *ntd_root;

struct CLB *cmu_getclb(node, task)
cmi_descriptor *node, *task;
{
	struct CLB *p;
	if (ntd_root)
	{
		for (p = RELQUE2PTR(ntd_root->cqh.fl) ; p != ntd_root ; p = RELQUE2PTR(p->cqe.fl))
		{
			if (p->nod.dsc$w_length == node->dsc$w_length
				&& memcmp(p->nod.dsc$a_pointer, node->dsc$a_pointer, p->nod.dsc$w_length) == 0)
			{
				if (p->tnd.dsc$w_length == task->dsc$w_length
					&& memcmp(p->tnd.dsc$a_pointer, task->dsc$a_pointer, p->tnd.dsc$w_length) == 0)
					return p;
			}
		}
	}
	return 0;
}
