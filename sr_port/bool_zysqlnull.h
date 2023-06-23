/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef BOOL_ZYSQLNULL_INCLUDED
#define BOOL_ZYSQLNULL_INCLUDED

typedef struct bool_sqlnull_t {
	boolean_t	is_zysqlnull;
	boolean_t	result;
	opctype		andor_opcode;
} bool_sqlnull_t;

typedef struct boolZysqlnullArray_t {
	int				cur_depth; /* The boolean expression evaluation depth (AND/OR increments it by 1) */
	int				alloc_depth; /* The depth up to which `*array` has been allocted */
	bool_sqlnull_t			*array; /* An array that records whether $ZYSQLNULL was seen at every boolean expression
						 * evaluation depth. Used to come up with the final tri-state result of the
						 * evaluation (FALSE/TRUE/$ZYSQLNULL). The 3rd state indicates UNKNOWN and is
						 * used by Octo to return results that are correct in SQL-land.
						 */
	struct stack_frame_struct	*frame_pointer;	/* Pointer to M frame where this boolean expression is being evaluated.
							 * This is needed to take into account where a boolean expression operand
							 * involves a function call that in turn has its own boolean expression.
							 */
	struct boolZysqlnullArray_t	*previous;	/* Pointer to similar structure corresponding to previous M frame */
	struct boolZysqlnullArray_t	*next_free;	/* Pointer to free list (used first for allocations) */
	boolean_t			booleval_in_prog;	/* A boolean evaluation is in progress */
	boolean_t			zysqlnull_seen;		/* If FALSE (i.e. $ZYSQLNULL was never seen till now in the
								 * boolean expression evaluation), a lot of boolean state
								 * maintenance is skipped to keep the evaluation as lightweight
								 * as possible (like it used to be in r1.28).
								 * Note: This field is usable only when `booleval_in_prog` is TRUE.
								 */
#	ifdef DEBUG
	int				boolexpr_nesting_depth;	/* debug field to ensure we do not go too deep */
#	endif
} boolZysqlnullArray_t;

#define	IS_ANDOR_OPCODE(OPCODE)	((OC_OR == OPCODE) || (OC_NOR == OPCODE)		\
					|| (OC_AND == OPCODE) || (OC_NAND == OPCODE)	\
					|| (OC_NOOP == OPCODE))

#ifdef DEBUG
# define	DBG_CHECK_ANDOR_OPCODE_COMPATIBLE(OPCODE1, OPCODE2)		\
{										\
	switch(OPCODE1)								\
	{									\
		case OC_OR:							\
		case OC_NOR:							\
			assert((OC_OR == OPCODE2) || (OC_NOR == OPCODE2));	\
			break;							\
		case OC_AND:							\
		case OC_NAND:							\
			assert((OC_AND == OPCODE2) || (OC_NAND == OPCODE2));	\
			break;							\
		default:							\
			assert(FALSE);						\
			break;							\
	}									\
}
#else
# define	DBG_CHECK_ANDOR_OPCODE_COMPATIBLE(OPCODE1, OPCODE)
#endif

#define	BOOL_RESULT_UNINITIALIZED	-1

#include "stack_frame.h"

GBLREF	boolZysqlnullArray_t	*boolZysqlnull;
GBLREF	stack_frame		*frame_pointer;
GBLREF	bool_sqlnull_t		*boolZysqlnullCopy;
GBLREF	int			boolZysqlnullCopyAllocLen;

/* Below are various helper functions used by the boolean evaluation logic when a $ZYSQLNULL is encountered.
 * All of them are declared as inline functions. Some of them were previously standalone functions while some
 * were macros. The standalone functions gained a lot performance wise (a simple benchmark showed 20% improvement
 * when $ZYSQLNULL was involved in the boolean expression) when they were moved to inline functions. Those are
 *	1) bool_zysqlnull()
 *	2) bool_zysqlnull_depth_check()
 *	3) bxoprnd_is_zysqlnull()
 *	4) bxoprnd_is_not_zysqlnull()
 *	to help with debugging.
 * The rest of the inline functions below were all previously macros. A small slowdown was noticed with converting
 * them to inline functions (0.4%) but is considered negligible enough that we went ahead with the no-macro
 * approach as this is a lot more easy to maintain/debug.
 */
static inline void set_is_zysqlnull_true(bool_sqlnull_t *bool_ptr)
{
	bool_ptr->is_zysqlnull = TRUE;
	/* Note that `BOOL_PTR->result` can be BOOL_RESULT_UNINITIALIZED at this point if
	 * $ZYSQLNULL was encountered in the middle of a deep boolean expression.
	 * But now that we have found that the result of the evaluations is $ZYSQLNULL,
	 * reset `result` in case it is used later (e.g. it is left side of an ! or & operator).
	 */
	bool_ptr->result = 0;
}

