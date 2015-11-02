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
#include "opcode.h"
#include "toktyp.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "subscript.h"

error_def(ERR_VAREXPECTED);

int f_name(oprtype *a, opctype op)
{
	boolean_t	gbl;
	oprtype		*depth;
	triple		*r;
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
		r->opcode = OC_INDFNNAME;
		if (!indirection(&(r->operand[0])))
			return FALSE;
		depth = &r->operand[1];
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
		advancewindow();
		if (EXPR_FAIL == expr(depth, MUMPS_STR))
			return FALSE;
	}
	coerce(depth, OCT_MVAL);
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
