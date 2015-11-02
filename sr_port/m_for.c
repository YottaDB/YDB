/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"
#include "lv_val.h"

GBLREF	char		window_token;
GBLREF	triple		*curtchain;

error_def(ERR_EQUAL);
error_def(ERR_FOROFLOW);
error_def(ERR_MAXFORARGS);
error_def(ERR_SPOREOL);

/* The following macro checks to see if the evaluation of control variable components has done
 * anything that might have expose us to a messed up the control variable context. We only
 * have a problem when the control variable is subscripted, because if an extrinsic rearranges
 * the array - a KILL will do it - the op_putindx we did initially might be pointing into
 * never-neverland and slamming a value into it would definately not be a healthly thing.
 * Without indirection we know at compile time whether or not the control variable is subscripted
 * but with indirection we only know at run-time; we tried some contortions to skip the refresh if
 * it's not needed but lost the battle with the compiler's tendency to lose reference with a scope
 * that's not short - OC_PASSTHRU is suppose to give it a clue but having two of those in a row
 * seems not to work.
 */
#define	DEAL_WITH_DANGER(LVL, CNTL_VAR, VAL)								\
{													\
	triple *Ref;											\
													\
	if ((TRUE_WITH_INDX == TAREF1(for_temps, for_stack_level)))					\
	{												\
		Ref = newtriple(OC_RFRSHINDX); 								\
		Ref->operand[0] = put_ilit(LVL); 							\
		Ref->operand[1] = put_ilit(1);								\
		CNTL_VAR = put_tref(Ref);								\
		newtriple(OC_PASSTHRU)->operand[0] = CNTL_VAR;	/* warn off optimizer */		\
	}												\
	Ref = newtriple(OC_STO);									\
	Ref->operand[0] = CNTL_VAR;									\
	Ref->operand[1] = VAL;										\
}

/* the macro below pushes the compiler FOR stack - the FOR_POP is in compiler.h 'cause stx_error uses it
 * there are actually two stacks - one for code references and one for temps flags; the code reference
 * one, for_stack, uses for_stack_ptr; the for_temps doesn't have its own global index, but instead uses
 * a local variable calculated from the relationship between the for_stack and for_stack_ptr
 */
#define	FOR_PUSH()											\
{													\
	int	Level;											\
													\
	Level = ((++(TREF(for_stack_ptr))) - (oprtype **)TADR(for_stack));				\
	if (MAX_FOR_STACK > Level)									\
	{												\
		assert(TREF(for_stack_ptr) > (oprtype **)TADR(for_stack));				\
		*(TREF(for_stack_ptr)) = NULL;								\
		TAREF1(for_temps, Level) = TAREF1(for_temps, Level - 1) ? TRUE : FALSE;			\
	} else												\
	{												\
		--(TREF(for_stack_ptr));								\
		stx_error(ERR_FOROFLOW, 1, (MAX_FOR_STACK - 1));					\
		FOR_POP(BLOWN_FOR);									\
		return FALSE;										\
	}												\
}

/* the macro below tucks a code reference into the for_stack so a FOR that's done can move on correctly when skipped */
#define	SAVE_FOR_OVER_ADDR()											\
{														\
	assert(TREF(for_stack_ptr) >= (oprtype **)TADR(for_stack));						\
	assert(TREF(for_stack_ptr) < (oprtype **)(TADR(for_stack) + (MAX_FOR_STACK * SIZEOF(oprtype **))));	\
	if (NULL == *(TREF(for_stack_ptr)))									\
		*(TREF(for_stack_ptr)) = (oprtype *)mcalloc(SIZEOF(oprtype));					\
	tnxtarg(*(TREF(for_stack_ptr)));									\
}

