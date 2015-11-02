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
error_def(ERR_FORCTRLINDX);	/* until we fix the problem this guards against */
error_def(ERR_FOROFLOW);
error_def(ERR_MAXFORARGS);
error_def(ERR_SPOREOL);

/* The following macro checks to see if the evaluation of control variable components has done
 * anything that might have messed up our control variable context. temp_subs is a flag used
 * by m_set to identify parsing of extrinsics external calls and $increment() - as far as we know,
 * we don't care about $increment() and external calls are a long shot, but since both of them
 * seem rarely useful in this context, we borrowed it principally to detect exrinsics. We only
 * have a problem when the control variable is subscripted, because if an extrinsic rearranges
 * the array - a KILL will do it - the op_putindx we did initially might be pointing into
 * never-neverland and slamming a value into it would definately not be a healthly thing.
 * Without indirection we know at compile time whether or not the control variable is subscripted
 * but with indirection we only know at run-time so we have additional contorsions to skip the
 * refresh if it's not needed. All this is really ugly, so if you have a better idea, have at it.
 * The odd construction for indirection prevents an unreferenced temp (INDR_SUBS) from upsetting
 * the compiler.
 */
#define	DEAL_WITH_DANGER(CTRL_LV_REF, INDR_SUBS, CNTL_VAR)						\
{													\
	ref = step_ref = NULL;										\
	if ((TREF(temp_subs)) && (OC_PUTINDX == CTRL_LV_REF))						\
	{												\
		stx_error(ERR_FORCTRLINDX);	/* until this is fixed + next line */			\
		TREF(source_error_found) = (int4 )ERR_FORCTRLINDX;					\
		/* ref = newtriple(OC_RFRSHINDX); */							\
	} else if (OC_INDLVADR == CTRL_LV_REF)								\
	{												\
		step_ref = newtriple(OC_VXCMPL);							\
		step_ref->operand[0] = INDR_SUBS;							\
		step_ref->operand[1] = put_ilit(TREF(temp_subs) ? 1 : 9);				\
		step_ref = newtriple(OC_JMPNEQ);							\
		ins_errtriple(ERR_FORCTRLINDX);	/* until this is fixed */				\
		/* ref = newtriple(OC_RFRSHINDX); */								\
	}												\
	if (NULL != ref)										\
	{												\
		ref->operand[0] = put_ilit(TRUE); 							\
		ref->operand[1] = put_ilit(TREF(for_stack_ptr) - TADR(for_stack) - 1);			\
		CNTL_VAR = put_tref(ref);								\
	}												\
	if (NULL != step_ref)										\
		tnxtarg(&step_ref->operand[0]);								\
}

#define	FOR_POP()											\
{													\
	--(TREF(for_stack_ptr));									\
	assert(TREF(for_stack_ptr) >= (oprtype **)TADR(for_stack));					\
	assert(TREF(for_stack_ptr) < (oprtype **)(TADR(for_stack) + ggl_for_stack));			\
}

#define	FOR_PUSH()											\
{													\
	int	level;											\
													\
	if (++(TREF(for_stack_ptr)) < (oprtype **)(TADR(for_stack) + ggl_for_stack))			\
	{												\
		assert(TREF(for_stack_ptr) >= (oprtype **)TADR(for_stack));				\
		*(TREF(for_stack_ptr)) = NULL;								\
		level = (TREF(for_stack_ptr) - (oprtype **)TADR(for_stack));				\
		TAREF1(for_temps, level) = TAREF1(for_temps, level - 1);				\
	} else												\
	{												\
		stx_error(ERR_FOROFLOW);								\
		--(TREF(for_stack_ptr));								\
		return FALSE;										\
	}												\
}
#define	SAVE_FOR_OVER_ADDR()										\
{													\
	assert(TREF(for_stack_ptr) >= (oprtype **)TADR(for_stack));					\
	assert(TREF(for_stack_ptr) < (oprtype **)(TADR(for_stack) + ggl_for_stack));			\
	if (NULL == *(TREF(for_stack_ptr)))								\
		*(TREF(for_stack_ptr)) = (oprtype *)mcalloc(SIZEOF(oprtype));				\
	tnxtarg(*TREF(for_stack_ptr));									\
}

