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
#include "bool2mval.h"
#include "mvalconv.h"
#include "bool_zysqlnull.h"
#include "bool_finish.h"

LITREF mval		literal_sqlnull;

void	bool2mval(int src, int this_bool_depth, mval *ret)
{
	/* Note: If "this_bool_depth" is not INIT_GBL_BOOL_DEPTH, it is possible that it is GREATER THAN OR EQUAL TO
	 * boolZysqlnull->alloc_depth, e.g. `set x='$test` where the boolean expression only involved checking for
	 * `$test` and no other OC_COBOOL etc. which would have invoked `bool_zysqlnull()` to do the allocation.
	 * Hence the `if` check below instead of an assert that is usually done in other places.
	 */
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
			{	/* The result of the boolean is $ZYSQLNULL. Return that. */
				*ret = literal_sqlnull;
				/* Now that the result of this boolean expression has been returned,
				 * clear the boolean result global variable for future boolean expressions.
				 */
				bool_finish();
				return;
			}
		}
		bool_finish();
	}
	i2mval(ret, src);
	return;
}
