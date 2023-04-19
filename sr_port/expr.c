/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	INCREMENT_EXPR_DEPTH;
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	if (EXPR_FAIL == (rval = eval_expr(a)))		/* NOTE assignment */
	{
		DECREMENT_EXPR_DEPTH;
		return FALSE;
	}
	coerce(a, (MUMPS_INT == m_type) ? OCT_MINT : OCT_MVAL);
<<<<<<< HEAD
	ex_tail(a, 0);	/* There is a chance this will return a OCT_MVAL when we want OCT_MINT; force it again */
=======
	ex_tail(a, FALSE, FALSE);	/* There is a chance this will return a OCT_MVAL when we want OCT_MINT; force it again */
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
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
	DECREMENT_EXPR_DEPTH;
	return rval;
}
