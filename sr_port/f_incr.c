/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "advancewindow.h"
#include "fullbool.h"
#include "show_source_line.h"

error_def(ERR_VAREXPECTED);

int f_incr(oprtype *a, opctype op)
{
	boolean_t	ok;
	oprtype		*increment;
	triple		incrchain, *oldchain, *r, *savptr, targchain, tmpexpr, *triptr, *triptr2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	/* may need to evaluate the increment (2nd arg) early and use result later: prepare to juggle triple chains */
	dqinit(&targchain, exorder);	/* a place for the operation and the target */
	dqinit(&tmpexpr, exorder);	/* a place to juggle the shifted chain in case it's active */
	triptr = maketriple(OC_NOOP);
	dqrins(&tmpexpr, exorder, triptr);
	triptr = TREF(expr_start);
	savptr = TREF(expr_start_orig);	/* but make sure expr_start_orig == expr_start since this is a new chain */
	assert(&tmpexpr != tmpexpr.exorder.bl);
	TREF(expr_start_orig) = TREF(expr_start) = &tmpexpr;
	oldchain = setcurtchain(&targchain);	/* save the result of the first argument 'cause it evaluates 2nd */
	switch (TREF(window_token))
	{
	case TK_IDENT:
		/* $INCREMENT() performs an implicit $GET() on a first argument lvn so we use OC_PUTINDX because
		 * we know only at runtime whether to signal an UNDEF error (depending on whether we have
		 * VIEW "NOUNDEF" or "UNDEF" state; op_putindx creates the local variable unconditionally, even if
		 * we have "UNDEF" state, in which case any error in op_fnincr causes an op_kill of that local variable
		 */
		ok = (lvn(&(r->operand[0]), OC_PUTINDX, 0));
		break;
	case TK_CIRCUMFLEX:
		ok = gvn();
		r->opcode = OC_GVINCR;
		r->operand[0] = put_ilit(0);	/* dummy fill since emit_code does not like empty operand[0] */
		break;
	case TK_ATSIGN:
		ok = indirection(&r->operand[0]);
		r->opcode = OC_INDINCR;
		break;
	default:
		ok = FALSE;
		break;
	}
	if (!ok)
	{
		setcurtchain(oldchain);
		return FALSE;
	}
	TREF(expr_start) = triptr;				/* restore original shift chain */
	TREF(expr_start_orig) = savptr;
	increment = &r->operand[1];
	if (TK_COMMA != TREF(window_token))
		*increment = put_ilit(1);	/* default optional increment to 1 */
	else
	{
		dqinit(&incrchain, exorder);	/* a place for the increment */
		setcurtchain(&incrchain);	/* increment expr must evaluate before the glvn in $INCR(glvn,expr) */
		advancewindow();
		if (EXPR_FAIL == expr(increment, MUMPS_NUM))
		{
			setcurtchain(oldchain);
			return FALSE;
		}
		setcurtchain(&targchain);
		dqadd(&targchain, &incrchain, exorder);	/* dir before targ - this is a violation of info hiding */
	}
	coerce(increment, OCT_MVAL);
	ins_triple(r);
	if (&tmpexpr != tmpexpr.exorder.bl->exorder.bl)
	{	/* one or more OC_GVNAME may have shifted so add to the end of the shift chain */
		assert(TREF(shift_side_effects));
		assert(&tmpexpr != tmpexpr.exorder.bl);
		dqadd(TREF(expr_start), &tmpexpr, exorder);	/* this is a violation of info hiding */
		TREF(expr_start) = tmpexpr.exorder.bl;
		if (OC_GVSAVTARG == (TREF(expr_start))->opcode)
		{
			triptr2 = newtriple(OC_GVRECTARG);	/* restore the result of the last gvn to preserve $referece (the naked) */
			triptr2->operand[0] = put_tref(TREF(expr_start));
		} else
			assert(OC_NOOP == (TREF(expr_start))->opcode);
	}
	if (!TREF(shift_side_effects) || (YDB_BOOL != TREF(ydb_fullbool)) || (OC_INDINCR != r->opcode)
		|| (NULL == TREF(expr_start)) || (NULL == triptr))
	{	/* Put it on the end of the main chain as there's no reason to play more with the ordering */
		/* Note: The reason for the "(NULL == triptr)" check in the above "if" is the following.
		 * If "triptr" is NULL, it means "TREF(expr_start)" was NULL before the "expr()" call above.
		 * If "expr()" involved a boolean expression evaluation, then "TREF(expr_start)" could have gotten
		 * set to a non-NULL value. In that case, "TREF(expr_start()" would point to a triple in the "incrchain"
		 * (which has already been merged into "targchain") and so the "dqadd(TREF(expr_start), &targchain, ...)"
		 * call in the "else" block below cannot be done as "dqadd()" assumes that "TREF(expr_start)" and "targchain"
		 * are non-intersecting chains. Hence we treat a NULL value of "triptr" as if "TREF(expr_start)" is NULL
		 * and fall through into the "if" block in that case.
		 */
		setcurtchain(oldchain);
		triptr = (TREF(curtchain))->exorder.bl;
		dqadd(triptr, &targchain, exorder);	/* this is a violation of info hiding */
	} else	/* need full side effects or indirect 1st argument so put everything on the shift chain */
	{	/* add the chain after "expr_start" which may be much before "curtchain" */
		newtriple(OC_GVSAVTARG);
		setcurtchain(oldchain);
		assert(&targchain != targchain.exorder.bl);
		dqadd(TREF(expr_start), &targchain, exorder);	/* this is a violation of info hiding */
		TREF(expr_start) = targchain.exorder.bl;
		assert(OC_GVSAVTARG == (TREF(expr_start))->opcode);
		triptr = newtriple(OC_GVRECTARG);	/* restore the result of the last gvn to preserve $referece (the naked) */
		triptr->operand[0] = put_tref(TREF(expr_start));
	}
	/* $increment() args need to avoid side effect processing but that's handled in expritem so eval_expr gets $i()'s SE flag */
	*a = put_tref(r);
	return TRUE;
}
