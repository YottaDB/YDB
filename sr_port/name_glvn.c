/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "fnname.h"
#include "advancewindow.h"

GBLREF char 	window_token;
GBLREF mident 	window_ident;

int name_glvn(bool gblvn, oprtype *a)
{
	triple 	*ref, *t1, *t2;
	char	x;
	/* Note:  MAX_LVSUBSCRIPTS and MAX_GVSUBSCRIPTS are currently equal.  Should that change,
			this should also change */
	oprtype subscripts[MAX_LVSUBSCRIPTS + 1], *sb1, *sb2;
	int	fnname_type;
	bool vbar, parse_status;
	error_def(ERR_COMMA);
	error_def(ERR_GVNAKEDEXTNM);
	error_def(ERR_EXTGBLDEL);
	error_def(ERR_MAXNRSUBSCRIPTS);
	error_def(ERR_GBLNAME);

	sb1 = sb2 = subscripts;
	sb1++;						/* save room for type indicator */
	if (gblvn)
	{	fnname_type = FNGBL;
		if (window_token == TK_LBRACKET || window_token == TK_VBAR)
		{
			vbar = (window_token == TK_VBAR);
			if (vbar)
				fnname_type |= FNVBAR;
			advancewindow();
			if (vbar)
				parse_status = expr(sb1++);
			else
				parse_status = expratom(sb1++);
			if (!parse_status)
			{
				return FALSE;
			}
			if (window_token != TK_COMMA)
				fnname_type |= FNEXTGBL1;
			else
			{
				fnname_type |= FNEXTGBL2;
				advancewindow();
				if (vbar)
					parse_status = expr(sb1++);
				else
					parse_status = expratom(sb1++);
				if (!parse_status)
				{
					return FALSE;
				}
			}
			if ((!vbar && window_token != TK_RBRACKET) || (vbar && window_token != TK_VBAR))
			{
				stx_error(ERR_EXTGBLDEL);
				return FALSE;
			}
			advancewindow();
		}
	}
	else
		fnname_type = FNLCL;

	if (window_token != TK_IDENT)
	{
		assert(fnname_type & FNGBL);
		if (fnname_type != FNGBL)
		{
			stx_error(ERR_GVNAKEDEXTNM);
			return FALSE;
		}
		if (window_token != TK_LPAREN)
		{
			stx_error(ERR_GBLNAME);
			return FALSE;
		}
		fnname_type = FNNAKGBL;
	}
	else
	{
		*sb1++ = put_str(window_ident.addr, window_ident.len);
		advancewindow();
	}
	if (window_token == TK_LPAREN)
	{	for (;;)
		{	if (sb1 - sb2 > MAX_GVSUBSCRIPTS)
			{	stx_error(ERR_MAXNRSUBSCRIPTS);
				return FALSE;
			}
			advancewindow();
			if (!expr(sb1))
			{	return FALSE;
			}
			sb1++;
			if ((x = window_token) == TK_RPAREN)
			{	advancewindow();
				break;
			}
			if (x != TK_COMMA)
			{	stx_error(ERR_COMMA);
				return FALSE;
			}
		}
	}
	subscripts[0] = put_ilit(fnname_type);
	ref = t1 = newtriple(OC_PARAMETER);
	ref->operand[0] = put_ilit((mint)(sb1 - sb2 + 2)); /* # of subscripts + dst + depth argument (determine at f_name) */
	for ( ; sb2 < sb1 ; sb2++)
	{
		t2 = newtriple(OC_PARAMETER);
		t1->operand[1] = put_tref(t2);
		t1 = t2;
		t1->operand[0] = *sb2;
	}
	*a = put_tref(ref);
	return TRUE;
}
