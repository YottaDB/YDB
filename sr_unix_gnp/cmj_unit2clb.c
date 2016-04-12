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

struct CLB *cmj_unit2clb(struct NTD *tsk, cmi_unit_t unit)
{
	que_ent_ptr_t qp;
	struct CLB *p;

	for (qp = RELQUE2PTR(tsk->cqh.fl) ; qp != &tsk->cqh ;
			qp = RELQUE2PTR(p->cqe.fl))
	{
		p = QUEENT2CLB(qp, cqe);
		if (p->mun == unit)
			return p;
	}
	return NULL;
}
