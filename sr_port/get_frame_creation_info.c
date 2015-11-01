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

#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "error_trap.h"

#define CREATEDBY_DO		0
#define CREATEDBY_XECUTE	1
#define CREATEDBY_FUNCTION	2

GBLREF	stack_frame		*frame_pointer;
GBLREF	spdesc			stringpool;
GBLREF	mv_stent		*mv_chain;

void	get_frame_creation_info(int level, int cur_zlevel, mval *result)
{
	mstr		createdby_text[3] = {LEN_AND_LIT("DO"), LEN_AND_LIT("XECUTE"), LEN_AND_LIT("$$")};
	int		count;
	stack_frame	*fp, *previous;
	mv_stent	*mvc;

	assert(0 < level);
	assert(level < cur_zlevel);
	count = cur_zlevel;
	for (previous = NULL, fp = frame_pointer;  ; previous = fp, fp = fp->old_frame_pointer)
	{
		if (NULL == fp->old_frame_pointer)
		{
			assert(FALSE);
			result->str.len = 0;
			return;
		}
		if (!(fp->type & SFT_COUNT))
			continue;
		count--;
		if (count == level)
			break;
	}
	if (fp->flags & SFF_INDCE)
		result->str = createdby_text[CREATEDBY_XECUTE];
	else
	{
		for (mvc = mv_chain; ; mvc = (mv_stent *) (mvc->mv_st_next + (char *) mvc))
		{
			if ((mvc >= (mv_stent *)fp) || (!mvc->mv_st_next))
			{
				assert(mvc->mv_st_next);
				result->str = createdby_text[CREATEDBY_DO];
				break;
			}
			if (mvc <= (mv_stent *)previous)
				continue;
			if ((MVST_PARM == mvc->mv_st_type) && mvc->mv_st_cont.mvs_parm.ret_value)
			{
				assert(mvc->mv_st_next);
				result->str = createdby_text[CREATEDBY_FUNCTION];
				break;
			}
		}
	}
	s2pool(&result->str);
	assert(((unsigned char *)result->str.addr + result->str.len) == stringpool.free);
}