int m_for(void)
{
	unsigned int	arg_cnt, arg_index, for_stack_level;
	oprtype		arg_eval_addr[MAX_FORARGS], increment[MAX_FORARGS], terminate[MAX_FORARGS],
			arg_next_addr, arg_value, dummy, *body, control_variable,
			*iteration_start_addr, iteration_start_addr_indr, *not_even_once_addr;
	triple		*eval_next_addr[MAX_FORARGS], *control_ref,
			*forchk1opc, forpos_in_chain, *init_ref, *ref, *step_ref, *term_ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	forpos_in_chain = TREF(pos_in_chain);
	FOR_PUSH();
	if (TK_SPACE == window_token)
	{	/* "argumentless" form */
		FOR_END_OF_SCOPE(1, dummy);
		ref = newtriple(OC_FORCHK1);
		if (!linetail())
		{
			TREF(pos_in_chain) = forpos_in_chain;
			assert(TREF(source_error_found));
			stx_error(TREF(source_error_found));
			FOR_POP(BLOWN_FOR);
			return FALSE;
		}
		SAVE_FOR_OVER_ADDR();				/* stash address of next op in the for_stack array */
		newtriple(OC_JMP)->operand[0] = put_tjmp(ref);	/* transfer back to just before the begining of the body */
		FOR_POP(GOOD_FOR);				/* and pop the array */
		return TRUE;
	}
	for_stack_level = (TREF(for_stack_ptr) - TADR(for_stack));
	init_ref = newtriple(OC_FORNESTLVL);
	init_ref->operand[0] = put_ilit(for_stack_level);
	if (TK_ATSIGN == window_token)
	{
		if (!indirection(&control_variable))
		{
			FOR_POP(BLOWN_FOR);
			return FALSE;
		}
		ref = newtriple(OC_INDLVADR);
		ref->operand[0] = control_variable;
		control_variable = put_tref(ref);
		control_ref = NULL;
	} else
	{
		/* The following relies on the fact that lvn() always generates an OC_VAR triple first */
		control_ref = curtchain->exorder.bl;
		if (!lvn(&control_variable, OC_SAVPUTINDX, NULL))
		{
			FOR_POP(BLOWN_FOR);
			return FALSE;
		}
		assert(OC_VAR == control_ref->exorder.fl->opcode);
		assert(MVAR_REF == control_ref->exorder.fl->operand[0].oprclass);
	}
	if (TK_EQUAL != window_token)
	{
		stx_error(ERR_EQUAL);
		FOR_POP(BLOWN_FOR);
		return FALSE;
	}
	newtriple(OC_PASSTHRU)->operand[0] = control_variable;	/* make sure optimizer doesn't ditch control_variable */
	FOR_END_OF_SCOPE(1, dummy);
	assert((0 < for_stack_level) && (MAX_FOR_STACK >= for_stack_level));
	if ((OC_SAVPUTINDX == control_variable.oprval.tref->opcode) || (OC_INDLVADR == control_variable.oprval.tref->opcode))
		TAREF1(for_temps, for_stack_level) = TRUE_WITH_INDX;	/* most uses treat this as a boolean, but some need more */
	else
		init_ref->opcode = OC_NOOP;
	iteration_start_addr = (oprtype *)mcalloc(SIZEOF(oprtype));
	iteration_start_addr_indr = put_indr(iteration_start_addr);
	arg_next_addr.oprclass = NOCLASS;
	not_even_once_addr = NULL;	/* used to skip processing where the initial control exceeds the termination */
	for (arg_cnt = 0; ; ++arg_cnt)
	{
		if (MAX_FORARGS <= arg_cnt)
		{
			stx_error(ERR_MAXFORARGS);
			FOR_POP(BLOWN_FOR);
			return FALSE;
		}
		assert((TK_COMMA == window_token) || (TK_EQUAL == window_token));
		advancewindow();
		tnxtarg(&arg_eval_addr[arg_cnt]);		/* put location of this arg eval in arg_eval_addr array */
		if (NULL != not_even_once_addr)
		{
			*not_even_once_addr = arg_eval_addr[arg_cnt];
			not_even_once_addr = NULL;
		}
		if (EXPR_FAIL == expr(&arg_value))	/* starting (possibly only) value */
		{
			FOR_POP(BLOWN_FOR);
			return FALSE;
		}
		assert(TRIP_REF == arg_value.oprclass);
		if (TK_COLON != window_token)
		{	/* list point value? */
			increment[arg_cnt].oprclass = terminate[arg_cnt].oprclass = 0;
			DEAL_WITH_DANGER(for_stack_level, control_variable, arg_value);
		} else
		{	/* stepping value */
			init_ref = newtriple(OC_STOTEMP);		/* tuck it in a temp undisturbed by coming evals */
			init_ref->operand[0] = arg_value;
			newtriple(OC_CONUM)->operand[0] = put_tref(init_ref);	/* make start numeric */
			advancewindow();				/* past the first colon */
			if (EXPR_FAIL == expr(&increment[arg_cnt]))	/* pick up step */
			{
				FOR_POP(BLOWN_FOR);
				return FALSE;
			}
			assert(TRIP_REF == increment[arg_cnt].oprclass);
			ref = increment[arg_cnt].oprval.tref;
			if (OC_LIT != ref->opcode)
			{
				if (!TAREF1(for_temps, for_stack_level))
					TAREF1(for_temps, for_stack_level) = TRUE;
				if (OC_VAR == ref->opcode)
				{
					step_ref = newtriple(OC_STOTEMP);
					step_ref->operand[0] = put_tref(ref);
					increment[arg_cnt] = put_tref(step_ref);
				}
			}
			if (TK_COLON != window_token)
			{
				DEAL_WITH_DANGER(for_stack_level, control_variable, put_tref(init_ref));
				terminate[arg_cnt].oprclass = 0;	/* no termination on iteration for this arg */
			} else
			{
				advancewindow();	/* past the second colon */
				if (EXPR_FAIL == expr(&terminate[arg_cnt]))		/* termination control value */
				{
					FOR_POP(BLOWN_FOR);
					return FALSE;
				}
				assert(TRIP_REF == terminate[arg_cnt].oprclass);
				ref = terminate[arg_cnt].oprval.tref;
				if (OC_LIT != ref->opcode)
				{
					if (!TAREF1(for_temps, for_stack_level))
						TAREF1(for_temps, for_stack_level) = TRUE;
					if (OC_VAR == ref->opcode)
					{
						term_ref = newtriple(OC_STOTEMP);
						term_ref->operand[0] = put_tref(ref);
						terminate[arg_cnt] = put_tref(term_ref);
					}
				}
				DEAL_WITH_DANGER(for_stack_level, control_variable, put_tref(init_ref));
				term_ref = newtriple(OC_PARAMETER);
				term_ref->operand[0] = terminate[arg_cnt];
				step_ref = newtriple(OC_PARAMETER);
				step_ref->operand[0] = increment[arg_cnt];
				step_ref->operand[1] = put_tref(term_ref);
				ref = newtriple(OC_FORINIT);
				ref->operand[0] = control_variable;
				ref->operand[1] = put_tref(step_ref);
				not_even_once_addr = newtriple(OC_JMPGTR)->operand;
			}
		}
		if ((0 < arg_cnt) || (TK_COMMA == window_token))
		{
			if (!TAREF1(for_temps, for_stack_level))
				TAREF1(for_temps, for_stack_level) = TRUE;
			if (NOCLASS == arg_next_addr.oprclass)
				arg_next_addr = put_tref(newtriple(OC_CDADDR));
			(eval_next_addr[arg_cnt] = newtriple(OC_LDADDR))->destination = arg_next_addr;
		}
		if (TK_COMMA != window_token)
			break;
		newtriple(OC_JMP)->operand[0] = iteration_start_addr_indr;
	}
	if (not_even_once_addr)
		 FOR_END_OF_SCOPE(1, *not_even_once_addr);	/* 1 means down a level */
	forchk1opc = newtriple(OC_FORCHK1);	/* FORCHK1 is a do-nothing routine used by the out-of-band mechanism */
	*iteration_start_addr = put_tjmp(forchk1opc);
	body = (oprtype *)mcalloc(SIZEOF(oprtype));
	tnxtarg(body);
	if ((TK_EOL != window_token) && (TK_SPACE != window_token))
	{
		stx_error(ERR_SPOREOL);
		FOR_POP(BLOWN_FOR);
		return FALSE;
	}
	if (!linetail())
	{
		TREF(pos_in_chain) = forpos_in_chain;
		assert(TREF(source_error_found));
		stx_error(TREF(source_error_found));
		FOR_POP(BLOWN_FOR);
		return FALSE;
	}
	SAVE_FOR_OVER_ADDR();		/* stash address of next op in the for_stack array */
	if (0 < arg_cnt)
		newtriple(OC_JMPAT)->operand[0] = put_tref(eval_next_addr[0]);
	for (arg_index = 0; arg_index <= arg_cnt; ++arg_index)
	{
		if (0 < arg_cnt)
			tnxtarg(eval_next_addr[arg_index]->operand);
			if (TRUE_WITH_INDX == TAREF1(for_temps, for_stack_level))
			{	/* since it might have moved, before touching the control variable get a fix on it */
				ref = newtriple(OC_RFRSHINDX);
				ref->operand[0] = put_ilit(for_stack_level);
				ref->operand[1] = put_ilit((increment[arg_index].oprclass || terminate[arg_index].oprclass)
					? FALSE : TRUE); /* if increment rather than new value, rfrsh w/ srchindx else putindx */
				control_variable = put_tref(ref);
			} else
			{
				assert(control_ref);
				control_variable = put_mvar(&control_ref->exorder.fl->operand[0].oprval.vref->mvname);
			}
			newtriple(OC_PASSTHRU)->operand[0] = control_variable;	/* warn off optimizer */
		if (terminate[arg_index].oprclass)
		{
			term_ref = newtriple(OC_PARAMETER);
			term_ref->operand[0] = terminate[arg_index];
			step_ref = newtriple(OC_PARAMETER);
			step_ref->operand[0] = increment[arg_index];
			step_ref->operand[1] = put_tref(term_ref);
			init_ref = newtriple(OC_PARAMETER);
			init_ref->operand[0] = control_variable;
			init_ref->operand[1] = put_tref(step_ref);
			ref = newtriple(OC_FORLOOP);
			/* redirects back to forchk1, which is at the beginning of new iteration */
			ref->operand[0] = *iteration_start_addr;
			ref->operand[1] = put_tref(init_ref);
		} else if (increment[arg_index].oprclass)
		{
			step_ref = newtriple(OC_ADD);
			step_ref->operand[0] = control_variable;
			step_ref->operand[1] = increment[arg_index];
			ref = newtriple(OC_STO);
			ref->operand[0] = control_variable;
			ref->operand[1] = put_tref(step_ref);
			newtriple(OC_JMP)->operand[0] = *iteration_start_addr;
		}
		if (arg_index < arg_cnt)	/* go back and evaluate the next argument */
			newtriple(OC_JMP)->operand[0] = arg_eval_addr[arg_index + 1];
	}
	FOR_POP(GOOD_FOR);
	return TRUE;
}
