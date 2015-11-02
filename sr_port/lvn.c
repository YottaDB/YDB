/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "fullbool.h"
#include "opcode.h"
#include "mdq.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "show_source_line.h"

GBLREF	boolean_t	run_time;

error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_RPARENMISSING);
error_def(ERR_VAREXPECTED);
error_def(ERR_SIDEEFFECTEVAL);

int lvn(oprtype *a, opctype index_op, triple *parent)
{
	char		x;
	oprtype		*sb, *sb1, *sb2, subscripts[MAX_LVSUBSCRIPTS];
	triple 		*ref, *root;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(window_token) != TK_IDENT)
	{
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	*a = put_mvar(&(TREF(window_ident)));
	advancewindow();
	if (TK_LPAREN != TREF(window_token))
		return TRUE;
	assert(TRIP_REF == a->oprclass);
	DEBUG_ONLY(ref = a->oprval.tref);
	assert(OC_VAR == ref->opcode);
	sb1 = sb2 = subscripts;
	*sb1++ = *a;
	for (;;)
	{
		if (ARRAYTOP(subscripts) <= sb1)
		{
			stx_error(ERR_MAXNRSUBSCRIPTS);
			return FALSE;
		}
		advancewindow();
		if (EXPR_FAIL == expr(sb1++, MUMPS_EXPR))
			return FALSE;
		if (TK_RPAREN == (x = TREF(window_token)))	/* NOTE assignment */
		{
			advancewindow();
			break;
		}
		if (TK_COMMA != x)
		{
			stx_error(ERR_RPARENMISSING);
			return FALSE;
		}
	}
	if (parent)
	{	/* only $ORDER, $NEXT, $ZPREV have parent */
		sb1--;
		if ((sb1 - sb2) == 1)	/* only name and 1 subscript */
		{	/* SRCHINDX not necessary if only 1 subscript */
			sb = &parent->operand[1];
			*sb = *sb1;
			return TRUE;
		}
	}
	root = ref = newtriple(index_op);
	ref->operand[0] = put_ilit((mint)(sb1 - sb2));
	SUBS_ARRAY_2_TRIPLES(ref, sb1, sb2, subscripts, 0);
	if (parent)
	{
		parent->operand[0] = put_tref(root);
		sb = &parent->operand[1];
		*sb = *sb1;
		return TRUE;
	}
	*a = put_tref(root);
	return TRUE;
}
