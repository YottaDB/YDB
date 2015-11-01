/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

GBLREF char window_token;

int f_name(oprtype *a, opctype op)
{
	triple *r;
	oprtype	*depth;
	bool	gbl;
	error_def(ERR_VAREXPECTED);

	r = maketriple(op);
	gbl = FALSE;
	switch (window_token)
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
	if (window_token != TK_COMMA)
		*depth = put_ilit(MAXPOSINT4);		/* default to largest positive number allowed by law */
	else
	{
		advancewindow();
		if (!intexpr(depth))
			return FALSE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
