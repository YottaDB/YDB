/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "advancewindow.h"

GBLREF char window_token;

int f_zjobexam(oprtype *a, opctype op)
{
	triple *r;

	r = maketriple(op);
	if (TK_RPAREN == window_token)
	{	/* No argument specified - default to null */
		r->operand[0] = put_str("",0);
	} else if (!strexpr(&(r->operand[0])))
		return FALSE;	/* Improper string argument */
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
