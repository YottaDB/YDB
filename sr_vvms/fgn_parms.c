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
#include <descrip.h>
#include "desc2mval.h"

static readonly mval empty_mv;
static mval ret_mv;

int fgn_parms(
int output,
mval *margs[],
int maxargs,
int *fgnargs)
{
	int	i,argcnt, *fparms;
	mval	**v;
	error_def(ERR_MAXACTARG);

	v = margs;
	fparms = fgnargs;
	argcnt = *fparms++;
	if (output)
	{	argcnt--;
		fparms++;
		margs[maxargs] = &ret_mv;	/* extra slot on the end just to take return value */

	}
	else
		margs[maxargs] = 0;

	if (argcnt > MAX_ACTUALS)
		rts_error(VARLSTCNT(1) ERR_MAXACTARG);

	for (i = 0; i < argcnt; i++)
	{	*v = push_mval(&empty_mv);
		desc2mval(*fparms++, *v++);
	}
	return argcnt;
}
