/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mdq.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF	boolean_t	run_time;
GBLREF mline		*mline_tail;

error_def(ERR_ACTOFFSET);

int m_do(void)
/* compiler module for a DO command */
{
	int		opcd;
	oprtype		*cr;
	triple		*calltrip, *labelref, *obp, *oldchain, *ref0, *ref1, *routineref, tmpchain, *triptr;
#	ifndef __i386
	triple		*tripsize;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TK_SPACE == TREF(window_token)) || (TK_EOL == TREF(window_token)))
	{
		if (!run_time)	/* DO SP SP is a noop at run time */
		{
			calltrip = newtriple(OC_CALLSP);
			calltrip->operand[0] = put_mnxl();
			mline_tail->block_ok = TRUE;
#			ifndef __i386
			calltrip->operand[1] = put_ocnt();
#			endif
		}
		return TRUE;
	} else if (TK_AMPERSAND == TREF(window_token))
	{
		if (!extern_func(0))
			return FALSE;
		else
			return TRUE;
	}
	dqinit(&tmpchain, exorder);
	oldchain = setcurtchain(&tmpchain);
	calltrip = entryref(OC_CALL, OC_EXTCALL, (mint)indir_do, TRUE, FALSE, FALSE);
	setcurtchain(oldchain);
	if (!calltrip)
		return FALSE;
	if (TK_LPAREN == TREF(window_token))
	{
		if (OC_CALL == calltrip->opcode)
		{
			assert(MLAB_REF == calltrip->operand[0].oprclass);
			calltrip->opcode = OC_EXCAL;
#			ifdef __i386
			ref0 = calltrip;
#			else
			ref0 = newtriple(OC_PARAMETER);
			calltrip->operand[1] = put_tref(ref0);
			ref0->operand[0] = put_tsiz();	/* parm to hold size of jump codegen */
			tripsize = ref0->operand[0].oprval.tref;
			assert(OC_TRIPSIZE == tripsize->opcode);
#			endif
		} else
		{
			if (OC_EXTCALL == calltrip->opcode)
			{
				assert(TRIP_REF == calltrip->operand[1].oprclass);
#				ifdef __i386
				if (OC_CDLIT == calltrip->operand[1].oprval.tref->opcode)
				{
					assert(CDLT_REF == calltrip->operand[1].oprval.tref->operand[0].oprclass);
#				else
				opcd = calltrip->operand[1].oprval.tref->opcode;
				if ((OC_CDLIT == opcd) || (OC_CDIDX == opcd))
				{
					DEBUG_ONLY(opcd = calltrip->operand[1].oprval.tref->operand[0].oprclass);
					assert((CDLT_REF == opcd) || (CDIDX_REF == opcd));
#				endif
				} else
				{
					assert(OC_LABADDR == calltrip->operand[1].oprval.tref->opcode);
					assert(TRIP_REF == calltrip->operand[1].oprval.tref->operand[1].oprclass);
					assert(OC_PARAMETER == calltrip->operand[1].oprval.tref->operand[1].oprval.tref->opcode);
					assert(TRIP_REF ==
						calltrip->operand[1].oprval.tref->operand[1].oprval.tref->operand[0].oprclass);
					DEBUG_ONLY(opcd = calltrip->operand[1].oprval.tref->operand[1].oprval.tref->
						   operand[0].oprval.tref->opcode);
					assert((OC_ILIT == opcd) || (OC_COMINT == opcd));
					DEBUG_ONLY(opcd = calltrip->operand[1].oprval.tref->operand[1].oprval.tref->
						   operand[0].oprval.tref->operand[0].oprclass);
					assert((ILIT_REF == opcd) || (TRIP_REF == opcd));
					/* The opcd references above added to allow an invalid syntax using indirect values for
					 * offsets while specifying a parm list to get through the above asserts (invalid syntax
					 * should not trip asserts) but it leads to the conclusion that the below test may not be
					 * robust enough since it is looking at a literal integer value when there is none so have
					 * added further checks mirroring the first checks done in the two most recent asserts to
					 * make the check more robust. [Example bad code: Do @lbl+@n^artn(arg)]
					 */
					if ((0 != calltrip->operand[1].oprval.tref->operand[1].oprval.tref->
					     operand[0].oprval.tref->operand[0].oprval.ilit)
					    || (OC_ILIT != calltrip->operand[1].oprval.tref->operand[1].oprval.tref->
						operand[0].oprval.tref->opcode)
					    || (ILIT_REF != calltrip->operand[1].oprval.tref->operand[1].oprval.tref->
						operand[0].oprval.tref->operand[0].oprclass))
					{
						stx_error(ERR_ACTOFFSET);
						return FALSE;
					}
				}
			} else
			{	/* DO _ @dlabel actuallist */
				assert(OC_COMMARG == calltrip->opcode);
				assert(TRIP_REF == calltrip->operand[1].oprclass);
				assert(OC_ILIT == calltrip->operand[1].oprval.tref->opcode);
				assert(ILIT_REF == calltrip->operand[1].oprval.tref->operand[0].oprclass);
				assert((mint)indir_do == calltrip->operand[1].oprval.tref->operand[0].oprval.ilit);
				assert(calltrip->exorder.fl == &tmpchain);
				routineref = maketriple(OC_CURRHD);
				labelref = maketriple(OC_LABADDR);
				ref0 = maketriple(OC_PARAMETER);
				dqins(calltrip->exorder.bl, exorder, routineref);
				dqins(calltrip->exorder.bl, exorder, labelref);
				dqins(calltrip->exorder.bl, exorder, ref0);
				labelref->operand[0] = calltrip->operand[0];
				labelref->operand[1] = put_tref(ref0);
				ref0->operand[0] = calltrip->operand[1];
				ref0->operand[0].oprval.tref->operand[0].oprval.ilit = 0;
				ref0->operand[1] = put_tref(routineref);
				calltrip->operand[0] = put_tref(routineref);
				calltrip->operand[1] = put_tref(labelref);
			}
			calltrip->opcode = OC_EXTEXCAL;
			ref0 = newtriple(OC_PARAMETER);
			ref0->operand[0] = calltrip->operand[1];
			calltrip->operand[1] = put_tref(ref0);
		}
		if (!actuallist(&ref0->operand[1]))
			return FALSE;
	} else if (OC_CALL == calltrip->opcode)
	{
#		ifndef __i386
		calltrip->operand[1] = put_ocnt();
#		endif
		if (TREF(for_stack_ptr) != (oprtype **)TADR(for_stack))
		{
			if (TAREF1(for_temps, (TREF(for_stack_ptr) - (oprtype **)TADR(for_stack))))
				calltrip->opcode = OC_FORLCLDO;
		}
	}
	if (TK_COLON == TREF(window_token))
	{
		advancewindow();
		cr = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (!bool_expr(FALSE, cr))
			return FALSE;
		for (triptr = (TREF(curtchain))->exorder.bl; OC_NOOP == triptr->opcode; triptr = triptr->exorder.bl)
			;
		if (OC_LIT == triptr->opcode)
		{	/* it's a literal so optimize it */
			unuse_literal(&triptr->operand[0].oprval.mlit->v);
			dqdel(triptr, exorder);
			if (0 == triptr->operand[0].oprval.mlit->v.m[1])
			{
				setcurtchain(oldchain);		/* it's a FALSE so just discard the whole thing */
#				ifndef __i386
				if (OC_EXCAL == calltrip->opcode)
					tripsize->opcode = OC_NOOP;		/* if we are abandoning this DO, clear this too */
#				endif
				return TRUE;
			}					/* the code below is the same as for the no postconditional case */
			obp = oldchain->exorder.bl;		/* it's a TRUE - just pretend it isn't even there */
			dqadd(obp, &tmpchain, exorder);		/* this is a violation of info hiding */
			if (OC_EXCAL == calltrip->opcode)
			{	/* this code is the same as below for no condition */
				triptr = newtriple(OC_JMP);
				triptr->operand[0] = put_mfun(&calltrip->operand[0].oprval.lab->mvname);
				calltrip->operand[0].oprclass = ILIT_REF;	/* dummy placeholder */
#				ifndef __i386
				tripsize->operand[0].oprval.tsize->ct = triptr;
#				endif
			}
			return TRUE;
		}
		if ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode))
		{
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(TREF(expr_start));
		}
		obp = oldchain->exorder.bl;
		dqadd(obp, &tmpchain, exorder);			/* this is a violation of info hiding */
		if (OC_EXCAL == calltrip->opcode)
		{
			triptr = newtriple(OC_JMP);
			triptr->operand[0] = put_mfun(&calltrip->operand[0].oprval.lab->mvname);
			calltrip->operand[0].oprclass = ILIT_REF;	/* dummy placeholder */
#			ifndef __i386
			tripsize->operand[0].oprval.tsize->ct = triptr;
#			endif
		}
		if ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode))
		{
			ref0 = newtriple(OC_JMP);
			ref1 = newtriple(OC_GVRECTARG);
			ref1->operand[0] = put_tref(TREF(expr_start));
			*cr = put_tjmp(ref1);
			tnxtarg(&ref0->operand[0]);
		} else
			tnxtarg(cr);
	} else
	{	/* the code below is the same as for the case of a postconditional known at compile time to be TRUE */
		obp = oldchain->exorder.bl;
		dqadd(obp, &tmpchain, exorder);			/* this is a violation of info hiding */
		if (OC_EXCAL == calltrip->opcode)
		{
			triptr = newtriple(OC_JMP);
			triptr->operand[0] = put_mfun(&calltrip->operand[0].oprval.lab->mvname);
			calltrip->operand[0].oprclass = ILIT_REF;	/* dummy placeholder */
#			ifndef __i386
			tripsize->operand[0].oprval.tsize->ct = triptr;
#			endif
		}
	}
	return TRUE;
}
