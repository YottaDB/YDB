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

struct CLB *cmj_unit2clb(tsk,unit)
struct NTD *tsk;
unsigned short unit;
{
	struct CLB *p;

	for (p = RELQUE2PTR(tsk->cqh.fl) ; p != tsk ; p = RELQUE2PTR(p->cqe.fl))
	{
		if (p->mun == unit)
			return p;
	}
	return 0;
}
