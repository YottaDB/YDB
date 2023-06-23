/****************************************************************
 *								*
 * Copyright (c) 2006-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "mmemory.h"
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "fnpc.h"
#include "gtm_utf8.h"
#include "stringpool.h"
#include "op.h"

GBLREF boolean_t	gtm_utf8_mode;

error_def(ERR_COMMA);

/*
 * Given a input (op) indicating whether we are using $ZPIECE or $PIECE, create the appropriate triple for runtime execution
 *	or run $[Z]PIECE if all inputs are literals. There is also a possibility of a OC_FNZP1 being generated if appropriate.
 * @input[out] a A pointer that will be set to the the result of the expression; in some cases a triple to be evaluated, or
 *	the string literal representing the result of the $PIECE fnction
 * @returns An integer flag of; TRUE if the function completed successfully, or FALSE if there was an error
 * @par Side effects
 *  - Calls advance window multiple times, and consumes tokens accordingly
 *  - Calls expr multiple times, which (most notably) adds literals to a hash table
 *  - Calls ins_triple, which adds triples to the execution chain
 *  - Calls st2pool, which inserts strings into the string pool
 */
int f_piece(oprtype *a, opctype op)
{
	delimfmt	unichar;
	mval		*delim_mval, tmp_mval;
	oprtype		x, *newop;
	triple		*delimiter, *first, *last, *r;
	static mstr	scratch_space = {0, 0, 0};

	DCL_THREADGBL_ACCESS;
	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	delimiter = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(delimiter);
	first = newtriple(OC_PARAMETER);
	delimiter->operand[1] = put_tref(first);
	if (EXPR_FAIL == expr(&x, MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
		first->operand[0] = put_ilit(1);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(first->operand[0]), MUMPS_INT))
			return FALSE;
	}
	assert(TRIP_REF == x.oprclass);
	if ((TK_COMMA != TREF(window_token)) && (OC_LIT == x.oprval.tref->opcode)
	    && (1 == ((gtm_utf8_mode && (OC_FNZPIECE != op)) ? MV_FORCE_LEN_DEC(&x.oprval.tref->operand[0].oprval.mlit->v)
		      : x.oprval.tref->operand[0].oprval.mlit->v.str.len)))
	{	/* Potential shortcut to op_fnzp1 or op_fnp1. Make some further checks */
		delim_mval = &x.oprval.tref->operand[0].oprval.mlit->v;
		/* Both valid chars of char_len 1 and invalid chars of byte length 1 get the fast path */
		unichar.unichar_val = 0;
		if (!gtm_utf8_mode || OC_FNZPIECE == op)
		{       /* Single byte delimiter */
			r->opcode = OC_FNZP1;
			unichar.unibytes_val[0] = *delim_mval->str.addr;
		} else
		{       /* Potentially multiple bytes in one int */
			r->opcode = OC_FNP1;
			assert(SIZEOF(int) >= delim_mval->str.len);
			memcpy(unichar.unibytes_val, delim_mval->str.addr, delim_mval->str.len);
		}
		delimiter->operand[0] = put_ilit(unichar.unichar_val);
		/* If we have all literals, run at compile time and return the result. To maintain backwards compatibility,
		 * we should emit a warning if there is an invalid UTF8 character, but continue compilation anyaway.
		 */
		if ((OC_LIT == r->operand[0].oprval.tref->opcode)
			&& (OC_ILIT == delimiter->operand[0].oprval.tref->opcode)
			&& (OC_ILIT == first->operand[0].oprval.tref->opcode)
			&& (!gtm_utf8_mode || (valid_utf_string(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str)
				&& valid_utf_string(&x.oprval.tref->operand[0].oprval.mlit->v.str))))
		{	/* We don't know how much space we will use; but we know it will be <= the size of the current string */
			if (scratch_space.len < r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len)
			{
				if (NULL != scratch_space.addr)
					free(scratch_space.addr);
				scratch_space.addr = malloc(r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len);
				scratch_space.len = r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len;
			}
			tmp_mval.str.addr = scratch_space.addr;
			if (OC_FNZP1 == r->opcode)
			{
				op_fnzp1(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v, /* First string */
					delimiter->operand[0].oprval.tref->operand[0].oprval.ilit,
					first->operand[0].oprval.tref->operand[0].oprval.ilit,
					&tmp_mval);
			} else
			{
				op_fnp1(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v, /* First string */
					delimiter->operand[0].oprval.tref->operand[0].oprval.ilit,
					first->operand[0].oprval.tref->operand[0].oprval.ilit,
					&tmp_mval);
			}
			s2pool(&tmp_mval.str);
			newop = (oprtype *)mcalloc(SIZEOF(oprtype));
			*newop = put_lit(&tmp_mval);				/* Copies mval so stack var tmp_mval not an issue */
			assert(TRIP_REF == newop->oprclass);
			newop->oprval.tref->src = r->src;
			*a = put_tref(newop->oprval.tref);
			return TRUE;

		}
		ins_triple(r);
		*a = put_tref(r);
		return TRUE;
	}
	/* Fall into here if (1) have multi-char delimiter or (2) an invalid utf8 sequence of bytelen > 1
	 * This generates the longer form call to op_fnpiece/op_fnzpiece.
	 */
	delimiter->operand[0] = x;
	last = newtriple(OC_PARAMETER);
	first->operand[1] = put_tref(last);
	if (TK_COMMA != TREF(window_token))
		last->operand[0] = first->operand[0];
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(last->operand[0]), MUMPS_INT))
			return FALSE;
	}
	/* If we have all literals, run at compile time and return the result */
	if ((OC_LIT == r->operand[0].oprval.tref->opcode)
		&& (OC_LIT == x.oprval.tref->opcode)
		&& (OC_ILIT == first->operand[0].oprval.tref->opcode)
		&& (OC_ILIT == last->operand[0].oprval.tref->opcode)
		&& (!gtm_utf8_mode
		|| (valid_utf_string(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str)
			&& valid_utf_string(&x.oprval.tref->operand[0].oprval.mlit->v.str))))
	{	/* We don't know how much space we will use; but we know it will be <= the size of the current string */
		if (scratch_space.len < r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len)
		{
			if (NULL != scratch_space.addr)
				free(scratch_space.addr);
			scratch_space.addr = malloc(r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len);
			scratch_space.len = r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len;
		}
		tmp_mval.str.addr = scratch_space.addr;
		if (!gtm_utf8_mode || (OC_FNZPIECE == op))
		{
			op_fnzpiece(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v,
				&x.oprval.tref->operand[0].oprval.mlit->v,
				first->operand[0].oprval.tref->operand[0].oprval.ilit,
				last->operand[0].oprval.tref->operand[0].oprval.ilit, &tmp_mval);
		} else
		{
			op_fnpiece(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v,
				&x.oprval.tref->operand[0].oprval.mlit->v,
				first->operand[0].oprval.tref->operand[0].oprval.ilit,
				last->operand[0].oprval.tref->operand[0].oprval.ilit, &tmp_mval);
		}
		s2pool(&tmp_mval.str);
		newop = (oprtype *)mcalloc(SIZEOF(oprtype));
		*newop = put_lit(&tmp_mval);			/* Copies mval so stack var tmp_mval not an issue */
		assert(TRIP_REF == newop->oprclass);
		newop->oprval.tref->src = r->src;
		*a = put_tref(newop->oprval.tref);
		return TRUE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
