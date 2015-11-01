/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "mvalconv.h"
#include "op.h"
#include "dollar_zlevel.h"
#include "mv_stent.h"
#include "error_trap.h"
#include "gtm_caseconv.h"

/* -----------------------------------------------------------------------------------
 * op_fnstack2()
 *
 * MUMPS Stack function (with 2 parameters)
 *
 * Parameters:
 *	level	- Integer containing level counter
 *	info	- Pointer to mval containing one of "PLACE", "MCODE" or "ECODE"
 *      result	- Pointer to mval containing the requested information
 * -----------------------------------------------------------------------------------
 */

GBLREF	stack_frame		*error_frame;
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	dollar_stack_type	dollar_stack;			/* structure containing $STACK related information */

#define	GET_FRAME_INFO(level, mode, cur_zlevel, result)					\
{											\
	assert(level < cur_zlevel);							\
	switch(mode)									\
	{										\
		case DOLLAR_STACK_ECODE:						\
			result->str.len = 0;						\
			break;								\
		case DOLLAR_STACK_PLACE:						\
		case DOLLAR_STACK_MCODE:						\
			get_frame_place_mcode(level, mode, cur_zlevel, result);		\
			break;								\
		default:								\
			GTMASSERT;							\
	}										\
}

void op_fnstack2(int level, mval *info, mval *result)
{
 	int		cur_zlevel;
	int		mode;
 	unsigned char	info_upper[sizeof("MCODE")];

	error_def(ERR_INVSTACODE);

	result->mvtype = MV_STR;
	mode = DOLLAR_STACK_INVALID;
	/* make sure that info is one of the three valid strings */
	if (info->str.len == 5)
	{
		lower_to_upper(info_upper, (unsigned char *)info->str.addr, 5);
		if (!memcmp("MCODE", info_upper, sizeof("MCODE")-1))
			mode = DOLLAR_STACK_MCODE;
		else if (!memcmp("ECODE", info_upper, sizeof("ECODE")-1))
			mode = DOLLAR_STACK_ECODE;
		else if (!memcmp("PLACE", info_upper, sizeof("PLACE")-1))
			mode = DOLLAR_STACK_PLACE;
	}
	if (DOLLAR_STACK_INVALID == mode)
		rts_error(VARLSTCNT(4) ERR_INVSTACODE, 2, info->str.len, info->str.addr);
	cur_zlevel = dollar_zlevel();
	if (0 > level)
		result->str.len = 0;
	else if (!dollar_stack.index)
	{
		if (level < cur_zlevel)
		{
			GET_FRAME_INFO(level, mode, cur_zlevel, result);
		} else
			result->str.len = 0;
	} else if (level < dollar_stack.index)
		get_dollar_stack_info(level, mode, result);
	else if (!dollar_stack.incomplete && (1 == dollar_ecode.index)
			&& (error_frame == dollar_ecode.first_ecode_error_frame) && (level < cur_zlevel))
	{
		GET_FRAME_INFO(level, mode, cur_zlevel, result);
	} else
		result->str.len = 0;
	return;
}
