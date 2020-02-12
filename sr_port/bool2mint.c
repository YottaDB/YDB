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

#include "compiler.h"
#include "bool2mint.h"
#include "bool_zysqlnull.h"
#include "bool_finish.h"

int	bool2mint(int src, int this_bool_depth)
{
	assert(this_bool_depth);
	if (INIT_GBL_BOOL_DEPTH != this_bool_depth)
	{
		if (boolZysqlnull->zysqlnull_seen && (this_bool_depth < boolZysqlnull->alloc_depth))
		{
			if (boolZysqlnull->cur_depth > this_bool_depth)
			{
				bool_zysqlnull_depth_check(this_bool_depth, INHERIT_TRUE);
				boolZysqlnull->cur_depth = this_bool_depth;
			}
			if (boolZysqlnull->array[this_bool_depth].is_zysqlnull)
			{	/* The result of the boolean is $ZYSQLNULL. Cannot convert that into an integer. Issue error.
				 * But before that, now that the result of this boolean expression is going to be used for the return,
				 * clear the boolean result global variable for future boolean expressions.
				 */
				boolZysqlnull->array[this_bool_depth].is_zysqlnull = FALSE;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYSQLNULLNOTVALID);
			}
		}
		bool_finish();
	}
	return src;
}
