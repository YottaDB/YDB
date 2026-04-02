/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef CALLG_H
#define CALLG_H

typedef struct gparam_list_struct
{
	intszofptr_t	n;
	void    	*arg[MAX_ACTUALS -4 + PUSH_PARM_OVERHEAD];  /* -3 to make assert in callg.c happy */
} gparam_list;

typedef	INTPTR_T (*callgfnptr)(intszofptr_t cnt, ...);
INTPTR_T callg(callgfnptr, gparam_list *);
void callg_signal(void *);

#endif
