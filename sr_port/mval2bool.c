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
#include "mval2bool.h"
#include "bool_zysqlnull.h"

/* Returns a value `bool_result` that is set to the result of the boolean expression evaluation till this point */
boolean_t mval2bool(mval *src, int this_bool_depth, uint4 combined_opcode)
{
	boolean_t	bool_result, skip_bool_zysqlnull;

	skip_bool_zysqlnull = (INIT_GBL_BOOL_DEPTH == this_bool_depth);
	if (!skip_bool_zysqlnull && MV_IS_SQLNULL(src))
	{	/* The operand is $ZYSQLNULL. Return value is not deterministic.
		 * Do $ZYSQLNULL related processing.
		 */
		bool_result = bxoprnd_is_zysqlnull(this_bool_depth, combined_opcode, TRUE);
	} else
	{
		MV_FORCE_NUM(src);
		bool_result = (0 != src->m[1]);
		if (!skip_bool_zysqlnull && boolZysqlnull->zysqlnull_seen)
		{	/* The operand is not $ZYSQLNULL but we are dealing with a boolean expression that has already seen
			 * a $ZYSQLNULL. Do processing related to that.
			 */
			bool_result = bxoprnd_is_not_zysqlnull(this_bool_depth, combined_opcode, bool_result, bool_result, TRUE);
		}
	}
	return bool_result;
}
