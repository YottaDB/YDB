/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GLVN_POOL_H_INCLUDED
#define GLVN_POOL_H_INCLUDED

#include "callg.h"
#include "compiler.h"
#include "lv_val.h"

/* Here we provide tools for saving identifying information of local or global variables. For local variables this includes
 * the variable name itself, as well as copies of each subscript. For global variables we save each argument that needs to
 * be later passed into op_gvname/op_gvnaked/op_gvextnam. Note that deferring the op_gvnaked call allows us to achieve
 * correct naked indicator flow. For example, in the command SET @"^(subs)"=rhs, the ^() needs to be evaluated relative to
 * the expression on the right hand side, and the right hand side needs to happen AFTER the subscripts have been evaluated.
 *	The structure involved - the "glvn pool" - consists of a stack of entries, each corresponding to a single glvn, and
 * a parallel stack of mvals, each corresponding to some parent entry. Both stacks are expandable, doubling in size whenever
 * they run out of space. The glvn pool entries can be popped in three different ways. For SET, it's convenient to save the
 * glvn pool state, i.e. the top of the array of still-relevant entries. After completion of the set, the pool is restored to
 * this state and younger entries popped. With FOR, it's more convenient defer popping until another FOR loop at the same
 * nesting level starts. At that time the previously used FOR slot is recycled and everything younger than it is popped.
 * Finally, when a non-indirect frame is unwound, all younger pool entries are popped.
 */

#define ANY_SLOT			0
#define FIRST_SAVED_ARG(SLOT)		((OC_SAVLVN == (SLOT)->sav_opcode) ? 1 : 0)
/* To avoid breaking gtmpcat, we retain the 'for_ctrl_stack' field of the frame_pointer struct instead of replacing it
* with a uint4 'glvn_indx' field. The macros manipulate this field. Also for the convenience of gtmpcat: we declare the
* glvn_pool and glvn_pool_entry structs in compiler.h instead of here. This is because gtmpcat does not know to include
* glvn_pool.h when going through gtm_threadgbl_defs.h.
* 	When gtmpcat is changed to accommodate a new name, these macros and the stack frame struct need to be changed,
* and the glvn_pool structs in compiler.h should probably be moved into this file.
*/
#define GLVN_INDX(FP)			((uint4)(UINTPTR_T)((FP)->for_ctrl_stack))
#define GLVN_POOL_EMPTY			((uint4)-1)	/* this must be an "impossible" value for a real pool */
#define GLVN_POOL_SLOTS_AVAILABLE	((TREF(glvn_pool_ptr))->capacity - (TREF(glvn_pool_ptr))->top)
#define GLVN_POOL_MVALS_AVAILABLE	((TREF(glvn_pool_ptr))->mval_capacity - (TREF(glvn_pool_ptr))->mval_top)
#define INIT_GLVN_POOL_CAPACITY		8
#define INIT_GLVN_POOL_MVAL_CAPACITY	64
#define GLVN_POOL_UNTOUCHED		0
#define MAX_EXPECTED_CAPACITY		2048
#define MAX_EXPECTED_MVAL_CAPACITY	2048
#define SLOT_NEEDS_REWIND(INDX)		(((TREF(glvn_pool_ptr))->top <= (INDX)) && (GLVN_POOL_EMPTY != (INDX)))
#define SLOT_OPCODE(INDX)		((TREF(glvn_pool_ptr))->slot[INDX].sav_opcode)

#define SET_GLVN_INDX(FP, VALUE)					\
{									\
	(FP)->for_ctrl_stack = (unsigned char *)(UINTPTR_T)(VALUE);	\
}

#define GLVN_POOL_EXPAND_IF_NEEDED				\
{								\
	if (!TREF(glvn_pool_ptr))				\
		glvn_pool_init();				\
	else if (!GLVN_POOL_SLOTS_AVAILABLE)			\
		glvn_pool_expand_slots();			\
}

#define ENSURE_GLVN_POOL_SPACE(SPC)					\
{									\
	if (GLVN_POOL_MVALS_AVAILABLE < (uint4)(SPC))			\
	{								\
		glvn_pool_expand_mvals();				\
		assert(GLVN_POOL_MVALS_AVAILABLE >= (uint4)(SPC));	\
	}								\
}

