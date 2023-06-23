/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"

void walktree(mvar *n,void (*f)(),char *arg)
{
	while (TRUE)
	{
		if (n->lson)
			walktree(n->lson,f,arg);
		(*f)(n,arg);
		if (n->rson)
			n = n->rson;
		else
			break;
	}
	return;
}