#include "gtm_string.h"

#define	INHERIT_FALSE	FALSE
#define	INHERIT_TRUE	TRUE

/* Check if is_zysqlnull needs to be inherited from any deeper boolean levels.
 * `new_depth` is the depth at which we should stop inheriting.
 * If `inherit` is FALSE, we do not inherit but just check if it is okay to inherit and return that result.
 * If `inherit` is TRUE, we inherit and return TRUE.
 */
static inline boolean_t bool_zysqlnull_depth_check(int new_depth, boolean_t inherit)
{
	opctype			andor_opcode;
	bool_sqlnull_t		*ptr1, *ptr2;
	int			depth;

	assert(INIT_GBL_BOOL_DEPTH != new_depth);
	assert(NULL != boolZysqlnull);
	assert(boolZysqlnull->frame_pointer == frame_pointer);
	assert(boolZysqlnull->zysqlnull_seen);
	assert(boolZysqlnull->cur_depth > new_depth);
	assert(boolZysqlnull->cur_depth < boolZysqlnull->alloc_depth);
	depth = boolZysqlnull->cur_depth;
	if (!inherit)
	{
		if (boolZysqlnullCopyAllocLen < (depth - new_depth + 1))
		{
			if (NULL != boolZysqlnullCopy)
				free(boolZysqlnullCopy);
			boolZysqlnullCopyAllocLen = 2 * (depth - new_depth + 1);
			boolZysqlnullCopy = malloc(SIZEOF(bool_sqlnull_t) * boolZysqlnullCopyAllocLen);
		}
		memcpy(boolZysqlnullCopy, &boolZysqlnull->array[new_depth], SIZEOF(bool_sqlnull_t) * (depth - new_depth + 1));
		ptr2 = &boolZysqlnullCopy[depth - new_depth];
	} else
		ptr2 = &boolZysqlnull->array[depth];
	for (depth = boolZysqlnull->cur_depth; depth > new_depth; depth--)
	{
		ptr1 = ptr2 - 1;
		andor_opcode = ptr1->andor_opcode;
		if (OC_NOOP != ptr2->andor_opcode)
		{
			if (ptr2->is_zysqlnull)
			{
				set_is_zysqlnull_true(ptr1);	/* outer level has to inherit $zysqlnull result from inner level */
				if (!inherit)
					return FALSE;
				ptr2->is_zysqlnull = FALSE;
			} else if (ptr1->is_zysqlnull)
			{
				assert(BOOL_RESULT_UNINITIALIZED != ptr2->result);
				switch(andor_opcode)
				{
				case OC_OR:
				case OC_NOR:
					if (ptr2->result)
					{
						ptr1->is_zysqlnull = FALSE;
						ptr1->result = ((OC_OR == andor_opcode) ? TRUE : FALSE);
						ptr1->andor_opcode = OC_OR;
					} else if (!inherit)
						return FALSE;
					break;
				case OC_AND:
				case OC_NAND:
					if (!ptr2->result)
					{
						ptr1->is_zysqlnull = FALSE;
						ptr1->result = ((OC_AND == andor_opcode) ? FALSE : TRUE);
						ptr1->andor_opcode = OC_AND;
					} else if (!inherit)
						return FALSE;
					break;
				case OC_NOOP:
					break;
				default:
					assert(FALSE);
					break;
				}
			} else
			{
#				ifdef DEBUG
				switch(ptr2->andor_opcode)
				{
				case OC_OR:
				case OC_AND:
				case OC_NAND:
				case OC_NOR:
					break;
				default:
					assert(FALSE);
					break;
				}
#				endif
				assert(BOOL_RESULT_UNINITIALIZED != ptr2->result);
				if (BOOL_RESULT_UNINITIALIZED == ptr1->result)
				{
					assert((0 == ptr2->result) || (1 == ptr2->result));
					switch(ptr2->andor_opcode)
					{
					case OC_OR:
					case OC_AND:
						ptr1->result = ptr2->result;
						break;
					case OC_NOR:
					case OC_NAND:
						ptr1->result = !ptr2->result;
						break;
					default:
						assert(FALSE);
						break;
					}
					switch(andor_opcode)
					{
					case OC_NOR:
						ptr1->result = !ptr1->result;
						ptr1->andor_opcode = OC_OR;
						break;
					case OC_NAND:
						ptr1->result = !ptr1->result;
						ptr1->andor_opcode = OC_AND;
						break;
					default:
						break;
					}
				} else
				{
					switch(andor_opcode)
					{
					case OC_OR:
						ptr1->result = ptr1->result || ptr2->result;
						break;
					case OC_AND:
						ptr1->result = ptr1->result && ptr2->result;
						break;
					case OC_NAND:
						ptr1->result = (ptr1->result && ptr2->result) ? 0 : 1;
						ptr1->andor_opcode = OC_AND;
						break;
					case OC_NOR:
						ptr1->result = (ptr1->result || ptr2->result) ? 0 : 1;
						ptr1->andor_opcode = OC_OR;
						break;
					case OC_NOOP:
						break;
					default:
						assert(FALSE);
						break;
					}
				}
			}
		} else if (ptr2->is_zysqlnull)
		{
			set_is_zysqlnull_true(ptr1);	/* outer level has to inherit $zysqlnull result from inner level */
			ptr2->is_zysqlnull = FALSE;
		}
		ptr2--;
	}
	return TRUE;
}

