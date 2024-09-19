/****************************************************************
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "op.h"
#include "bool_zysqlnull.h"
#include "is_equ.h"

int	op_bxrelop_equ(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode)
{
	int		result;
	int		numpcs;
	uint4		tempuint;
	boolean_t	bool_result, ok_to_inherit, invert;
	int		jmp_depth;
	opctype		andor_opcode, jmp_opcode;
	bool_sqlnull_t	*bool_ptr, *bool_ptr2;

	assert(INIT_GBL_BOOL_DEPTH != this_bool_depth);
	if (MV_IS_SQLNULL(lhs) || MV_IS_SQLNULL(rhs))
	{	/* At least one of the operands is $ZYSQLNULL. Return value is not deterministic.
		 * Do $ZYSQLNULL related processing.
		 */
		MV_FORCE_DEFINED(lhs);
		MV_FORCE_DEFINED(rhs);
		result = bxoprnd_is_zysqlnull(this_bool_depth, combined_opcode, FALSE);
		return result;
	}
	result = is_equ(lhs, rhs);
	bool_result = result;
	if (boolZysqlnull->zysqlnull_seen)
	{	/* The operand is not $ZYSQLNULL but we are dealing with a boolean expression that has already seen
		 * a $ZYSQLNULL. Do processing related to that.
		 */
		result = bxoprnd_is_not_zysqlnull(this_bool_depth, combined_opcode, result, bool_result, FALSE);
	}
	return result;
}

