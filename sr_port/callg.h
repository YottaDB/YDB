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
#ifndef CALLG_H
#define CALLG_H
#include "fgncalsp.h"

typedef struct gparam_list_struct
{
	intszofptr_t	n;
	void    	*arg[MAXIMUM_PARAMETERS];
} gparam_list;

/* defined as macro on MVS */
#ifndef callg
typedef	INTPTR_T (*callgfnptr)(intszofptr_t cnt, ...);
INTPTR_T callg(callgfnptr, gparam_list *);
#endif
void callg_signal(void *);

#endif
