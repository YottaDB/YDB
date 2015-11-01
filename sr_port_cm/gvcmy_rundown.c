/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmy_rundown.h"
#include "gvcmy_close.h"

GBLREF struct NTD *ntd_root;

void gvcmy_rundown(void)
{
	struct CLB	*p;

	if (!ntd_root)
		return;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl) ; p != (struct CLB *)ntd_root ;
		p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl))
	{
		gvcmy_close(p);
	}
}
