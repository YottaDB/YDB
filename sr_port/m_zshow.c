/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "mlkdef.h"
#include "zshow.h"
#include "advancewindow.h"
#include "cmd.h"
#include "lv_val.h"

error_def(ERR_VAREXPECTED);

int m_zshow(void)
{
	static readonly char def_str[]="S";
	int	code, rval;
	oprtype	func, output, *stack_dst, *stack_next_op = NULL;
	triple	*lvar, *outtype, *r = NULL, *stack_level;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TK_SPACE == TREF(window_token)) || (TK_EOL == TREF(window_token)) || (TK_COLON == TREF(window_token)))
		code = ZSHOW_NOPARM;
	else
	{
		code = ZSHOW_DEVICE;
		switch (expr(&func, MUMPS_STR))
		{
		case EXPR_FAIL:
			return FALSE;
		case EXPR_GOOD:
			break;
		case EXPR_INDR:
			if (TK_COLON != TREF(window_token))
			{
				make_commarg(&func, indir_zshow);
				return TRUE;
			}
			break;
		default:
			assertpro(FALSE);
		}
	}
	if (TK_COLON == TREF(window_token))
	{
		advancewindow();
		switch (TREF(window_token))
		{
		case TK_CIRCUMFLEX:
			if (!gvn())
				return FALSE;
			r = maketriple(OC_ZSHOW);
			outtype = newtriple(OC_PARAMETER);
			r->operand[1] = put_tref(outtype);
			if (code == ZSHOW_NOPARM)
				r->operand[0] = put_str(&def_str[0], (SIZEOF(def_str) - 1));
			else
				r->operand[0] = func;
			outtype->operand[0] = put_ilit(ZSHOW_GLOBAL);
			stack_dst = &outtype->operand[1];
			break;
		case TK_IDENT:
			if (!lvn(&output, OC_PUTINDX, 0))
			{
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			r = maketriple(OC_ZSHOWLOC);
			outtype = newtriple(OC_PARAMETER);
			r->operand[1] = put_tref(outtype);
			if (code == ZSHOW_NOPARM)
				r->operand[0] = put_str(&def_str[0], (SIZEOF(def_str) - 1));
			else
				r->operand[0] = func;
			lvar = newtriple(OC_PARAMETER);
			outtype->operand[1] = put_tref(lvar);
			stack_dst = &lvar->operand[1];
			lvar->operand[0] = output;
			outtype->operand[0] = put_ilit(ZSHOW_LOCAL);
			break;
		case TK_ATSIGN:
			if (!indirection(&output))
			{
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			r = maketriple(OC_INDRZSHOW);
			if (code == ZSHOW_NOPARM)
				r->operand[0] = put_str(&def_str[0], (SIZEOF(def_str) - 1));
			else
				r->operand[0] = func;
			stack_dst = &r->operand[1];
			stack_next_op = &output;
			break;
		/* Second colon of `ZSHOW "V"::`, handled below */
		case TK_COLON:
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
	}
	if (NULL == r) { /* no output specified: output to device */
		r = maketriple(OC_ZSHOW);
		outtype = newtriple(OC_PARAMETER);
		r->operand[1] = put_tref(outtype);
		if (code == ZSHOW_NOPARM)
			r->operand[0] = put_str(&def_str[0], (SIZEOF(def_str) - 1));
		else
			r->operand[0] = func;
		outtype->operand[0] = put_ilit(ZSHOW_DEVICE);
		stack_dst = &outtype->operand[1];
	}

	stack_level = newtriple(OC_PARAMETER);
	if (TK_COLON == TREF(window_token))
	{
		advancewindow();
		if (EXPR_FAIL == expr(&stack_level->operand[0], MUMPS_INT))
			return FALSE;
	} else {
		stack_level->operand[0] = put_ilit(STACK_LEVEL_MINUS_ONE);
	}

	*stack_dst = put_tref(stack_level);
	if (NULL != stack_next_op)
		stack_level->operand[1] = *stack_next_op;

	ins_triple(r);
	return TRUE;
}
