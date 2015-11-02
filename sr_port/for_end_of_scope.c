/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

oprtype for_end_of_scope(int depth)
{
	oprtype		**ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TREF(for_stack_ptr) >= (oprtype **)TADR(for_stack));
	assert(TREF(for_stack_ptr) < (oprtype **)(TADR(for_stack) + ggl_for_stack));
	ptr = (oprtype **)TREF(for_stack_ptr) - depth;
	assert(ptr >= (oprtype **)TADR(for_stack));
	if (NULL == *ptr)
		*ptr = (oprtype *)mcalloc(SIZEOF(oprtype));
	return put_indr(*ptr);
}
