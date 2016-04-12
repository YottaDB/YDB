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
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "fullbool.h"

error_def(ERR_VAREXPECTED);

int f_query(oprtype *a, opctype op)
{
	triple		*oldchain, *r, *r0, *r1;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TK_IDENT == TREF(window_token))
	{
		if (!lvn(a, OC_FNQUERY, 0))
			return FALSE;
		assert(TRIP_REF == a->oprclass);
		if (OC_FNQUERY == a->oprval.tref->opcode)
		{
			assert(OC_FNQUERY == a->oprval.tref->opcode);
			assert(TRIP_REF == a->oprval.tref->operand[0].oprclass);
			assert(OC_ILIT == a->oprval.tref->operand[0].oprval.tref->opcode);
			assert(ILIT_REF == a->oprval.tref->operand[0].oprval.tref->operand[0].oprclass);
			assert(0 < a->oprval.tref->operand[0].oprval.tref->operand[0].oprval.ilit);
			a->oprval.tref->operand[0].oprval.tref->operand[0].oprval.ilit += 2;
			assert(TRIP_REF == a->oprval.tref->operand[1].oprclass);
			assert(OC_PARAMETER == a->oprval.tref->operand[1].oprval.tref->opcode);
			assert(TRIP_REF == a->oprval.tref->operand[1].oprval.tref->operand[0].oprclass);
			r0 = a->oprval.tref->operand[1].oprval.tref->operand[0].oprval.tref;
			assert(OC_VAR == r0->opcode);
			assert(MVAR_REF == r0->operand[0].oprclass);
			r1 = maketriple(OC_PARAMETER);
			r1->operand[0] = put_str(r0->operand[0].oprval.vref->mvname.addr, r0->operand[0].oprval.vref->mvname.len);
			r1->operand[1] = a->oprval.tref->operand[1];
			a->oprval.tref->operand[1] = put_tref (r1);
			dqins (a->oprval.tref->exorder.fl, exorder, r1);
		} else
		{
			assert(OC_VAR == a->oprval.tref->opcode);
			r0 = newtriple(OC_FNQUERY);
			r0->operand[0] = put_ilit (3);
			r0->operand[1] = put_tref(newtriple(OC_PARAMETER));
			r0->operand[1].oprval.tref->operand[0] = put_str(a->oprval.tref->operand[0].oprval.vref->mvname.addr,
									a->oprval.tref->operand[0].oprval.vref->mvname.len);
			r1 = r0->operand[1].oprval.tref;
			r1->operand[1] = *a;
			*a = put_tref (r0);
		}
	} else
	{
		r = maketriple(op);
		switch (TREF(window_token))
		{
		case TK_CIRCUMFLEX:
			if (!gvn())
				return FALSE;
			r->opcode = OC_GVQUERY;
			ins_triple(r);
			break;
		case TK_ATSIGN:
			if (SHIFT_SIDE_EFFECTS)
			{
				START_GVBIND_CHAIN(&save_state, oldchain);
				if (!indirection(&(r->operand[0])))
				{
					setcurtchain(oldchain);
					return FALSE;
				}
				r->operand[1] = put_ilit((mint)indir_fnquery);
				ins_triple(r);
				PLACE_GVBIND_CHAIN(&save_state, oldchain);
			} else
			{
				if (!indirection(&(r->operand[0])))
					return FALSE;
				r->operand[1] = put_ilit((mint)indir_fnquery);
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
