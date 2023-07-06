/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"		/* needed by INCREMENT_EXPR_DEPTH */
#include "compiler.h"
#include "opcode.h"
#include "fullbool.h"
#include "mdq.h"

int expr(oprtype *a, int m_type)
{
	int		rval;
	boolean_t	start_shift;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	INCREMENT_EXPR_DEPTH;
	start_shift = TREF(shift_side_effects);
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	if (EXPR_FAIL == (rval = eval_expr(a)))		/* NOTE assignment */
	{
		DECREMENT_EXPR_DEPTH;
		return FALSE;
	}
	coerce(a, (MUMPS_INT == m_type) ? OCT_MINT : OCT_MVAL);
	ex_tail(a, 0);	/* There is a chance this will return a OCT_MVAL when we want OCT_MINT; force it again */
	RETURN_EXPR_IF_RTS_ERROR;
	coerce(a, (MUMPS_INT == m_type) ? OCT_MINT : OCT_MVAL);	/* Investigate whether ex_tail can do a better job */
	if (TREF(expr_start) != TREF(expr_start_orig) && (OC_NOOP != (TREF(expr_start))->opcode))
	{
		assert((OC_GVSAVTARG == (TREF(expr_start))->opcode));
		if ((OC_GVSAVTARG == (TREF(expr_start))->opcode) && ((YDB_BOOL == TREF(ydb_fullbool)) || !TREF(saw_side_effect)))
		{
			if ((OC_GVRECTARG != (TREF(curtchain))->exorder.bl->opcode)
				|| ((TREF(curtchain))->exorder.bl->operand[0].oprval.tref != TREF(expr_start)))
					newtriple(OC_GVRECTARG)->operand[0] = put_tref(TREF(expr_start));
		}
	}
	TREF(shift_side_effects) = start_shift;
	DECREMENT_EXPR_DEPTH;
	return rval;
}