int m_for(void)
{
	unsigned int	arg_cnt, arg_index, for_stack_level;
	oprtype		arg_eval_addr[MAX_FORARGS], increment[MAX_FORARGS], terminate[MAX_FORARGS],
			arg_next_addr, arg_value, *body, control_variable, indr_jeopardy_ref,
			*iteration_start_addr, iteration_start_addr_indr, *not_even_once_addr;
	triple		*eval_next_addr[MAX_FORARGS],
			*control_ref, *ctrl_lv_ref, forpos_in_chain, *init_ref, *ref, *step_ref, *term_ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	forpos_in_chain = TREF(pos_in_chain);
	(void)for_end_of_scope(0);
	if (TK_SPACE == window_token)
	{	/* "argumentless" form */
		(void)for_end_of_scope(0);
		FOR_PUSH();
		ref = newtriple(OC_FORCHK1);
		if (!linetail())
		{
			TREF(pos_in_chain) = forpos_in_chain;
			assert(TREF(source_error_found));
			stx_error(TREF(source_error_found));
			FOR_POP();
			return FALSE;
		}
		SAVE_FOR_OVER_ADDR();				/* stash address of next op in the for_stack array */
		newtriple(OC_JMP)->operand[0] = put_tjmp(ref);	/* transfer back to just before the begining of the body */
		FOR_POP();					/* and pop the array */
		return TRUE;
	}
	if (TK_ATSIGN == window_token)
	{
		if (!indirection(&control_variable))
			return FALSE;
		ref = newtriple(OC_INDLVADR);
		ref->operand[0] = control_variable;
		control_variable = put_tref(ref);
		ref = newtriple(OC_FORCTRLINDR2);	/* return whether indlvadr hit subscripts */
		ref->operand[0] = put_ilit(NOARG);
		indr_jeopardy_ref = put_tref(ref);
		control_ref = NULL;
	} else
	{
		/* The following relies on the fact that lvn() always generates an OC_VAR triple first */
		control_ref = curtchain->exorder.bl;
		if (!lvn(&control_variable, OC_PUTINDX, NULL))
			return FALSE;
		assert(OC_VAR == control_ref->exorder.fl->opcode);
		assert(MVAR_REF == control_ref->exorder.fl->operand[0].oprclass);
	}
	if (TK_EQUAL != window_token)
	{
		stx_error(ERR_EQUAL);
		return FALSE;
	}
	newtriple(OC_PASSTHRU)->operand[0] = control_variable;	/* make sure optimizer doesn't ditch control_variable */
	(void)for_end_of_scope(0);
	FOR_PUSH();
	for_stack_level = (TREF(for_stack_ptr) - TADR(for_stack));
	assert((0 < for_stack_level) && (MAX_FOR_STACK > for_stack_level));
	ctrl_lv_ref = control_variable.oprval.tref;
	if ((OC_PUTINDX == ctrl_lv_ref->opcode) || (OC_INDLVADR == ctrl_lv_ref->opcode))
		TAREF1(for_temps, for_stack_level) = TRUE;
	iteration_start_addr = (oprtype *)mcalloc(SIZEOF(oprtype));
	iteration_start_addr_indr = put_indr(iteration_start_addr);
	arg_next_addr.oprclass = 0;
	not_even_once_addr = NULL;	/* used to skip processing where the initial control exceeds the termination */
	for (arg_cnt = 0; ; ++arg_cnt)
	{
		if (MAX_FORARGS <= arg_cnt)
		{
			stx_error(ERR_MAXFORARGS);
			FOR_POP();
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
		/* "borrow" temp_subs from m_set to find out if anything might mess with the array holding our control variable */
		TREF(temp_subs) = FALSE;
		if (EXPR_FAIL == expr(&arg_value))	/* starting (possibly only) value */
		{
			FOR_POP();
			return FALSE;
		}
		assert(TRIP_REF == arg_value.oprclass);
		if (TK_COLON != window_token)
		{	/* list point value? */
			increment[arg_cnt].oprclass = terminate[arg_cnt].oprclass = 0;
			DEAL_WITH_DANGER(ctrl_lv_ref->opcode, indr_jeopardy_ref, control_variable);
			ref = newtriple(OC_STO);
			ref->operand[0] = control_variable;
			ref->operand[1] = arg_value;
		} else
		{	/* stepping value */
			init_ref = newtriple(OC_STOTEMP);	/* tuck it in a temp undisturbed by coming evals */
			init_ref->operand[0] = arg_value;
			newtriple(OC_CONUM)->operand[0] = put_tref(init_ref);	/* make start numeric */
			advancewindow();					/* past the first colon */
			if (EXPR_FAIL == expr(&increment[arg_cnt]))		/* pick up step */
			{
				FOR_POP();
				return FALSE;
			}
			assert(TRIP_REF == increment[arg_cnt].oprclass);
			ref = increment[arg_cnt].oprval.tref;
			if (OC_LIT != ref->opcode)
			{
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
				DEAL_WITH_DANGER(ctrl_lv_ref->opcode, indr_jeopardy_ref, control_variable);
				ref = newtriple(OC_STO);
				ref->operand[0] = control_variable;
				ref->operand[1] = put_tref(init_ref);
				terminate[arg_cnt].oprclass = 0;	/* no termination on iteration for this arg */
			} else
			{
				advancewindow();	/* past the second colon */
				if (EXPR_FAIL == expr(&terminate[arg_cnt]))		/* termination control value */
				{
					FOR_POP();
					return FALSE;
				}
				assert(TRIP_REF == terminate[arg_cnt].oprclass);
				ref = terminate[arg_cnt].oprval.tref;
				if (OC_LIT != ref->opcode)
				{
					TAREF1(for_temps, for_stack_level) = TRUE;
					if (OC_VAR == ref->opcode)
					{
						term_ref = newtriple(OC_STOTEMP);
						term_ref->operand[0] = put_tref(ref);
						terminate[arg_cnt] = put_tref(term_ref);
					}
				}
				DEAL_WITH_DANGER(ctrl_lv_ref->opcode, indr_jeopardy_ref, control_variable);
				ref = newtriple(OC_STO);
				ref->operand[0] = control_variable;
				ref->operand[1] = put_tref(init_ref);
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
			TAREF1(for_temps, for_stack_level) = TRUE;
			if (0 == arg_next_addr.oprclass)
				arg_next_addr = put_tref(newtriple(OC_CDADDR));
			(eval_next_addr[arg_cnt] = newtriple(OC_LDADDR))->destination = arg_next_addr;
		}
		if (TK_COMMA != window_token)
			break;
		newtriple(OC_JMP)->operand[0] = iteration_start_addr_indr;
	}
	if (not_even_once_addr)
		*not_even_once_addr = for_end_of_scope(1);	/* 1 means down a level */
	ref = newtriple(OC_FORCHK1);		/* FORCHK1 is a do-nothing routine used by the out-of-band mechanism */
	*iteration_start_addr = put_tjmp(ref);
	body = (oprtype *)mcalloc(SIZEOF(oprtype));
	tnxtarg(body);
	if ((TK_EOL != window_token) && (TK_SPACE != window_token))
	{
		stx_error(ERR_SPOREOL);
		FOR_POP();
		return FALSE;
	}
	if (!linetail())
	{
		TREF(pos_in_chain) = forpos_in_chain;
		assert(TREF(source_error_found));
		stx_error(TREF(source_error_found));
		FOR_POP();
		return FALSE;
	}
	SAVE_FOR_OVER_ADDR();		/* stash address of next op in the for_stack array */
	if (0 < arg_cnt)
		newtriple(OC_JMPAT)->operand[0] = put_tref(eval_next_addr[0]);
	for (arg_index = 0; arg_index <= arg_cnt; ++arg_index)
	{
		if (0 < arg_cnt)
			tnxtarg(eval_next_addr[arg_index]->operand);
		if (terminate[arg_index].oprclass)
		{
			if (NULL != control_ref)	 /* else should op_rfrshindx, which should update control_variable */
			{
				if (OC_PUTINDX == ctrl_lv_ref->opcode)
					put_mvar(&control_ref->exorder.fl->operand[0].oprval.vref->mvname);
				else
					control_variable = put_mvar(&control_ref->exorder.fl->operand[0].oprval.vref->mvname);
			}
			term_ref = newtriple(OC_PARAMETER);
			term_ref->operand[0] = terminate[arg_index];
			step_ref = newtriple(OC_PARAMETER);
			step_ref->operand[0] = increment[arg_index];
			step_ref->operand[1] = put_tref(term_ref);
			init_ref = newtriple(OC_PARAMETER);
			init_ref->operand[0] = control_variable;
			init_ref->operand[1] = put_tref(step_ref);
			ref = newtriple(OC_FORLOOP);
			ref->operand[0] = *body;
			ref->operand[1] = put_tref(init_ref);
		} else if (increment[arg_index].oprclass)
		{
			if (NULL != control_ref)	 /* else should op_rfrshindx, which should update control_variable */
			{
				if (OC_PUTINDX == ctrl_lv_ref->opcode)
					put_mvar(&control_ref->exorder.fl->operand[0].oprval.vref->mvname);
				else
					control_variable = put_mvar(&control_ref->exorder.fl->operand[0].oprval.vref->mvname);
			}
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
	FOR_POP();
	return TRUE;
}
