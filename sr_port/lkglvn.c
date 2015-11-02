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

#include "gtm_string.h"

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "subscript.h"
#include "advancewindow.h"

GBLREF char 	window_token;
GBLREF mident 	window_ident;

int lkglvn(bool gblvn)
{
	triple		*ref, *t1;
	char		x, lkname_buf[MAX_MIDENT_LEN + 1], *lknam;
	oprtype		subscripts[MAX_LVSUBSCRIPTS], *sb1, *sb2;
	opctype		ox;
	bool		vbar, parse_status;

	error_def(ERR_COMMA);
	error_def(ERR_EXTGBLDEL);
	error_def(ERR_MAXNRSUBSCRIPTS);
	error_def(ERR_LKNAMEXPECTED);

	ox = OC_LKNAME;
	sb1 = sb2 = subscripts;
	lknam = lkname_buf;
	if (gblvn)
		*lknam++ = '^';
	if ((TK_LBRACKET == window_token) || (TK_VBAR == window_token))
	{
		vbar = (TK_VBAR == window_token);
		advancewindow();
		if (vbar)
			parse_status = expr(sb1++);
		else
			parse_status = expratom(sb1++);
		if (!parse_status)
			return FALSE;
		if (window_token == TK_COMMA)
		{
			advancewindow();
			if (vbar)
				parse_status = expr(sb1++);
			else
				parse_status = expratom(sb1++);
			if (!parse_status)
			{
				return FALSE;
			}
		} else
			*sb1++ = put_str(0,0);
		if ((!vbar && (TK_RBRACKET != window_token)) || (vbar && (TK_VBAR != window_token)))
		{
			stx_error(ERR_EXTGBLDEL);
			return FALSE;
		}
		advancewindow();
		ox = OC_LKEXTNAME;
	} else
		*sb1++ = put_ilit(0);

	if (window_token != TK_IDENT)
	{	stx_error(ERR_LKNAMEXPECTED);
		return FALSE;
	}
	assert(window_ident.len <= MAX_MIDENT_LEN);
	memcpy(lknam, window_ident.addr, window_ident.len);
	lknam += window_ident.len;
	*sb1++ = put_str(lkname_buf,(mstr_len_t)(lknam - lkname_buf));
	advancewindow();
	if (window_token == TK_LPAREN)
	{
		for (;;)
		{
			if (sb1 >= ARRAYTOP(subscripts))
			{
				stx_error(ERR_MAXNRSUBSCRIPTS);
				return FALSE;
			}
			advancewindow();
			if (!expr(sb1))
				return FALSE;
			sb1++;
			if ((x = window_token) == TK_RPAREN)
			{
				advancewindow();
				break;
			}
			if (x != TK_COMMA)
			{
				stx_error(ERR_COMMA);
				return FALSE;
			}
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
	return TRUE;
}
