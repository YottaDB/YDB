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
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "mdq.h"
#include "advancewindow.h"
#include "fullbool.h"
#include "show_source_line.h"
#include "hashtab_mname.h"

GBLREF	boolean_t	run_time;

error_def(ERR_EXPR);
error_def(ERR_EXTGBLDEL);
error_def(ERR_GBLNAME);
error_def(ERR_GVNAKEDEXTNM);
error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_RPARENMISSING);
error_def(ERR_SIDEEFFECTEVAL);

int gvn(void)
{
	boolean_t	parse_status, shifting, vbar;
	char		x;
	int		hash_code;
	opctype		ox;
	oprtype		*sb1, *sb2, subscripts[MAX_GVSUBSCRIPTS + 1];
	triple		*oldchain, *ref, *s, tmpchain, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TK_CIRCUMFLEX == TREF(window_token));
	advancewindow();
	sb1 = sb2 = subscripts;
	ox = 0;
	if (shifting = (TREF(shift_side_effects) && (!TREF(saw_side_effect) || (GTM_BOOL == TREF(gtm_fullbool)
		&& (OLD_SE == TREF(side_effect_handling))))))
	{	/* NOTE assignment above */
		dqinit(&tmpchain, exorder);
		oldchain = setcurtchain(&tmpchain);
	}
	if ((TK_LBRACKET == TREF(window_token)) || (TK_VBAR == TREF(window_token)))
	{
		assert(sb2 == sb1);
		/* set "hash_code" as the first operand so OC_GVEXTNAM has it passed in at same spot as op_gvname */
		sb1++;
		vbar = (TK_VBAR == TREF(window_token));
		advancewindow();
		if (vbar)
			parse_status = expr(sb1++, MUMPS_EXPR);
		else
			parse_status = expratom(sb1++);
		if (!parse_status)
		{
			stx_error(ERR_EXPR);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (TK_COMMA == TREF(window_token))
		{
			advancewindow();
			if (vbar)
				parse_status = expr(sb1++, MUMPS_EXPR);
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
		if ((!vbar && (TK_RBRACKET != TREF(window_token))) || (vbar && (TK_VBAR != TREF(window_token))))
		{
			stx_error(ERR_EXTGBLDEL);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		advancewindow();
		ox = OC_GVEXTNAM;
	}
	if (TK_IDENT == TREF(window_token))
	{
		COMPUTE_HASH_MSTR((TREF(window_ident)), hash_code);
		if (!ox)
		{
			ox = OC_GVNAME;
			*sb1++ = put_ilit((mint)hash_code);
		} else
			*sb2 = put_ilit((mint)hash_code);	/* fill in hash_code in the space previously set aside */
		*sb1++ = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
		advancewindow();
	} else
	{	if (ox)
		{
			stx_error(ERR_GVNAKEDEXTNM);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (TK_LPAREN != TREF(window_token))
		{
			stx_error(ERR_GBLNAME);
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		ox = OC_GVNAKED;
		/* pass in a dummy hash_code in case of OC_GVNAKED. We need this so op_gvname_fast, op_gvextnam_fast and
		 * op_gvnaked_fast have the same call interface. op_savgvn.c relies on this to replace OC_GVNAME, OC_GVEXTNAM
		 * or OC_GVNAKED opcodes with a OC_SAVGVN opcode.
		 */
		*sb1++ = put_ilit((mint)0);
	}
	if (TK_LPAREN == TREF(window_token))
	{
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
			if (EXPR_FAIL == expr(sb1, MUMPS_EXPR))
			{
				if (shifting)
					setcurtchain(oldchain);
				return FALSE;
			}
			assert(TRIP_REF == sb1->oprclass);
			s = sb1->oprval.tref;
			if (OC_LIT == s->opcode)
				*sb1 = make_gvsubsc(&s->operand[0].oprval.mlit->v);
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
