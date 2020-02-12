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
#include "bxrelop_operator.h"
#include "matchc.h"
#include "sorts_after.h"
#include "patcode.h"
#include "mmemory.h"
#include "numcmp.h"
#include "is_equ.h"
#include "bool_zysqlnull.h"

/* Returns the result of the Boolean eXpression RELational OPerator (hence the BXRELOP prefix in the function name) */
int	bxrelop_operator(mval *lhs, mval *rhs, opctype relopcode, int this_bool_depth, uint4 combined_opcode)
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
	if ((OC_EQU == relopcode) || (OC_NEQU == relopcode))
	{	/* Most common code path hence keep it as first check separate from the switch/case below */
		result = is_equ(lhs, rhs);
		bool_result = (OC_EQU == relopcode) ? result : !result;
	} else
	{
		switch(relopcode)
		{
		case OC_NCONTAIN:
		case OC_CONTAIN:
			MV_FORCE_STR(lhs);
			MV_FORCE_STR(rhs);
			numpcs = 1;
			matchc(rhs->str.len, (uchar_ptr_t)rhs->str.addr, lhs->str.len, (uchar_ptr_t)lhs->str.addr,
													&result, &numpcs);
			bool_result = (OC_CONTAIN == relopcode) ? (0 < result) : !result;
			break;
		case OC_NSORTS_AFTER:
		case OC_SORTS_AFTER:
			result = sorts_after(lhs, rhs);
			bool_result = (OC_SORTS_AFTER == relopcode) ? (0 < result) : (0 >= result);
			break;
		case OC_NPATTERN:
		case OC_PATTERN:
			GET_ULONG(tempuint, rhs->str.addr);
			result = (tempuint ? do_patfixed(lhs, rhs) : do_pattern(lhs, rhs));
			bool_result = (OC_PATTERN == relopcode) ? result : !result;
			break;
		case OC_NFOLLOW:
		case OC_FOLLOW:
			MV_FORCE_STR(lhs);
			MV_FORCE_STR(rhs);
			result = memvcmp(lhs->str.addr, lhs->str.len, rhs->str.addr, rhs->str.len);
			bool_result = (OC_FOLLOW == relopcode) ? (0 < result) : (0 >= result);
			break;
		case OC_NGT:
		case OC_GT:
			result = numcmp(lhs, rhs);
			bool_result = (OC_GT == relopcode) ? (0 < result) : (0 >= result);
			break;
		default:
			assert((OC_LT == relopcode) || (OC_NLT == relopcode));
			result = numcmp(lhs, rhs);
			bool_result = (OC_LT == relopcode) ? (0 > result) : (0 <= result);
			break;
		}
	}
	if (boolZysqlnull->zysqlnull_seen)
	{	/* The operand is not $ZYSQLNULL but we are dealing with a boolean expression that has already seen
		 * a $ZYSQLNULL. Do processing related to that.
		 */
		result = bxoprnd_is_not_zysqlnull(this_bool_depth, combined_opcode, result, bool_result, FALSE);
	}
	return result;
}
