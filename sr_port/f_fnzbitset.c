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
#include "advancewindow.h"

GBLREF char window_token;

int f_fnzbitset( oprtype *a, opctype op)
{
	triple *r, *parm;
	error_def(ERR_COMMA);

	r = maketriple(op);
	if (!expr(&(r->operand[0])))      /* bitstring */
		return FALSE;
	if (window_token != TK_COMMA)
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	parm = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(parm);
	advancewindow();
	if (!intexpr(&(parm->operand[0])))    /* position  */
		return FALSE;
	if (window_token != TK_COMMA)
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	if (!intexpr(&(parm->operand[1])))    /* truthval  */
		return FALSE;

	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
