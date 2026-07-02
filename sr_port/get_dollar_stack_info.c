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

#include "mdef.h"

#include "stringpool.h"
#include "error_trap.h"
#include "gcol_list.h"

GBLREF	spdesc			stringpool;
GBLREF	dollar_stack_type	dollar_stack;			/* structure containing $STACK related information */

void	get_dollar_stack_info(int level, stack_mode_t mode,  mval *result)
{
	assert(0 <= level);
	assert(level < dollar_stack.index);
	switch (mode)
	{
		case DOLLAR_STACK_MODE:
			result->str.umstr = dollar_stack.array[level].mode_str.umstr;
			break;
		case DOLLAR_STACK_MCODE:
			result->str.umstr = dollar_stack.array[level].mcode_str.umstr;
			break;
		case DOLLAR_STACK_PLACE:
			result->str.umstr = dollar_stack.array[level].place_str.umstr;
			break;
		case DOLLAR_STACK_ECODE:
			if (NULL != dollar_stack.array[level].ecode_ptr)
				result->str.umstr = dollar_stack.array[level].ecode_ptr->ecode_str.umstr;
			else
			{
				result->str.len = 0;
				return;
			}
			break;
		default:
			assertpro(FALSE && mode);
	}
	s2pool(&result->str);
	assert(!result->str.len || IS_AT_END_OF_STRINGPOOL(result->str.addr, result->str.len));
	return;
}
