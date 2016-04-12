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
#include "gtcm_find_proc.h"

struct CLB *gtcm_find_proc(struct NTD *tsk, unsigned short pnum)
{
	struct CLB *ptr;

	for (ptr = (struct CLB *)RELQUE2PTR(tsk->cqh.fl);
		ptr != (struct CLB *)tsk && ((connection_struct *)ptr->usr)->procnum != pnum ;
		ptr = (struct CLB *)RELQUE2PTR(ptr->cqe.fl))
		;
	if (ptr == (struct CLB *)tsk)
		ptr = NULL;
	return ptr;
}
