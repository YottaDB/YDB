/****************************************************************
 *                                                              *
 *      Copyright 2006, 2011 Fidelity Information Services, Inc       *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"

#ifdef UNICODE_SUPPORTED
#include "toktyp.h"
#include "opcode.h"
#include "advancewindow.h"
#include "gtm_conv.h"

error_def(ERR_BADCASECODE);
error_def(ERR_BADCHSET);
error_def(ERR_COMMA);

/* $ZCONVERT(): 3 parameters (3rd optional) - all are string expressions.
 * For 2 argument $ZCONVERT, if 2nd argument is a literal, must be one of
 * "U", "L", or "T" (case independent) or else raise BADCASECODE error.
 * For 3 argument $ZCONVERT, if 2nd or 3rd arguments are literals, they
 * must be one of "UTF-8", "UTF-16LE", or "UTF-16BE" (case independent)
 * or else raise BADCHSET error.
 */
int f_zconvert(oprtype *a, opctype op)
{
	triple	*r, *mode, *mode2;
	mstr	*tmpstr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	/* 2nd parameter (required) */
	mode = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(mode);
	if (EXPR_FAIL == expr(&(mode->operand[0]), MUMPS_STR))
		return FALSE;
	/* Check for 3rd parameter */
	if (TK_COMMA != TREF(window_token))
	{	/* 3rd parameter does not exist. Do checks for 2 arument $zconvert */
		if (mode->operand[0].oprval.tref->opcode == OC_LIT &&
		    -1 == verify_case((tmpstr = &mode->operand[0].oprval.tref->operand[0].oprval.mlit->v.str)))
		{
			stx_error(ERR_BADCASECODE, 2, tmpstr->len, tmpstr->addr);
			return FALSE;
		}
	} else
	{	/* 3rd parameter exists .. reel it in after error checking 2nd parm */
		r->opcode = OC_FNZCONVERT3;
		if (mode->operand[0].oprval.tref->opcode == OC_LIT &&
		    0 >= verify_chset((tmpstr = &mode->operand[0].oprval.tref->operand[0].oprval.mlit->v.str)))
		{
			stx_error(ERR_BADCHSET, 2, tmpstr->len, tmpstr->addr);
			return FALSE;
		}
		advancewindow();
		mode2 = newtriple(OC_PARAMETER);
		mode->operand[1] = put_tref(mode2);
		if (EXPR_FAIL == expr(&(mode2->operand[0]), MUMPS_STR))
			return FALSE;
		if (mode2->operand[0].oprval.tref->opcode == OC_LIT &&
		    0 >= verify_chset((tmpstr = &mode2->operand[0].oprval.tref->operand[0].oprval.mlit->v.str)))
		{
			stx_error(ERR_BADCHSET, 2, tmpstr->len, tmpstr->addr);
			return FALSE;
		}
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}

#else /* Unicode is not supported */
int f_zconvert(oprtype *a, opctype op)
{
	GTMASSERT;
}
#endif
