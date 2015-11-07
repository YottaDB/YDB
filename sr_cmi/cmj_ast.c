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

void cmj_ast(lnk)
struct CLB *lnk;
{
	uint4 status;
	error_def(CMI_DCNINPROG);
	error_def(CMI_LNKNOTIDLE);

	cmj_fini(lnk);
	if (lnk->ast != 0)
		(*lnk->ast)(lnk);
	return;
}