#define GET_GLVN_POOL_STATE(SLOT, M)				\
{	/* Find current available SLOT and mval. */		\
	uint4			INDX, MVAL_INDX;		\
								\
	INDX = (TREF(glvn_pool_ptr))->share_slot;		\
	SLOT = &(TREF(glvn_pool_ptr))->slot[INDX];		\
	MVAL_INDX = SLOT->mval_top;				\
	M = &(TREF(glvn_pool_ptr))->mval_stack[MVAL_INDX];	\
}

#define DRAIN_GLVN_POOL_IF_NEEDED												\
{																\
	int			I;												\
	uint4			INDX, FINDX;											\
	glvn_pool_entry		*SLOT;												\
																\
	if ((GLVN_POOL_UNTOUCHED != GLVN_INDX(frame_pointer)) && !(frame_pointer->flags & SFF_INDCE))				\
	{	/* Someone used an ugly FOR control variable or did an indirect set. */						\
		INDX = (GLVN_POOL_EMPTY != GLVN_INDX(frame_pointer)) ? GLVN_INDX(frame_pointer) : 0;				\
		op_glvnpop(INDX);												\
		for (I = 1; I <= MAX_FOR_STACK; I++)										\
		{	/* rewind the for_slot array */										\
			FINDX = (TREF(glvn_pool_ptr))->for_slot[I];								\
			if (SLOT_NEEDS_REWIND(FINDX))										\
			{	/* reset to precursor */									\
				SLOT = &(TREF(glvn_pool_ptr))->slot[FINDX];							\
				assert(!SLOT_NEEDS_REWIND(SLOT->precursor));							\
				(TREF(glvn_pool_ptr))->for_slot[I] = SLOT->precursor;						\
			} else	/* no higher FOR levels were used by current frame */						\
				break;												\
		}														\
	}															\
}

#define INSERT_INDSAVGLVN(CTRL, V, RECYCLE, DO_REF)								\
{														\
	triple	*PUSH, *SAV, *PAR;										\
														\
	PUSH = newtriple(OC_GLVNSLOT);										\
	PUSH->operand[0] = put_ilit((mint)(RECYCLE));								\
	CTRL = put_tref(PUSH);											\
	SAV = newtriple(OC_INDSAVGLVN);										\
	SAV->operand[0] = V;											\
	PAR = newtriple(OC_PARAMETER);										\
	SAV->operand[1] = put_tref(PAR);									\
	PAR->operand[0] = CTRL;											\
	PAR->operand[1] = put_ilit((mint)(DO_REF));	/* flag to suppress global reference here */		\
}

void	glvn_pool_init(void);							/* invoked via GLVN_POOL_EXPAND_IF_NEEDED macro */
void	glvn_pool_expand_slots(void);						/* invoked via GLVN_POOL_EXPAND_IF_NEEDED macro */
void	glvn_pool_expand_mvals(void);						/* invoked via ENSURE_GLVN_POOL_SPACE macro */
void	op_glvnpop(uint4 indx);							/* Used by [SET and $ORDER()] */
uint4	op_glvnslot(uint4 recycle);						/* Used by [FOR, SET and $ORDER()] */
void	op_indsavglvn(mval *target, uint4 slot, uint4 do_ref);			/* Used by [SET and $ORDER()] */
void	op_indsavlvn(mval *target, uint4 slot);					/* Used by [FOR] */
void	op_rfrshgvn(uint4 indx, opctype oc);					/* Used by [SET and $ORDER()] */
lv_val	*op_rfrshlvn(uint4 indx, opctype oc);					/* Used by [FOR, SET and $ORDER()] */
void	op_savgvn(UNIX_ONLY_COMMA(int argcnt) mval *val_arg, ...);		/* Used by [SET and $ORDER()] */
void	op_savlvn(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...);		/* Used by [FOR, SET and $ORDER()] */
void	op_shareslot(uint4 indx, opctype opcode);				/* Used by [FOR, SET and $ORDER()] */
void	op_stoglvn(uint4 indx, mval *value);					/* Used by [SET] */

#endif	/* GLVN_POOL_H_INCLUDED */
