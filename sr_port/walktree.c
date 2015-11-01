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
#include "compiler.h"

void walktree(mvar *n,void (*f)(),char *arg)
{
if (n->lson)
	walktree(n->lson,f,arg);
(*f)(n,arg);
if (n->rson)
	walktree(n->rson,f,arg);
return;
}
