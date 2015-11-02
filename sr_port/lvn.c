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
#include "subscript.h"
#include "advancewindow.h"

GBLREF char window_token;
GBLREF mident window_ident;

int lvn(oprtype *a,opctype index_op,triple *parent)
{
	oprtype subscripts[MAX_LVSUBSCRIPTS],*sb1,*sb2,*sb;
	triple *ref,*s, *root;
	char x;
	error_def(ERR_MAXNRSUBSCRIPTS);
	error_def(ERR_RPARENMISSING);
	error_def(ERR_VAREXPECTED);

	if (window_token != TK_IDENT)
	{
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	*a = put_mvar(&window_ident);
	advancewindow();
	if (window_token != TK_LPAREN)
		return TRUE;
	assert(a->oprclass == TRIP_REF);
	ref = a->oprval.tref;
	assert(ref->opcode == OC_VAR);
	sb1 = sb2 = subscripts;
	*sb1++ = *a;
	for (;;)
	{
		if (sb1 >= ARRAYTOP(subscripts))
		{
			stx_error(ERR_MAXNRSUBSCRIPTS);
			return FALSE;
		}
		advancewindow();
		if (!expr(sb1++))
			return FALSE;
		if ((x = window_token) == TK_RPAREN)
		{
			advancewindow();
			break;
		}
		if (x != TK_COMMA)
		{
			stx_error(ERR_RPARENMISSING);
			return FALSE;
		}
	}
	if (parent)
	{	/* only $ORDER, $NEXT, $ZPREV have parent */
		sb1--;
		if (sb1 - sb2 == 1)	/* only name and 1 subscript */
		{	/* SRCHINDX not necessary if only 1 subscript */
			sb = &parent->operand[1];  *sb = *sb1;
			return TRUE;
		}
	}

	root = ref = newtriple(index_op);
	ref->operand[0] = put_ilit((mint)(sb1 - sb2));
	while (sb2 < sb1)
	{
		s = newtriple(OC_PARAMETER);
		ref->operand[1] = put_tref(s);
		s->operand[0] = *sb2++;
		ref = s;
	}
	if (parent)
	{
		parent->operand[0] = put_tref(root);
		sb = &parent->operand[1];
		*sb = *sb2;
		return TRUE;
	}
	*a = put_tref(root);
	return TRUE;
}
