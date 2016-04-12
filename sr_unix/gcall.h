/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GCALL_ARGS_H__
#define __GCALL_ARGS_H__
#include "mdef.h"

typedef struct gcall_args_struct {
	intszofptr_t	callargs;
	intszofptr_t	truth;
	intszofptr_t	retval;
	intszofptr_t	mask;
	intszofptr_t	argcnt;
	mval		*argval[MAX_ACTUALS];
} gcall_args;

void ojchildparms(job_params_type *jparms, gcall_args *g_args, mval *arglst);

#endif
