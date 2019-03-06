/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GCALL_ARGS_H_INCLUDED
#define GCALL_ARGS_H_INCLUDED
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
