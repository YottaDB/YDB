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
#include "fullbool.h"

int expr(oprtype *a, int m_type)
{
	int	rval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!(TREF(expr_depth))++)
		TREF(expr_start) = TREF(expr_start_orig) = NULL;
	if (EXPR_FAIL == (rval = eval_expr(a)))		/* NOTE assignment */
	{
		TREF(expr_depth) = 0;
		return FALSE;
	}
	coerce(a, (MUMPS_INT == m_type) ? OCT_MINT : OCT_MVAL);
	ex_tail(a);
	if (TREF(expr_start) != TREF(expr_start_orig) && (OC_NOOP != (TREF(expr_start))->opcode))
	{
		assert(TREF(shift_side_effects));
		assert((OC_GVSAVTARG == (TREF(expr_start))->opcode));
		if ((OC_GVSAVTARG == (TREF(expr_start))->opcode) && ((GTM_BOOL == TREF(gtm_fullbool)) || !TREF(saw_side_effect)))
		{
			if ((OC_GVRECTARG != (TREF(curtchain))->exorder.bl->opcode)
				|| ((TREF(curtchain))->exorder.bl->operand[0].oprval.tref != TREF(expr_start)))
					newtriple(OC_GVRECTARG)->operand[0] = put_tref(TREF(expr_start));
		}
	}
	if (!(--(TREF(expr_depth))))
		TREF(saw_side_effect) = TREF(shift_side_effects) = FALSE;
	return rval;
}
