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
#include "get_command_line.h"
#include "gtmmsg.h"
#include "error_trap.h"
#include "get_mumps_code.h"

/*
 * -----------------------------------------------
 * op_fnstack2()
 *
 * MUMPS Stack function (with 2 parameters)
 *
 * Parameters:
 *	level	- Integer containing level counter
 *	info	- Pointer to mval containing one of "PLACE", "MCODE" or "ECODE"
 *      result	- Pointer to mval containing the requested information
 * -----------------------------------------------
 */

GBLREF	ecode_list	*dollar_ecode_list;
GBLREF	mval		dollar_zmode;
GBLREF	stack_frame	*frame_pointer;
GBLREF	spdesc		stringpool;
GBLREF	int		error_level;
#define INFO_INVALID -1
#define INFO_MCODE 1
#define INFO_ECODE 2
#define INFO_PLACE 3

void op_fnstack2(int level, mval *info, mval *result)
{
 	unsigned char	info_upper[sizeof("MCODE")];
 	int		current;
	int		max;
	int		type;
	stack_frame	*fp;
	error_def(ERR_TEXT);
	error_def(ERR_INVSTACODE);

	result->mvtype = MV_STR;
	type = INFO_INVALID;
	/* make sure that info is one of the three valid strings */
	if (info->str.len == 5)
	{
		lower_to_upper(info_upper, (unsigned char *) info->str.addr, 5);
		if (!memcmp("MCODE", info_upper, sizeof("MCODE")-1))
			type = INFO_MCODE;
		else if (!memcmp("ECODE", info_upper, sizeof("ECODE")-1))
			type = INFO_ECODE;
		else if (!memcmp("PLACE", info_upper, sizeof("PLACE")-1))
			type = INFO_PLACE;
	}
	if (INFO_INVALID == type)
		rts_error(VARLSTCNT(4) ERR_INVSTACODE, 2, info->str.len, info->str.addr);
	max = dollar_zlevel();
	if (dollar_ecode_list->previous && (error_level > max))
		max = error_level;
	if ((0 < level) && (level <= max))
	{
		current = dollar_zlevel() + 1;
		for (fp = frame_pointer; fp->old_frame_pointer; fp = fp->old_frame_pointer)
		{
			if (fp->type & SFT_COUNT)
			{
				current--;
				if (current == level)
				{
					switch (type)
					{
					case INFO_MCODE:
						get_mumps_code(fp, NULL, result);
						break;
					case INFO_ECODE:
						/* This option returns the error code
						 * at the level requested.
						 */
						dollar_ecode_build(level, result);
						break;
					case INFO_PLACE:
						get_mumps_code(fp, result, NULL);
						break;
					}
					break;
				}
			}
		}
	} else if (level == 0)
	{
		switch (type)
		{
		case INFO_MCODE:
			get_command_line(result);
			break;
		case INFO_ECODE:
			result->str.len = 0;
			result->str.addr = NULL;
			break;
		case INFO_PLACE:
			result->str = dollar_zmode.str;
			break;
		}
	} else
	{
		/* Negative values are "reserved"
		 * too large values are defined to cause empty string
		 */
		result->str.len = 0;
		result->str.addr = NULL;
	}
}
