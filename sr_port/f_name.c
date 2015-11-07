/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "indir_enum.h"
#include "advancewindow.h"
#include "subscript.h"
#include "fullbool.h"
#include "show_source_line.h"

GBLREF	boolean_t	run_time;
GBLREF	short int	source_column;

error_def(ERR_SIDEEFFECTEVAL);
error_def(ERR_VAREXPECTED);

int f_name(oprtype *a, opctype op)
{
	boolean_t	gbl;
	oprtype		*depth;
	short int	column;
	triple		*r, *s;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	gbl = FALSE;
	switch (TREF(window_token))
	{
	case TK_CIRCUMFLEX:
		gbl = TRUE;
		advancewindow();
		/* caution fall through */
	case TK_IDENT:
		if (!name_glvn(gbl, &r->operand[1]))
			return FALSE;
		depth = &r->operand[0];
		break;
	case TK_ATSIGN:
		r->opcode = OC_INDFNNAME2;			/* chomps extra subscripts of resulting string */
		s = maketriple(OC_INDFNNAME);
		if (!indirection(&(s->operand[0])))
			return FALSE;
		s->operand[1] = put_ilit(MAX_LVSUBSCRIPTS + 1);	/* first, get all the subscripts. r will chomp them */
		coerce(&s->operand[1], OCT_MVAL);
		ins_triple(s);
		depth = &r->operand[0];
		r->operand[1] = put_tref(s);
		break;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	/* allow for optional default value */
	if (TK_COMMA != TREF(window_token))
	{
		*depth = put_ilit(MAX_LVSUBSCRIPTS + 1);	/* default to maximum number of subscripts allowed by law */
		/* ideally this should be MAX(MAX_LVSUBSCRIPTS, MAX_GVSUBSCRIPTS) but they are the same so take the easy path */
		assert(MAX_LVSUBSCRIPTS == MAX_GVSUBSCRIPTS);	/* add assert to ensure our assumption is valid */
	} else
	{
		DISABLE_SIDE_EFFECT_AT_DEPTH;		/* doing this here let's us know specifically if direction had SE threat */
		advancewindow();
		column = source_column;
		if (EXPR_FAIL == expr(depth, MUMPS_STR))
			return FALSE;
		if (!run_time && (OC_INDFNNAME2 == r->opcode) && (SE_WARN == TREF(side_effect_handling)))
			ISSUE_SIDEEFFECTEVAL_WARNING(column - 1);
	}
	coerce(depth, OCT_MVAL);
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
