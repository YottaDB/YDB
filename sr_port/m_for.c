/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF	bool		for_temps[];
GBLREF	char		window_token;
GBLREF	int4		source_error_found;
GBLREF	oprtype		*for_stack[MAX_FOR_STACK],
			**for_stack_ptr;
GBLREF	triple		*curtchain,
			pos_in_chain;

error_def(ERR_EQUAL);
error_def(ERR_MAXFORARGS);
error_def(ERR_SPOREOL);


int m_for(void)
{
	int		index, top;
	oprtype		iterate[MAX_FORARGS], terminate[MAX_FORARGS], start_addr[MAX_FORARGS],
			*body, *no_times_through, *skip_forchk,
			body_indr, jump_target,
			init_value, index_variable,
			iter_temp, term_temp, temp_tnxt;
	triple		*ld_reg[MAX_FORARGS],
			*jmpref, *ref, *ref1, *term_ref, *step_ref, *index_ref, *init_ref,
			forpos_in_chain;


	forpos_in_chain = pos_in_chain;

	if (window_token == TK_SPACE)
	{
		for_end_of_scope(0);
		if (!for_push())
			return FALSE;

		if (window_token != TK_EOL  &&  window_token != TK_SPACE)
		{
			stx_error(ERR_SPOREOL);
			return FALSE;
		}

		jmpref = newtriple(OC_FORCHK1);

		if (!linetail())
		{
			pos_in_chain = forpos_in_chain;
			assert(source_error_found);
			stx_error(source_error_found);
			return FALSE;
		}

		tnxtarg(&temp_tnxt);
		for_declare_addr(temp_tnxt);

		newtriple(OC_JMP)->operand[0] = put_tjmp(jmpref);

		for_pop();

		return TRUE;
	}

	if (window_token == TK_ATSIGN)
	{
		if (!indirection(&index_variable))
			return FALSE;

		ref = newtriple(OC_INDLVADR);
		ref->operand[0] = index_variable;
		index_variable = put_tref(ref);
		index_ref = NULL;
	}
	else
	{
		/* The following relies on the fact that lvn() always generates an OC_VAR triple first */
		index_ref = curtchain->exorder.bl;
		if (!lvn(&index_variable, OC_PUTINDX, NULL))
			return FALSE;

		assert(index_ref->exorder.fl->opcode == OC_VAR);
		assert(index_ref->exorder.fl->operand[0].oprclass == MVAR_REF);
	}

	if (window_token != TK_EQUAL)
	{
		stx_error(ERR_EQUAL);
		return FALSE;
	}

	(void)for_end_of_scope(0);
	if (!for_push())
		return FALSE;

	if (index_variable.oprval.tref->opcode == OC_PUTINDX  ||
	    index_variable.oprval.tref->opcode == OC_INDLVADR)
		for_temps[for_stack_ptr - for_stack] = TRUE;

	body = (oprtype *)mcalloc(SIZEOF(oprtype));
	skip_forchk = (oprtype *)mcalloc(SIZEOF(oprtype));
	body_indr = put_indr(body);
	jump_target.oprclass = iter_temp.oprclass
			     = term_temp.oprclass
			     = 0;
	no_times_through = NULL;

	for (top = 0;  ;  ++top)
	{
		if (top >= MAX_FORARGS)
		{
			stx_error(ERR_MAXFORARGS);
			return FALSE;
		}

		advancewindow();
		tnxtarg(&start_addr[top]);

		if (no_times_through != NULL)
		{
			*no_times_through = start_addr[top];
			no_times_through = NULL;
		}

		if (expr(&init_value) == EXPR_FAIL)
			return FALSE;

		if (window_token != TK_COLON)
		{
			iterate[top].oprclass = terminate[top].oprclass
					      = 0;
			ref = newtriple(OC_STO);
			ref->operand[0] = index_variable;
			ref->operand[1] = init_value;
			assert(init_value.oprclass == TRIP_REF);
		}
		else
		{
			init_ref = newtriple(OC_STOTEMP);
			init_ref->operand[0] = init_value;

			newtriple(OC_CONUM)->operand[0] = put_tref(init_ref);

			advancewindow();
			if (expr(&iterate[top]) == EXPR_FAIL)
				return FALSE;

			assert(iterate[top].oprclass == TRIP_REF);
			ref = iterate[top].oprval.tref;
			if (ref->opcode != OC_LIT)
			{
				for_temps[for_stack_ptr - for_stack] = TRUE;
				if (ref->opcode == OC_VAR)
				{
					ref1 = newtriple(OC_STOTEMP);
					ref1->operand[0] = put_tref(ref);
					iterate[top] = put_tref(ref1);
				}
			}

			if (window_token != TK_COLON)
			{
				ref = newtriple(OC_STO);
				ref->operand[0] = index_variable;
				ref->operand[1] = put_tref(init_ref);
				assert(init_value.oprclass == TRIP_REF);
				terminate[top].oprclass = 0;
			}
			else
			{
				advancewindow();
				if (expr(&terminate[top]) == EXPR_FAIL)
					return FALSE;

				assert(terminate[top].oprclass == TRIP_REF);
				ref = terminate[top].oprval.tref;
				if (ref->opcode != OC_LIT)
				{
					for_temps[for_stack_ptr - for_stack] = TRUE;
					if (ref->opcode == OC_VAR)
					{
						ref1 = newtriple(OC_STOTEMP);
						ref1->operand[0] = put_tref(ref);
						terminate[top] = put_tref(ref1);
					}
				}

				ref = newtriple(OC_STO);
				ref->operand[0] = index_variable;
				ref->operand[1] = put_tref(init_ref);
				assert(init_value.oprclass == TRIP_REF);

				term_ref = newtriple(OC_PARAMETER);
				term_ref->operand[0] = terminate[top];

				step_ref = newtriple(OC_PARAMETER);
				step_ref->operand[0] = iterate[top];
				step_ref->operand[1] = put_tref(term_ref);

				ref = newtriple(OC_FORINIT);
				ref->operand[0] = index_variable;
				ref->operand[1] = put_tref(step_ref);

				no_times_through = newtriple(OC_JMPGTR)->operand;
			}
		}

		if (top > 0  ||  window_token == TK_COMMA)
		{
			for_temps[for_stack_ptr - for_stack] = TRUE;

			if (jump_target.oprclass == 0)
				jump_target = put_tref(newtriple(OC_CDADDR));

			(ld_reg[top] = newtriple(OC_LDADDR))->destination = jump_target;
		}

		if (window_token != TK_COMMA)
			break;

		newtriple(OC_JMP)->operand[0] = body_indr;
	}

	if (no_times_through)
		*no_times_through = for_end_of_scope(1);

	*body = put_tjmp(newtriple(OC_FORCHK1));
	tnxtarg(skip_forchk);

	if (window_token != TK_EOL  &&  window_token != TK_SPACE)
	{
		stx_error(ERR_SPOREOL);
		return FALSE;
	}

	if (!linetail())
	{
		pos_in_chain = forpos_in_chain;
		assert(source_error_found);
		stx_error(source_error_found);
		return FALSE;
	}

	tnxtarg(&temp_tnxt);
	for_declare_addr(temp_tnxt);

	if (top > 0)
	{
		newtriple(OC_PASSTHRU)->operand[0] = index_variable;	/* Dummy ref to keep any temporary alive */
		newtriple(OC_JMPAT)->operand[0] = put_tref(ld_reg[0]);
	}

	for(index = 0;  index <= top;  ++index)
	{
		if (top > 0)
			tnxtarg(ld_reg[index]->operand);

		if (terminate[index].oprclass)
		{
			if (index_ref != NULL)
				put_mvar(&index_ref->exorder.fl->operand[0].oprval.vref->mvname);

			term_ref = newtriple(OC_PARAMETER);
			term_ref->operand[0] = terminate[index];

			step_ref = newtriple(OC_PARAMETER);
			step_ref->operand[0] = iterate[index];
			step_ref->operand[1] = put_tref(term_ref);

			ref = newtriple(OC_PARAMETER);
			ref->operand[0] = index_variable;
			ref->operand[1] = put_tref(step_ref);

			ref1 = newtriple(OC_FORLOOP);
			ref1->operand[0] = *skip_forchk;
			ref1->operand[1] = put_tref(ref);
		}
		else
			if (iterate[index].oprclass)
			{
				if (index_ref != NULL)
					put_mvar(&index_ref->exorder.fl->operand[0].oprval.vref->mvname);

				ref = newtriple(OC_ADD);
				ref->operand[0] = index_variable;
				ref->operand[1] = iterate[index];

				ref1 = newtriple(OC_STO);
				ref1->operand[0] = index_variable;
				ref1->operand[1] = put_tref(ref);

				newtriple(OC_JMP)->operand[0] = *body;
			}

		if (index < top)
			newtriple(OC_JMP)->operand[0] = start_addr[index + 1];
	}

	for_pop();

	return TRUE;
}
