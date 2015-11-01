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
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"

GBLREF bool	shift_gvrefs;
GBLREF char	window_token;
GBLREF triple	*expr_start;

int f_query ( oprtype *a, opctype op)
{
	triple		*oldchain, tmpchain, *r, *r0, *r1, *triptr;
	error_def	(ERR_VAREXPECTED);

	if (window_token == TK_IDENT)
	{
		if (!lvn (a, OC_FNQUERY, 0))
			return FALSE;
		assert (a->oprclass == TRIP_REF);
		if (a->oprval.tref->opcode == OC_FNQUERY)
		{
			assert (a->oprval.tref->opcode == OC_FNQUERY);
			assert (a->oprval.tref->operand[0].oprclass == TRIP_REF);
			assert (a->oprval.tref->operand[0].oprval.tref->opcode == OC_ILIT);
			assert (a->oprval.tref->operand[0].oprval.tref->operand[0].oprclass == ILIT_REF);
			assert (a->oprval.tref->operand[0].oprval.tref->operand[0].oprval.ilit > 0);
			a->oprval.tref->operand[0].oprval.tref->operand[0].oprval.ilit += 2;
			assert (a->oprval.tref->operand[1].oprclass == TRIP_REF);
			assert (a->oprval.tref->operand[1].oprval.tref->opcode == OC_PARAMETER);
			assert (a->oprval.tref->operand[1].oprval.tref->operand[0].oprclass == TRIP_REF);
			r0 = a->oprval.tref->operand[1].oprval.tref->operand[0].oprval.tref;
			assert (r0->opcode == OC_VAR);
			assert (r0->operand[0].oprclass == MVAR_REF);
			r1 = maketriple (OC_PARAMETER);
			r1->operand[0] = put_str (r0->operand[0].oprval.vref->mvname.c,
							mid_len (&r0->operand[0].oprval.vref->mvname));
			r1->operand[1] = a->oprval.tref->operand[1];
			a->oprval.tref->operand[1] = put_tref (r1);
			dqins (a->oprval.tref->exorder.fl, exorder, r1);
		}
		else
		{
			assert (a->oprval.tref->opcode == OC_VAR);
			r0 = newtriple (OC_FNQUERY);
			r0->operand[0] = put_ilit (3);
			r0->operand[1] = put_tref (newtriple (OC_PARAMETER));
			r0->operand[1].oprval.tref->operand[0] = put_str (a->oprval.tref->operand[0].oprval.vref->mvname.c,
									mid_len (&a->oprval.tref->operand[0].oprval.vref->mvname));
			r1 = r0->operand[1].oprval.tref;
			r1->operand[1] = *a;
			*a = put_tref (r0);
		}
	}
	else
	{
	r = maketriple(op);
	switch (window_token)
	{
	case TK_CIRCUMFLEX:
		if (!gvn())
			return FALSE;
		r->opcode = OC_GVQUERY;
		ins_triple(r);
		break;
	case TK_ATSIGN:
		if (shift_gvrefs)
		{
			dqinit(&tmpchain, exorder);
			oldchain = setcurtchain(&tmpchain);
			if (!indirection(&(r->operand[0])))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			r->operand[1] = put_ilit((mint) indir_fnquery);
			ins_triple(r);
			newtriple(OC_GVSAVTARG);
			setcurtchain(oldchain);
			dqadd(expr_start, &tmpchain, exorder);
			expr_start = tmpchain.exorder.bl;
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(expr_start);
		}
		else
		{
			if (!indirection(&(r->operand[0])))
				return FALSE;
			r->operand[1] = put_ilit((mint) indir_fnquery);
			ins_triple(r);
		}
		r->opcode = OC_INDFUN;
		break;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	*a = put_tref(r);
	}
	return TRUE;
}
