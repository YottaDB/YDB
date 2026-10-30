/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	*
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
#include "mdq.h"

GBLREF	int4		pending_errtriplecode;	/* if non-zero contains the error code to invoke ins_errtriple with */
GBLREF	triple		t_orig;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_DIVZERO);
error_def(ERR_EXPRTOODEEP);
error_def(ERR_NEGFRACPWR);
error_def(ERR_NUMOFLOW);
error_def(ERR_PATNOTFOUND);

void ins_errtriple(int4 in_error)
{
	boolean_t	add_rterror_triple;
	triple 		*triptr, *x;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!IS_STX_WARN(in_error) GTMTRIG_ONLY( || TREF(trigger_compile_and_link)))
	{	/* Not a warning and not a trigger, we have a real error (warnings become errors in triggers) */
		if (TREF(curtchain) != &t_orig)
		{	/* If working with more than 1 chain defer until back to 1 because dqdelchain cannot delete across
			 * multiple chains. Set global variable "pending_errtriplecode" and let "setcurtchain" call here again.
			 */
			if (!pending_errtriplecode)			/* Give user only the first error on the line */
				pending_errtriplecode = in_error;	/* Save error for later insert */
			return;
		}
		x = (TREF(pos_in_chain)).exorder.bl;
		/* Note that "x" can be NULL in case we encounter an error before we started processing the first line of
		 * the M program (see https://gitlab.com/YottaDB/DB/YDB/-/issues/860#note_1056075754 for test case).
		 */
		assert((NULL == x) || (NULL != x->exorder.fl));
		if (NULL != x)
		{
			/* If first error in the current line/cmd, delete all triples and replace them with an OC_RTERROR triple.
			 * If not the first error in the current line/cmd, do not add a new OC_RTERROR triple as there is
			 * already one.
			 */
			triptr = x->exorder.fl;
			add_rterror_triple = (OC_RTERROR != triptr->opcode);
			if (add_rterror_triple)
			{	/* delete all trailing triples (and replace them further down with an OC_RTERROR triple) */
				dqdelchain(x, TREF(curtchain), exorder);
				CHKTCHAIN(TREF(curtchain), exorder, FALSE);
				assert((TREF(pos_in_chain)).exorder.bl->exorder.fl == TREF(curtchain));
				assert((TREF(curtchain))->exorder.bl == (TREF(pos_in_chain)).exorder.bl);
			} else if ((TRIP_REF == triptr->operand[0].oprclass) && (TRIP_REF == triptr->operand[1].oprclass)
					&& (triptr->exorder.fl == triptr->operand[0].oprval.tref)
					&& (triptr->exorder.fl->exorder.fl == triptr->operand[1].oprval.tref))
			{	/* The OC_RTERROR triple and its 2 OC_ILIT operands are consecutive, as when an earlier call
				 * here added them. That call could have been for an IS_STX_WARN error, after which parsing
				 * went on, so delete the triples after them, which can be those of a parse that failed. An
				 * OC_RTERROR triple that "line()" adds is not followed by its operands, and other triples can
				 * be in between, so leave those untouched.
				 */
				dqdelchain(triptr->operand[1].oprval.tref, TREF(curtchain), exorder);
				CHKTCHAIN(TREF(curtchain), exorder, FALSE);
			}
		} else
		{	/* We have not yet started processing the current line and so add OC_RTERROR triple */
			add_rterror_triple = TRUE;
		}
		if ((ERR_DIVZERO == in_error) || (ERR_NEGFRACPWR == in_error) || (ERR_NUMOFLOW == in_error)
					|| (ERR_PATNOTFOUND == in_error) || (ERR_BOOLEXPRTOODEEP == in_error)
					|| (ERR_EXPRTOODEEP == in_error))
			TREF(rts_error_in_parse) = TRUE;
	} else
	{	/* For IS_STX_WARN errors (if not compiling a trigger), parsing continues, so dont strip the chain */
		add_rterror_triple = TRUE;
	}
	if (add_rterror_triple)
	{	/* WARN error or first error in the current line */
		triptr = newtriple(OC_RTERROR);
		triptr->operand[0] = put_ilit(in_error);
		triptr->operand[1] = put_ilit(FALSE);	/* not a subroutine reference */
	}
}
