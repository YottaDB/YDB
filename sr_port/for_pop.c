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


void	for_pop()
{
	--for_stack_ptr;

	assert(for_stack_ptr >= for_stack);
	assert(for_stack_ptr < &for_stack[MAX_FOR_STACK]);

	return;
}