static inline void bool_zysqlnull(int this_bool_depth)
{
	int			depth, new_alloc_depth, cur_depth, alloc_depth;
	bool_sqlnull_t		*tmp_boolZysqlnull;

	if (INIT_GBL_BOOL_DEPTH == this_bool_depth)
		return;	/* this invocation is not inside of a boolean expression evaluation */
	assert(NULL != boolZysqlnull);
	assert(boolZysqlnull->frame_pointer == frame_pointer);
	assert(boolZysqlnull->zysqlnull_seen);
	cur_depth = boolZysqlnull->cur_depth;
	alloc_depth = boolZysqlnull->alloc_depth;
	if (cur_depth > this_bool_depth)
		bool_zysqlnull_depth_check(this_bool_depth, INHERIT_TRUE);
	else if (cur_depth == this_bool_depth)
	{
		assert(cur_depth < alloc_depth);
	} else
	{	/* Case : boolZysqlnull->cur_depth < this_bool_depth
		 * We are descending into a deeper boolean expression depth. Allocate/Initialize deeper levels as needed.
		 */
		if (alloc_depth <= (this_bool_depth + 1))
		{	/* Need to allocate deeper levels first */
			new_alloc_depth = (this_bool_depth + 2) * 2;	/* + 1 needed in case `this_bool_depth` is 0 */
			tmp_boolZysqlnull = malloc(SIZEOF(bool_sqlnull_t) * new_alloc_depth);
			if (alloc_depth)
			{
				assert(NULL != boolZysqlnull->array);
				memcpy(tmp_boolZysqlnull, boolZysqlnull->array,
							SIZEOF(bool_sqlnull_t) * boolZysqlnull->alloc_depth);
				free(boolZysqlnull->array);
			} else
				assert(NULL == boolZysqlnull->array);
			memset(&tmp_boolZysqlnull[alloc_depth], 0, SIZEOF(bool_sqlnull_t) * (new_alloc_depth - alloc_depth));
			boolZysqlnull->alloc_depth = new_alloc_depth;
			boolZysqlnull->array = tmp_boolZysqlnull;
		}
		assert(this_bool_depth < boolZysqlnull->alloc_depth);
		for (depth = cur_depth + 1; depth <= this_bool_depth; depth++)
		{
			boolZysqlnull->array[depth].is_zysqlnull = FALSE;
			boolZysqlnull->array[depth].result = BOOL_RESULT_UNINITIALIZED;
			boolZysqlnull->array[depth].andor_opcode = OC_NOOP;
		}
	}
	boolZysqlnull->cur_depth = this_bool_depth;
}

static inline void bool_zysqlnull_clear_booleval_in_prog(void)
{
	boolZysqlnullArray_t	*tmpZysqlnull;

	boolZysqlnull->booleval_in_prog = FALSE;
	if (NULL != boolZysqlnull->previous)
	{
		tmpZysqlnull = boolZysqlnull->previous;
		tmpZysqlnull->next_free = boolZysqlnull;
		boolZysqlnull = tmpZysqlnull;
	} else
		boolZysqlnull->frame_pointer = NULL;
}

/* The below finishes an in-progress boolean expression evaluation in the normal case */
static inline void bool_zysqlnull_finish(void)
{
	assert(boolZysqlnull->zysqlnull_seen || (INIT_GBL_BOOL_DEPTH == boolZysqlnull->cur_depth));
	if (boolZysqlnull->zysqlnull_seen)
	{
		bool_zysqlnull(0);
		assert(0 == boolZysqlnull->cur_depth);
		boolZysqlnull->array[0].is_zysqlnull = FALSE;
	}
	bool_zysqlnull_clear_booleval_in_prog();
}

