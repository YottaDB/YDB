/****************************************************************
 *                                                              *
 * Copyright (c) 2006-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"

#ifdef UTF8_SUPPORTED
#include "toktyp.h"
#include "opcode.h"
#include "advancewindow.h"
#include "gtm_conv.h"
#include "gtm_ctype.h"
#include "gtm_string.h"

error_def(ERR_BADCASECODE);
error_def(ERR_BADCHSET);
error_def(ERR_COMMA);
error_def(ERR_ENCODING);

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
	int	i;
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
		if ((OC_LIT == mode->operand[0].oprval.tref->opcode) &&
		    (-1 == verify_case((tmpstr = &mode->operand[0].oprval.tref->operand[0].oprval.mlit->v.str))))
		{
			stx_error(ERR_BADCASECODE, 2, tmpstr->len, tmpstr->addr);
			return FALSE;
		}
	} else
	{	/* 3rd parameter exists .. reel it in after error checking 2nd parm */
		r->opcode = OC_FNZCONVERT3;
		if ((OC_LIT == mode->operand[0].oprval.tref->opcode) &&
		    (0 >= verify_chset((tmpstr = &mode->operand[0].oprval.tref->operand[0].oprval.mlit->v.str))))
		{	/* Convert extended ascii to a different encoding */
			if (CHSET_M != check_w1252(tmpstr))
			{
				stx_error(ERR_BADCHSET, 2, tmpstr->len, tmpstr->addr);
				return FALSE;
			}
			if (gtm_utf8_mode)
				dec_err(VARLSTCNT(1) ERR_ENCODING);
		}
		advancewindow();
		mode2 = newtriple(OC_PARAMETER);
		mode->operand[1] = put_tref(mode2);
		if (EXPR_FAIL == expr(&(mode2->operand[0]), MUMPS_STR))
			return FALSE;
		if ((OC_LIT == mode2->operand[0].oprval.tref->opcode) &&
		    (0 >= verify_chset((tmpstr = &mode2->operand[0].oprval.tref->operand[0].oprval.mlit->v.str))))
		{	/* Convert UTF8/UTF16 to an extended ascii encoding */
			if (CHSET_M != check_w1252(tmpstr))
			{
				stx_error(ERR_BADCHSET, 2, tmpstr->len, tmpstr->addr);
				return FALSE;
			}
			if (gtm_utf8_mode)
				dec_err(VARLSTCNT(1) ERR_ENCODING);
		}
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}

#else /* UTF8 is not supported */
int f_zconvert(oprtype *a, opctype op)
{
	assert(FALSE);
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2, LEN_AND_LIT("Error: UTF8 is not supported"), errno);
}
#endif
