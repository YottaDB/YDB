/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#ifdef DEBUG
#include "gtm_stdlib.h"
#endif

#include "compiler.h"
#include "bool_init.h"
#include "bool_zysqlnull.h"
#include "stack_frame.h"

void	bool_init(void)
{
	boolZysqlnullArray_t	*tmpZysqlnullArray;
#	ifdef DEBUG
	int			max_boolexpr_nesting_depth;
	char			*ydb_max_boolexpr_nesting_depth;
#	endif

	if ((NULL == boolZysqlnull) || boolZysqlnull->booleval_in_prog)
	{	/* A boolean expression evaluation is already in progress. Create a new structure and point boolZysqlnull to it */
		tmpZysqlnullArray = (NULL != boolZysqlnull) ? boolZysqlnull->next_free : NULL;
		if (NULL == tmpZysqlnullArray)
		{
			tmpZysqlnullArray = malloc(SIZEOF(boolZysqlnullArray_t));
			tmpZysqlnullArray->alloc_depth = 0;
			tmpZysqlnullArray->array = NULL;
			tmpZysqlnullArray->next_free = NULL;
		}
		tmpZysqlnullArray->cur_depth = INIT_GBL_BOOL_DEPTH;
		tmpZysqlnullArray->previous = boolZysqlnull;
#		ifdef DEBUG
		tmpZysqlnullArray->boolexpr_nesting_depth = (NULL == boolZysqlnull) ? 0 : boolZysqlnull->boolexpr_nesting_depth + 1;
#		endif
		boolZysqlnull = tmpZysqlnullArray;
	}
#	ifdef DEBUG
	else
	{
		boolZysqlnull->cur_depth = INIT_GBL_BOOL_DEPTH;
	}
	/* Assert that current boolexpr nesting depth is not too much. This was useful to have in the early stages of
	 * development when due to a bug, the nesting depth was getting bumped up uncontrollably. Having an alert about
	 * the depth when it goes to a high value would have caught that bug. Hence this check in debug code.
	 * 4 is considered a good enough max depth by default.
	 * There are some tests in the YDBTest repo that encounter errors in boolean expressions and continue on to executing
	 * more such cases before returning from that M frame so those need a higher depth. They set an env var accordingly
	 * so check that dbg-only env var if it is set. If not use 4.
	 */
	ydb_max_boolexpr_nesting_depth = getenv("ydb_max_boolexpr_nesting_depth");
	max_boolexpr_nesting_depth = (NULL != ydb_max_boolexpr_nesting_depth) ? atoi(ydb_max_boolexpr_nesting_depth) : 4;
	assert(max_boolexpr_nesting_depth > boolZysqlnull->boolexpr_nesting_depth);
#	endif
	boolZysqlnull->frame_pointer = frame_pointer;
	boolZysqlnull->booleval_in_prog = TRUE;
	boolZysqlnull->zysqlnull_seen = FALSE;
	return;
}