static inline void bool_zysqlnull_finish_if_needed(void)
{
	if ((NULL != boolZysqlnull) && boolZysqlnull->booleval_in_prog)
		bool_zysqlnull_finish();
}

/* This function is invoked when we unwind an M frame.
 * It cleans up boolZysqlnull related structures, if any, corresponding to this frame.
 */
static inline void bool_zysqlnull_unwind(void)
{
	assert(NULL != frame_pointer);
	if (NULL != boolZysqlnull)
	{	/* It is possible multiple boolean expressions were nested all in the
		 * same M frame when an error occurred so handle that case too.
		 */
		for ( ; boolZysqlnull->frame_pointer == frame_pointer; )
			bool_zysqlnull_finish_if_needed();
	}
}

/* The below function finishes an in-progress boolean expression evaluation when a runtime error occurs.
 * Is very similar to `bool_zysqlnull_finish_if_needed()` function.
 */
static inline void bool_zysqlnull_finish_error_if_needed(void)
{
	int	depth;

	if ((NULL != boolZysqlnull) && boolZysqlnull->booleval_in_prog && (frame_pointer == boolZysqlnull->frame_pointer))
	{
		assert(boolZysqlnull->zysqlnull_seen || (INIT_GBL_BOOL_DEPTH == boolZysqlnull->cur_depth));
		if (boolZysqlnull->zysqlnull_seen)
		{
			/* Simulate bool_zysqlnull(0) but with none of asserts as things could be
			 * in an out-of-design state due to a runtime error at an arbitrary point.
			 */
			depth = boolZysqlnull->cur_depth;
			if (0 < depth)
			{
				for ( ; 0 <= depth; depth--)
				{
					boolZysqlnull->array[depth].is_zysqlnull = FALSE;
					boolZysqlnull->array[depth].andor_opcode = OC_NOOP;
				}
				boolZysqlnull->cur_depth = 0;
			}
		}
		bool_zysqlnull_clear_booleval_in_prog();
	}
}

/* This function is invoked from `mval2bool()` and `bxrelop_operator()` in case they need to do $ZYSQLNULL related processing */
static inline int bxoprnd_is_zysqlnull(int this_bool_depth, uint4 combined_opcode, boolean_t caller_is_mval2bool)
{
	bool_sqlnull_t	*bool_ptr;
	boolean_t	invert;
	opctype		andor_opcode, jmp_opcode;
	int		jmp_depth;
	int		result;

	assert(NULL != boolZysqlnull);
	/* The boolean expression operand is $ZYSQLNULL. Return value is not deterministic. */
	boolZysqlnull->zysqlnull_seen = TRUE;	/* in case this is the first $ZYSQLNULL we see in this boolexpr */
	bool_zysqlnull(this_bool_depth); /* can update "boolZysqlnull->array[this_bool_depth].is_zysqlnull" */
	assert(this_bool_depth == boolZysqlnull->cur_depth); /* should have been set by "bool_zysqlnull()" call above */
	assert(this_bool_depth < boolZysqlnull->alloc_depth);
	bool_ptr = &boolZysqlnull->array[this_bool_depth];
	set_is_zysqlnull_true(bool_ptr);
	SPLIT_ANDOR_OPCODE_JMP_OPCODE_JMP_DEPTH_INVERT(combined_opcode, andor_opcode, jmp_opcode, jmp_depth, invert);
	/* Note: If `boolZysqlnull->zysqlnull_seen` was FALSE at entry into this function, `bool_ptr->andor_opcode`
	 * would have not been set (`bool_andor()` returns right away till the first $ZYSQLNULL is seen in the
	 * boolean expression). Hence the need to do this here.
	 */
	bool_ptr->andor_opcode = andor_opcode;
	/* The current opcode (OC_COBOOL/OC_BXRELOP) is guaranteed to be followed by some JMP opcode (e.g. OC_JMPEQU etc.)
	 * but we do not want to do the jump because $ZYSQLNULL is the value of the boolean expression till now.
	 * Therefore we set the boolean evaluation result such a way that the jump is never taken.
	 */
	switch(jmp_opcode)
	{
	case OC_JMPNEQ:
	case OC_JMPEQU:
		result = ((OC_JMPNEQ == jmp_opcode) ? 0 : 1);
		if (invert)
			result = !result;
		break;
	case OC_JMPGTR:
	case OC_JMPGEQ:
		assert(!caller_is_mval2bool);
		result = -1;
		if (invert)
			result = 1;
		break;
	default:
		assert(!caller_is_mval2bool);
		assert((OC_JMPLEQ == jmp_opcode) || (OC_JMPLSS == jmp_opcode));
		result = 1;
		if (invert)
			result = -1;
		break;
	}
	return result;
}

