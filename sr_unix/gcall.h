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

#ifndef __GCALL_ARGS_H__
#define __GCALL_ARGS_H__

typedef struct gcall_args_struct {
	int4	callargs;
	int4	truth;
	int4	retval;
	int4	mask;
	int4	argcnt;
	mval	*argval[MAX_ACTUALS];
} gcall_args;

void ojchildparms(job_params_type *jparms, gcall_args *g_args, mval *arglst);

#endif
