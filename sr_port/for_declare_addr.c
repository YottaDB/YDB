/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

GBLDEF	oprtype		*for_stack[MAX_FOR_STACK], **for_stack_ptr;
GBLDEF	bool		for_temps[MAX_FOR_STACK];

void	for_declare_addr(oprtype x)
{
	assert(for_stack_ptr >= for_stack);
	assert(for_stack_ptr < ARRAYTOP(for_stack));
	if (*for_stack_ptr == 0)
		*for_stack_ptr = (oprtype *) mcalloc(SIZEOF(oprtype));
	**for_stack_ptr = x;
	return;
}
