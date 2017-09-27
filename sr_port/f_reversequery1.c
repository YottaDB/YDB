/****************************************************************
 *								*
 * Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Code in this module is based on f_query.c and hence has an
 * FIS copyright even though this module was not created by FIS.
 */

#include "mdef.h"

#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "mmemory.h"
#include "fullbool.h"
#include "fnorder.h"
#include "advancewindow.h"
#include "glvn_pool.h"

error_def(ERR_VAREXPECTED);

/* This function is basically a 1-argument $query call where the direction (2nd argument) is known as reverse (i.e. reverse query).
 *
 * In case of $query(@x,-1), where the first argument is an indirection, "f_query" does not know what opcode to generate
 * (OC_FNQUERY or OC_GVQUERY). But since the direction "-1" is a literal known at compile time, "f_query" knows this is
 * a reverse query (not a forward query) and hence generates an OC_INDFUN triple with an indirection opcode indir_fnreversequery1.
 * The OC_INDFUN triple causes "op_indfun" to be invoked. That invokes the function "f_reversequery1" (because of
 * indir_fnreversequery1). Assuming the variable "x" evaluated to "y", at the time of "f_reversequery1" invocation, we have
 * reduced the original $query(@x,-1) function invocation to a $query(y,-1) i.e. a 2-argument $query to a 1-argument $query
 * where the second argument is known to be -1 (i.e. reverse query). So "f_reversequery1" is very similar to "f_query" in that
 * it needs to still compile a $query() call but it is a lot simpler due to no 2nd argument.
 * Any changes here might need to be made in "f_query.c" and vice versa.
 */
int f_reversequery1(oprtype *a, opctype op)
{
	triple		*oldchain, *r, *r0, *r1;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TK_IDENT == TREF(window_token))
	{
		if (!lvn(a, OC_FNREVERSEQUERY, 0))
			return FALSE;
		assert(TRIP_REF == a->oprclass);
		if (OC_FNREVERSEQUERY == a->oprval.tref->opcode)
		{	/* See comments in "f_query.c" for why the +2 and chain manipulations done below */
			assert(OC_FNREVERSEQUERY == a->oprval.tref->opcode);
			assert(TRIP_REF == a->oprval.tref->operand[0].oprclass);
			assert(OC_ILIT == a->oprval.tref->operand[0].oprval.tref->opcode);
			assert(ILIT_REF == a->oprval.tref->operand[0].oprval.tref->operand[0].oprclass);
			assert(0 < a->oprval.tref->operand[0].oprval.tref->operand[0].oprval.ilit);
			a->oprval.tref->operand[0].oprval.tref->operand[0].oprval.ilit += 2;
			assert(TRIP_REF == a->oprval.tref->operand[1].oprclass);
			assert(OC_PARAMETER == a->oprval.tref->operand[1].oprval.tref->opcode);
			assert(TRIP_REF == a->oprval.tref->operand[1].oprval.tref->operand[0].oprclass);
			r0 = a->oprval.tref->operand[1].oprval.tref->operand[0].oprval.tref;
			assert(OC_VAR == r0->opcode);
			assert(MVAR_REF == r0->operand[0].oprclass);
			r1 = maketriple(OC_PARAMETER);
			r1->operand[0] = put_str(r0->operand[0].oprval.vref->mvname.addr, r0->operand[0].oprval.vref->mvname.len);
			r1->operand[1] = a->oprval.tref->operand[1];
			a->oprval.tref->operand[1] = put_tref (r1);
			dqins (a->oprval.tref->exorder.fl, exorder, r1);
		} else
		{
			assert(OC_VAR == a->oprval.tref->opcode);
			r0 = newtriple(OC_FNREVERSEQUERY);
			r0->operand[0] = put_ilit (3);
			r0->operand[1] = put_tref(newtriple(OC_PARAMETER));
			r0->operand[1].oprval.tref->operand[0] = put_str(a->oprval.tref->operand[0].oprval.vref->mvname.addr,
									a->oprval.tref->operand[0].oprval.vref->mvname.len);
			r1 = r0->operand[1].oprval.tref;
			r1->operand[1] = *a;
			*a = put_tref (r0);
		}
	} else
	{
		r = maketriple(op);
		switch (TREF(window_token))
		{
		case TK_CIRCUMFLEX:
			if (!gvn())
				return FALSE;
			r->opcode = OC_GVREVERSEQUERY;
			ins_triple(r);
			break;
		case TK_ATSIGN:
			if (SHIFT_SIDE_EFFECTS)
			{
				START_GVBIND_CHAIN(&save_state, oldchain);
				if (!indirection(&(r->operand[0])))
				{
					setcurtchain(oldchain);
					return FALSE;
				}
				r->operand[1] = put_ilit((mint)indir_fnreversequery1);
				ins_triple(r);
				PLACE_GVBIND_CHAIN(&save_state, oldchain);
			} else
			{
				if (!indirection(&(r->operand[0])))
					return FALSE;
				r->operand[1] = put_ilit((mint)indir_fnreversequery1);
				ins_triple(r);
			}
			r->opcode = OC_INDFUN;
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		*a = put_tref(r);
	}
	return TRUE;
}
