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

#include "gtm_string.h"

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "subscript.h"
#include "advancewindow.h"

error_def(ERR_COMMA);
error_def(ERR_EXTGBLDEL);
error_def(ERR_LKNAMEXPECTED);
error_def(ERR_MAXNRSUBSCRIPTS);

int lkglvn(boolean_t gblvn)
{
	boolean_t	vbar;
	char		*lknam, lkname_buf[MAX_MIDENT_LEN + 1], x;
	opctype		ox;
	oprtype		*sb1, *sb2, subscripts[MAX_LVSUBSCRIPTS];
	triple		*ref, *t1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ox = OC_LKNAME;
	sb1 = sb2 = subscripts;
	lknam = lkname_buf;
	if (gblvn)
		*lknam++ = '^';
	if ((TK_LBRACKET == TREF(window_token)) || (TK_VBAR == TREF(window_token)))
	{
		vbar = (TK_VBAR == TREF(window_token));
		advancewindow();
		if (EXPR_FAIL == (vbar ? expr(sb1++, MUMPS_EXPR) : expratom(sb1++)))
			return FALSE;
		if (TK_COMMA == TREF(window_token))
		{
			advancewindow();
			if (EXPR_FAIL == (vbar ? expr(sb1++, MUMPS_EXPR) : expratom(sb1++)))
				return FALSE;
		} else
			*sb1++ = put_str(0, 0);
		if ((!vbar && (TK_RBRACKET != TREF(window_token))) || (vbar && (TK_VBAR != TREF(window_token))))
		{
			stx_error(ERR_EXTGBLDEL);
			return FALSE;
		}
		advancewindow();
		ox = OC_LKEXTNAME;
	} else
		*sb1++ = put_ilit(0);
	if (TK_IDENT != TREF(window_token))
	{
		stx_error(ERR_LKNAMEXPECTED);
		return FALSE;
	}
	assert(MAX_MIDENT_LEN >= (TREF(window_ident)).len);
	memcpy(lknam, (TREF(window_ident)).addr, (TREF(window_ident)).len);
	lknam += (TREF(window_ident)).len;
	*sb1++ = put_str(lkname_buf,(mstr_len_t)(lknam - lkname_buf));
	advancewindow();
	if (TK_LPAREN == TREF(window_token))
	{
		for (;;)
		{
			if (ARRAYTOP(subscripts) <= sb1)
			{
				stx_error(ERR_MAXNRSUBSCRIPTS);
				return FALSE;
			}
			advancewindow();
			if (EXPR_FAIL == expr(sb1, MUMPS_EXPR))
				return FALSE;
			sb1++;
			if (TK_RPAREN == (x = TREF(window_token)))	/* NOTE assignment */
			{
				advancewindow();
				break;
			}
			if (TK_COMMA != x)
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
