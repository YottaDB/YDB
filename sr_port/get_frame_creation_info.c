/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "stringpool.h"
#include "error_trap.h"

#define CREATEDBY_DO		0
#define CREATEDBY_XECUTE	1
#define CREATEDBY_FUNCTION	2
#define CREATEDBY_ZINTR		3
#define CREATEDBY_TRIGGER	4

GBLREF	stack_frame		*frame_pointer;
GBLREF	spdesc			stringpool;

#ifdef UNIX
LITDEF 	mstr	createdby_text[5] = {{0, LEN_AND_LIT("DO")}, {0, LEN_AND_LIT("XECUTE")}, {0, LEN_AND_LIT("$$")},
				     {0, LEN_AND_LIT("ZINTR")}, {0, LEN_AND_LIT("TRIGGER")}};
#endif

#ifdef VMS
LITDEF 	mstr	createdby_text[4] = {{LEN_AND_LIT("DO")}, {LEN_AND_LIT("XECUTE")}, {LEN_AND_LIT("$$")},
                                     {LEN_AND_LIT("ZINTR")}};
#endif

void	get_frame_creation_info(int level, int cur_zlevel, mval *result)
{
	int		count;
	stack_frame	*fp;

	assert(0 < level);
	assert(level < cur_zlevel);
	count = cur_zlevel;
	for (fp = frame_pointer; ; fp = fp->old_frame_pointer)
	{
		if (NULL == fp->old_frame_pointer)
		{
			if (fp->type & SFT_TRIGR)
				/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
				fp = *(stack_frame **)(fp + 1);
			else
			{	/* Something wrong, just return null or assert if debug mode */
				assert(FALSE);
				result->str.len = 0;
				return;
			}
		}
		assert(NULL != fp);
		if (!(fp->type & SFT_COUNT))
			continue;
		count--;
		if (count == level)
			break;
	}
	assert(fp && (fp->type & SFT_COUNT));
	if (fp && (fp->type & SFT_ZINTR))
		result->str = createdby_text[CREATEDBY_ZINTR];
	else if (fp && (fp->flags & SFF_INDCE))
		result->str = createdby_text[CREATEDBY_XECUTE];
#	ifdef GTM_TRIGGER
	else if (fp && (fp->old_frame_pointer->type & SFT_TRIGR))
		result->str = createdby_text[CREATEDBY_TRIGGER];
#	endif
	else if (fp && fp->ret_value)
		result->str = createdby_text[CREATEDBY_FUNCTION];
	else
		result->str = createdby_text[CREATEDBY_DO];
}
