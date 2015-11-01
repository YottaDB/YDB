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

GBLREF	oprtype		*for_stack[MAX_FOR_STACK],
			**for_stack_ptr;
GBLREF	bool		for_temps[MAX_FOR_STACK];


int	for_push(void)
{
	unsigned short	level;
	error_def(ERR_FOROFLOW);


	if (++for_stack_ptr >= &for_stack[MAX_FOR_STACK])
	{
		stx_error(ERR_FOROFLOW);
		return FALSE;
	}

	assert(for_stack_ptr >= for_stack);
	assert(for_stack_ptr < &for_stack[MAX_FOR_STACK]);

	*for_stack_ptr = 0;
	level = for_stack_ptr - for_stack;
	for_temps[level] = for_temps[level - 1];

	return TRUE;
}
