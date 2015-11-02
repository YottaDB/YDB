/****************************************************************
 *                                                              *
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "fullbool.h"


/* $ZWRITE(): Single parameter - string expression */
int f_zwrite(oprtype *a, opctype op)
{
	triple		*oldchain, *r;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TK_ATSIGN != TREF(window_token))
	{
		r = maketriple(op);
		if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
			return FALSE;
		ins_triple(r);
	} else
	{
		r = maketriple(OC_INDFUN);
		if (SHIFT_SIDE_EFFECTS)
		{
			START_GVBIND_CHAIN(&save_state, oldchain);
			if (!indirection(&(r->operand[0])))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			r->operand[1] = put_ilit((mint)indir_fnzwrite);
			ins_triple(r);
			PLACE_GVBIND_CHAIN(&save_state, oldchain);
		} else
		{
			if (!indirection(&(r->operand[0])))
				return FALSE;
			r->operand[1] = put_ilit((mint)indir_fnzwrite);
			ins_triple(r);
		}
	}
	*a = put_tref(r);
	return TRUE;
}

