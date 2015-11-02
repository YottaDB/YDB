/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "mdq.h"
#include "advancewindow.h"

GBLREF char		window_token;
GBLREF mident		window_ident;

int gvn(void)
{
	triple		*ref, *t1, *oldchain, tmpchain, *triptr, *s;
	oprtype		subscripts[MAX_GVSUBSCRIPTS], *sb1, *sb2;
	boolean_t	shifting, vbar, parse_status;
	opctype		ox;
	char		x;
	error_def(ERR_MAXNRSUBSCRIPTS);
	error_def(ERR_RPARENMISSING);
	error_def(ERR_GBLNAME);
	error_def(ERR_EXTGBLDEL);
	error_def(ERR_GVNAKEDEXTNM);
	error_def(ERR_EXPR);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(window_token == TK_CIRCUMFLEX);
	advancewindow();
	sb1 = sb2 = subscripts;
	ox = 0;
	if (shifting = TREF(shift_side_effects))
	{
		dqinit(&tmpchain, exorder);
		oldchain = setcurtchain(&tmpchain);
	}
	if (window_token == TK_LBRACKET || window_token == TK_VBAR)
	{	vbar = (window_token == TK_VBAR);
		advancewindow();
		if (vbar)
			parse_status = expr(sb1++);
		else
			parse_status = expratom(sb1++);
		if (!parse_status)
		{	stx_error(ERR_EXPR);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (window_token == TK_COMMA)
		{
			advancewindow();
			if (vbar)
				parse_status = expr(sb1++);
			else
				parse_status = expratom(sb1++);
			if (!parse_status)
			{	stx_error(ERR_EXPR);
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
		} else
			*sb1++ = put_str(0,0);
		if ((!vbar && window_token != TK_RBRACKET) || (vbar && window_token != TK_VBAR))
		{	stx_error(ERR_EXTGBLDEL);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		advancewindow();
		ox = OC_GVEXTNAM;
	}
	if (window_token == TK_IDENT)
	{
		if (!ox)
			ox = OC_GVNAME;
		*sb1++ = put_str(window_ident.addr, window_ident.len);
		advancewindow();
	} else
	{	if (ox)
		{
			stx_error(ERR_GVNAKEDEXTNM);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (window_token != TK_LPAREN)
		{
			stx_error(ERR_GBLNAME);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		ox = OC_GVNAKED;
	}
	if (window_token == TK_LPAREN)
		for (;;)
		{
			if (sb1 >= ARRAYTOP(subscripts))
			{
				stx_error(ERR_MAXNRSUBSCRIPTS);
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
			advancewindow();
			if (!expr(sb1))
			{
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
			assert(sb1->oprclass == TRIP_REF);
			s = sb1->oprval.tref;
			if (s->opcode == OC_LIT)
				*sb1 = make_gvsubsc(&s->operand[0].oprval.mlit->v);
			sb1++;
			if ((x = window_token) == TK_RPAREN)
			{
				advancewindow();
				break;
			}
			if (x != TK_COMMA)
			{
				stx_error(ERR_RPARENMISSING);
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
		}
	ref = newtriple(ox);
	ref->operand[0] = put_ilit((mint)(sb1 - sb2));
	for ( ; sb2 < sb1 ; sb2++)
	{
		t1 = newtriple(OC_PARAMETER);
		ref->operand[1] = put_tref(t1);
		ref = t1;
		ref->operand[0] = *sb2;
	}
	if (shifting)
	{
		newtriple(OC_GVSAVTARG);
		setcurtchain(oldchain);
		dqadd(TREF(expr_start), &tmpchain, exorder);
		TREF(expr_start) = tmpchain.exorder.bl;
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(TREF(expr_start));
	}
	return TRUE;
}
