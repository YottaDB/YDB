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
#include "bool_andor.h"
#include "bool_zysqlnull.h"

void	bool_andor(uint4 combined_opcode)
{
	bool_sqlnull_t		*bool_ptr, *bool_ptr2;
	opctype			andor_opcode;
	int			depth;
	int			operand_index;
#	ifdef DEBUG
	opctype			not_of_andor_opcode;
#	endif

	assert(NULL != boolZysqlnull);
	assert(boolZysqlnull->booleval_in_prog);
	assert(boolZysqlnull->frame_pointer == frame_pointer);
	if (!boolZysqlnull->zysqlnull_seen)
		return;
	SPLIT_ANDOR_OPCODE_DEPTH_OPRINDX(combined_opcode, andor_opcode, depth, operand_index);
	assert(INIT_GBL_BOOL_DEPTH != depth);
	assert(2 == NUM_TRIPLE_OPERANDS);
	assert((0 == operand_index) || (1 == operand_index));
	if (0 == operand_index)
	{
		if ((boolZysqlnull->cur_depth >= depth) && (OC_NOOP != boolZysqlnull->array[depth].andor_opcode))
			bool_zysqlnull_depth_check(depth - 1, INHERIT_TRUE);
		bool_zysqlnull(depth);		/* can update "boolZysqlnull->array[depth].is_zysqlnull" */
		bool_ptr = &boolZysqlnull->array[depth];
		assert(depth < boolZysqlnull->alloc_depth);
		bool_ptr->andor_opcode = andor_opcode;
		bool_ptr->result = BOOL_RESULT_UNINITIALIZED;	/* Clear values from any prior evaluations */
#		ifdef DEBUG
		switch(andor_opcode)
		{
		case OC_OR:
		case OC_NOR:
		case OC_AND:
		case OC_NAND:
			break;
		default:
			assert(FALSE);
			break;
		}
#		endif
	} else
	{
		assert(OC_NOOP != andor_opcode);
		assert(depth < boolZysqlnull->alloc_depth);
		/* If "boolZysqlnull->zysqlnull_seen" was FALSE during the invocation of `bool_andor` for `0 == operand_index`,
		 * but became TRUE somewhere in the middle, we would not have recorded the `andor_opcode` in the bool array
		 * (in which case it would be OC_NOOP (asserted below). Fix that here by initializing it (just like is done
		 * in the `0 == operand_index` case).
		 */
		bool_ptr = &boolZysqlnull->array[depth];
#		ifdef DEBUG
		not_of_andor_opcode = andor_opcode;
		LOGICAL_NOT(not_of_andor_opcode);
		assert((bool_ptr->andor_opcode == andor_opcode)
			|| (bool_ptr->andor_opcode == not_of_andor_opcode) || (OC_NOOP == bool_ptr->andor_opcode));
#		endif
		bool_ptr->andor_opcode = andor_opcode;
		if (boolZysqlnull->cur_depth > depth)
		{
			if (boolZysqlnull->cur_depth > (depth + 1))
				bool_zysqlnull_depth_check(depth + 1, INHERIT_TRUE);
			/* Copy over result of left hand side of evaluation of OC_AND/OC_OR etc. into corresponding depth */
			bool_ptr2 = bool_ptr + 1;
			bool_ptr->is_zysqlnull = bool_ptr2->is_zysqlnull;
			bool_ptr->result = bool_ptr2->result;
			bool_ptr2->is_zysqlnull = FALSE;
		}
	}
	boolZysqlnull->cur_depth = depth;
	return;
}
