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

#include "mdef.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "mvalconv.h"
#include "op.h"
#include "mv_stent.h"
#include "error_trap.h"

#include "dollar_zlevel.h"
#include "get_command_line.h"

/*
 * -----------------------------------------------
 * op_fnstack1()
 *
 * MUMPS Stack function (with 1 parameter)
 *
 * Arguments:
 *	level	- Integer containing level counter
 *      result	- Pointer to mval containing the requested information
 * -----------------------------------------------
 */

GBLREF	stack_frame		*error_frame;
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	dollar_stack_type	dollar_stack;			/* structure containing $STACK related information */

void op_fnstack1(int level, mval *result)
{
 	int		cur_zlevel;

	result->mvtype = MV_STR;
	cur_zlevel = dollar_zlevel();

	if (-1 == level)
	{
		if (!dollar_ecode.index)
		{
			MV_FORCE_MVAL(result, cur_zlevel - 1);
		} else if ((1 < dollar_ecode.index) || (error_frame != dollar_ecode.first_ecode_error_frame))
			MV_FORCE_MVAL(result, (int)dollar_stack.index - 1);
		else
		{	/* we are in first ECODE error-handler */
			if (cur_zlevel > dollar_stack.index)
			{
				MV_FORCE_MVAL(result, cur_zlevel - 1);
			} else
			{
				MV_FORCE_MVAL(result, (int)dollar_stack.index - 1);
			}
		}
	} else if (0 > level)
		result->str.len = 0;
	else if (0 == level)
		get_command_line(result, FALSE);	/* FALSE to indicate we want actual (not processed) command line */
	else if (!dollar_stack.index)
	{
		if (level < cur_zlevel)
			get_frame_creation_info(level, cur_zlevel, result);
		else
			result->str.len = 0;
	} else if (level < dollar_stack.index)
		get_dollar_stack_info(level, DOLLAR_STACK_MODE, result);
	else if (!dollar_stack.incomplete && (1 == dollar_ecode.index)
			&& (error_frame == dollar_ecode.first_ecode_error_frame) && (level < cur_zlevel))
		get_frame_creation_info(level, cur_zlevel, result);
	else
		result->str.len = 0;
	return;
}