/* This function is invoked from `mval2bool()` and `bxrelop_operator()` in case they find an operand in the boolean expression
 * that is not $ZYSQLNULL but is part of a boolean expression that has already seen a $ZYSQLNULL. This does some state
 * maintenance of the boolean evaluation result.
 */
static inline int bxoprnd_is_not_zysqlnull(int this_bool_depth, uint4 combined_opcode, int result,
						boolean_t bool_result, boolean_t caller_is_mval2bool)
{
	bool_sqlnull_t	*bool_ptr, *bool_ptr2;
	boolean_t	ok_to_inherit, invert;
	opctype		andor_opcode, jmp_opcode;
	int		jmp_depth;

	assert(NULL != boolZysqlnull);
	assert(boolZysqlnull->zysqlnull_seen);
	SPLIT_ANDOR_OPCODE_JMP_OPCODE_JMP_DEPTH_INVERT(combined_opcode, andor_opcode, jmp_opcode, jmp_depth, invert);
	if (this_bool_depth > jmp_depth)
	{
		bool_zysqlnull(this_bool_depth); /* can update "boolZysqlnull->array[this_bool_depth].is_zysqlnull" */
		bool_ptr = &boolZysqlnull->array[this_bool_depth];
		assert(this_bool_depth == boolZysqlnull->cur_depth);	/* should have been set by "bool_zysqlnull()"
									 * invocation above.
									 */
		assert(this_bool_depth < boolZysqlnull->alloc_depth);
		assert((this_bool_depth + 1) < boolZysqlnull->alloc_depth);
		bool_ptr2 = bool_ptr + 1;
		bool_ptr2->andor_opcode = andor_opcode;
		if (BOOL_RESULT_UNINITIALIZED != bool_ptr->result)
		{
			bool_ptr2->result = ((andor_opcode == bool_ptr->andor_opcode) ? bool_result : !bool_result);
			boolZysqlnull->cur_depth = this_bool_depth + 1;
			bool_zysqlnull_depth_check(this_bool_depth, INHERIT_TRUE);
			boolZysqlnull->cur_depth = this_bool_depth;
			ok_to_inherit = bool_zysqlnull_depth_check(jmp_depth, INHERIT_FALSE);
		} else
		{
			bool_ptr->is_zysqlnull = FALSE;
			DBG_CHECK_ANDOR_OPCODE_COMPATIBLE(andor_opcode, bool_ptr->andor_opcode);
			bool_ptr->result = ((andor_opcode == bool_ptr->andor_opcode) ? bool_result : !bool_result);
			switch(andor_opcode)
			{
			case OC_AND:
			case OC_NAND:
				bool_ptr2->result = 1;
				boolZysqlnull->cur_depth = this_bool_depth + 1;
				ok_to_inherit = bool_zysqlnull_depth_check(jmp_depth, INHERIT_FALSE);
				if (!ok_to_inherit)
					boolZysqlnull->cur_depth = this_bool_depth;
				break;
			case OC_OR:
			case OC_NOR:
				bool_ptr2->result = 0;
				boolZysqlnull->cur_depth = this_bool_depth + 1;
				ok_to_inherit = bool_zysqlnull_depth_check(jmp_depth, INHERIT_FALSE);
				if (!ok_to_inherit)
					boolZysqlnull->cur_depth = this_bool_depth;
				break;
			default:
				assert(OC_NOOP == andor_opcode);
				ok_to_inherit = FALSE;
				break;
			}
		}
		if (!ok_to_inherit)
		{	/* Even though the boolean expression evaluated at this depth is not $ZYSQLNULL, we cannot jump
			 * to the target depth (`jmp_depth`) because of an intermediate depth that evaluates to $ZYSQLNULL.
			 * Therefore return a value that will ensure we do not do the jump.
			 */
			switch(jmp_opcode)
			{
			case OC_JMPNEQ:
			case OC_JMPEQU:
				result = ((OC_JMPNEQ == jmp_opcode) ? 0 : 1);
				if (invert)
					result = !result;
				break;
			case OC_JMPGTR:
			case OC_JMPGEQ:
				assert(!caller_is_mval2bool);
				result = -1;
				if (invert)
					result = 1;
				break;
			default:
				assert(!caller_is_mval2bool);
				assert((OC_JMPLEQ == jmp_opcode) || (OC_JMPLSS == jmp_opcode));
				result = 1;
				if (invert)
					result = -1;
				break;
			}
		}
	}
	return result;
}

#endif /* BOOL_ZYSQLNULL_INCLUDED */
