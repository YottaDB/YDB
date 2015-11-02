/****************************************************************
 *								*
 *	Copyright 2004 Sanchez Computer Associates, Inc.	*
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
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "advancewindow.h"

GBLREF char	window_token;
GBLREF triple	*curtchain;
GBLREF bool	shift_gvrefs;
GBLREF triple	*expr_start;

int f_incr(oprtype *a, opctype op)
{
	triple		*r, tmpchain, *triptr, *oldchain;
	opctype		incr_oc;
	oprtype		*increment;

	error_def(ERR_VAREXPECTED);

	r = maketriple(op);
	/* we need to evaluate the "glvn" part after "expr" in $INCR("glvn","expr"). do appropriate triple chain switching */
	dqinit(&tmpchain, exorder);
	oldchain = setcurtchain(&tmpchain);
	switch (window_token)
	{
	case TK_IDENT:
		/* we need to use OC_PUTINDX below as we will know only at runtime whether to signal an UNDEF error
		 * (depending on whether view "NOUNDEF" or "UNDEF" is on. the way we do this now is that op_putindx
		 * will create the local variable unconditionally (even if view "UNDEF" is turned on) and later
		 * op_fnincr will do an op_kill of that local variable if necessary.
		 */
		if (!lvn(&(r->operand[0]), OC_PUTINDX, 0))
		{
			setcurtchain(oldchain);
			return FALSE;
		}
		break;
	case TK_CIRCUMFLEX:
		if (!gvn())
		{
			setcurtchain(oldchain);
			return FALSE;
		}
		r->opcode = OC_GVINCR;
		r->operand[0] = put_ilit(0);	/* dummy fill since emit_code does not like empty operand[0] */
		break;
	case TK_ATSIGN:
		r->opcode = OC_INDINCR;
		if (!indirection(&r->operand[0]))
		{
			setcurtchain(oldchain);
			return FALSE;
		}
		break;
	default:
		setcurtchain(oldchain);
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	setcurtchain(oldchain);
	/* allow for optional default value */
	increment = &r->operand[1];
	if (window_token != TK_COMMA)
		*increment = put_ilit(1);		/* default increment value to 1 */
	else
	{
		advancewindow();
		if (!strexpr(increment))
			return FALSE;
	}
	coerce(increment, OCT_MVAL);
	if ((OC_INDINCR != r->opcode) || !shift_gvrefs)
	{
		triptr = curtchain->exorder.bl;
		dqadd(triptr, &tmpchain, exorder); /* add the "glvn" chains at the tail of the "expr" chains */
		ins_triple(r);
	} else
	{	/* add the indirection triple chains immediately after "expr_start" which is possibly much before "curtchain" */
		oldchain = setcurtchain(&tmpchain);
		ins_triple(r);
		newtriple(OC_GVSAVTARG);
		setcurtchain(oldchain);
		dqadd(expr_start, &tmpchain, exorder);
		expr_start = tmpchain.exorder.bl;
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(expr_start);
	}
	*a = put_tref(r);
	return TRUE;
}
