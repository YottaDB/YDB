/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <iodef.h>
#include <efndef.h>
#include "cmihdr.h"
#include "cmidef.h"
#include "efn.h"

uint4 cmj_iostart(struct CLB *lnk, uint4 operation, unsigned int state)
{
	uint4 status;
	error_def(CMI_DCNINPROG);
	error_def(CMI_LNKNOTIDLE);
	void cmj_ast();

	if (lnk->sta != CM_CLB_IDLE)
		return (lnk->sta == CM_CLB_DISCONNECT) ? CMI_DCNINPROG : CMI_LNKNOTIDLE;
        lnk->sta = (unsigned char)state;
	if (lnk->ast)
	{
                status = sys$qio(EFN$C_ENF, lnk->dch, operation,
			&lnk->ios, cmj_ast, lnk,
			lnk->mbf, lnk->cbl, 0, 0, 0, 0);
	} else
	{
                status = sys$qio(EFN$C_ENF, lnk->dch, operation,
			&lnk->ios, 0, 0, lnk->mbf, lnk->cbl, 0, 0, 0, 0);
		if (1 & status)
		{
			status = sys$synch(EFN$C_ENF, &lnk->ios);
			cmj_fini(lnk);
			if (1 & status)
				status = lnk->ios.status;
		}
	}
	return status;
}
