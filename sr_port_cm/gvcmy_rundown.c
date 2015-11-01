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
#include "cmidef.h"
#include "cmmdef.h"

GBLREF struct NTD *ntd_root;

gvcmy_rundown()
{
	struct CLB	*p;

	if (!ntd_root)
		return;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl) ; p != (struct CLB *)ntd_root ;
		p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl))
	{
		gvcmy_close(p);
	}
	return;
}
