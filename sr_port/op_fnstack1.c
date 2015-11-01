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
#include "matchc.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "mvalconv.h"
#include "op.h"
#include "dollar_zlevel.h"
#include "gtm_caseconv.h"
#include "gtmmsg.h"
#include "mv_stent.h"
#include "error_trap.h"

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

GBLREF	ecode_list	*dollar_ecode_list;
GBLREF	stack_frame	*frame_pointer;
GBLREF	spdesc		stringpool;
GBLREF	int		error_level;
GBLREF	mv_stent	*mv_chain;
#define HOW_DO		0
#define HOW_XECUTE	1
#define HOW_FUNCTION	2

void op_fnstack1(int level, mval *result)
{
	int		count;
 	int		current;
	int		how;
	int		len;
	mval		mcode = DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT | MV_NUM_APPROX, 0, 0, 5, "MCODE", 0, 0);
	stack_frame	*fp, *previous;
	mv_stent	*mvc;
	mstr		how_text[3] = {2, "DO", 6, "XECUTE", 2, "$$"};
	mstr		direct = {11, "Direct Mode"};
	error_def(ERR_TEXT);

	result->mvtype = MV_STR;
	current = dollar_zlevel();
	if (-1 == level)
	{	/*
	 	 * If level == -1, the function returns the value of the highest
		 * level that contains valid information
		 *
		 * Normally, $STack(-1) would be equal to $ZLevel,
		 * within an error trap routine, $STack(-1) should be
		 * equal to the level where the error happened,
		 * even though $ZLevel could be a higher number at that time.
		 */
		if (dollar_ecode_list->previous && (error_level > current))
			current = error_level;
		MV_FORCE_MVAL(result, current);
	} else if (0 == level)
	{
		/* $STack(0) is an implementation-specific value
		 * that indicates how the process was started
		 */
		op_fnstack2(0, &mcode, result);
		if (0 == result->str.len)
		{
			result->str.len = direct.len;
			result->str.addr = direct.addr;
		}
	} else if (level <= (dollar_ecode_list->previous ? error_level : dollar_zlevel()))
	{
		/* If level is a valid positive integer,
		 * there is actual information to be returned:
		 * a code for how this level was reached.
		 */
		how = HOW_DO;
	        for (count = current, previous = NULL, fp = frame_pointer;
			(fp->old_frame_pointer) && (HOW_DO == how) && (count >= level);
			fp = fp->old_frame_pointer)
	        {
			if (fp->type & SFT_COUNT)
				count--;
			if ((count == level) && (fp->flags & SFF_INDCE))
			{
				how = HOW_XECUTE;
				break;
			}
			for (mvc = mv_chain; ; mvc = (mv_stent *) (mvc->mv_st_next + (char *) mvc))
			{
				if ((MVST_PARM == mvc->mv_st_type) &&
					((mv_stent *)previous < mvc) && (mvc < (mv_stent *)fp) &&
					(mvc->mv_st_cont.mvs_parm.ret_value))
				{
					how = HOW_FUNCTION;
					break;
				}
				if (!mvc->mv_st_next)
					break;
			}
			previous = fp;
	        }
		result->str.len = how_text[how].len;
	        result->str.addr = (char *)stringpool.free;
		s2pool(&how_text[how]);
	} else
	{
		result->str.len = 0;
		result->str.addr = NULL;
	}
}
