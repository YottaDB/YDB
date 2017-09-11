/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "mdq.h"
#include "subscript.h"
#include "advancewindow.h"
#include "fullbool.h"
#include "show_source_line.h"

GBLREF	boolean_t	run_time;

error_def(ERR_EXPR);
error_def(ERR_EXTGBLDEL);
error_def(ERR_LKNAMEXPECTED);
error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_RPARENMISSING);
error_def(ERR_SIDEEFFECTEVAL);

int lkglvn(boolean_t gblvn)
{
	boolean_t	shifting, vbar;
	char		*lknam, lkname_buf[MAX_MIDENT_LEN + 1], x;
	opctype		ox;
	oprtype		*sb1, *sb2, subscripts[MAX_GVSUBSCRIPTS + 1];
	triple		*oldchain, *ref, tmpchain, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ox = OC_LKNAME;
	sb1 = sb2 = subscripts;
	lknam = lkname_buf;
	if (gblvn)
		*lknam++ = '^';
	if (shifting = (TREF(shift_side_effects) && (!TREF(saw_side_effect) || (GTM_BOOL == TREF(gtm_fullbool)
		&& (OLD_SE == TREF(side_effect_handling))))))
	{	/* NOTE assignment above */
		dqinit(&tmpchain, exorder);
		oldchain = setcurtchain(&tmpchain);
	}
	if ((TK_LBRACKET == TREF(window_token)) || (TK_VBAR == TREF(window_token)))
	{
		vbar = (TK_VBAR == TREF(window_token));
		advancewindow();
		if (EXPR_FAIL == (vbar ? expr(sb1++, MUMPS_EXPR) : expratom(sb1++)))
		{
			stx_error(ERR_EXPR);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (TK_COMMA == TREF(window_token))
		{
			advancewindow();
			if (EXPR_FAIL == (vbar ? expr(sb1++, MUMPS_EXPR) : expratom(sb1++)))
			{
				stx_error(ERR_EXPR);
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
		} else
			*sb1++ = put_str(0, 0);
		if ((!vbar && (TK_RBRACKET != TREF(window_token))) || (vbar && (TK_VBAR != TREF(window_token))))
		{
			stx_error(ERR_EXTGBLDEL);
			if (shifting)
				setcurtchain(oldchain);
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
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
			advancewindow();
			if (EXPR_FAIL == expr(sb1, MUMPS_EXPR))
			{
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
			sb1++;
			if (TK_RPAREN == (x = TREF(window_token)))	/* NOTE assignment */
			{
				advancewindow();
				break;
			}
			if (TK_COMMA != x)
			{
				stx_error(ERR_RPARENMISSING);
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
		}
	}
	ref = newtriple(ox);
	ref->operand[0] = put_ilit((mint)(sb1 - sb2));
	SUBS_ARRAY_2_TRIPLES(ref, sb1, sb2, subscripts, 0);
	if (shifting)
	{
		if (TREF(saw_side_effect) && ((GTM_BOOL != TREF(gtm_fullbool)) || (OLD_SE != TREF(side_effect_handling))))
		{	/* saw a side effect in a subscript - time to stop shifting */
			setcurtchain(oldchain);
			triptr = (TREF(curtchain))->exorder.bl;
			dqadd(triptr, &tmpchain, exorder);
		} else
		{
			newtriple(OC_GVSAVTARG);
			setcurtchain(oldchain);
			dqadd(TREF(expr_start), &tmpchain, exorder);
			TREF(expr_start) = tmpchain.exorder.bl;
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(TREF(expr_start));
		}
	}
	return TRUE;
}
