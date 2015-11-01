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
#include "mmemory.h"

GBLREF	oprtype		*for_stack[MAX_FOR_STACK],
			**for_stack_ptr;


oprtype for_end_of_scope(int depth)
{
	oprtype		**ptr;


	assert(for_stack_ptr >= for_stack);
	assert(for_stack_ptr < &for_stack[MAX_FOR_STACK]);

	ptr = for_stack_ptr - depth;
	assert(ptr >= for_stack);

	if (*ptr == 0)
		*ptr = (oprtype *) mcalloc(sizeof(oprtype));

	return put_indr(*ptr);
}
