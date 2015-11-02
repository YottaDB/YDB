/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

GBLREF	spdesc			stringpool;
GBLREF	dollar_stack_type	dollar_stack;			/* structure containing $STACK related information */

void	get_dollar_stack_info(int level, stack_mode_t mode,  mval *result)
{
	assert(0 <= level);
	assert(level < dollar_stack.index);
	switch(mode)
	{
		case DOLLAR_STACK_MODE:
			result->str = dollar_stack.array[level].mode_str;
			break;
		case DOLLAR_STACK_MCODE:
			result->str = dollar_stack.array[level].mcode_str;
			break;
		case DOLLAR_STACK_PLACE:
			result->str = dollar_stack.array[level].place_str;
			break;
		case DOLLAR_STACK_ECODE:
			if (NULL != dollar_stack.array[level].ecode_ptr)
				result->str = dollar_stack.array[level].ecode_ptr->ecode_str;
			else
			{
				result->str.len = 0;
				return;
			}
			break;
		default:
			GTMASSERT;
	}
	s2pool(&result->str);
	assert(!result->str.len || ((unsigned char *)result->str.addr + result->str.len) == stringpool.free);
	return;
}
