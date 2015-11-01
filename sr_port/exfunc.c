/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "toktyp.h"
#include "mdq.h"
#include "advancewindow.h"

#define	INDIR_DUMMY	-1

GBLREF char	window_token;

int exfunc (oprtype *a)
{
	triple		*ref0, *calltrip, *masktrip, *counttrip, *funret, *tripsize;
	triple		*triptr;
	triple		tmpchain, *oldchain, *obp, *routineref, *labelref;
	error_def	(ERR_ACTOFFSET);

	assert (window_token == TK_DOLLAR);
	advancewindow();
	assert (window_token == TK_DOLLAR);
	advancewindow();
	dqinit (&tmpchain, exorder);
	oldchain = setcurtchain (&tmpchain);
	calltrip = entryref (OC_EXFUN, OC_EXTEXFUN, INDIR_DUMMY, TRUE, TRUE);
	setcurtchain (oldchain);
	if (!calltrip) return FALSE;
	if (calltrip->opcode == OC_EXFUN)
	{
		assert(calltrip->operand[0].oprclass == MLAB_REF);
		ref0 = newtriple(OC_PARAMETER);
		ref0->operand[0] = put_tsiz();		/* Need size of following code gen triple here */
		calltrip->operand[1] = put_tref(ref0);
		tripsize = ref0->operand[0].oprval.tref;
		assert(OC_TRIPSIZE == tripsize->opcode);
	}
	else
	{
		if (calltrip->opcode == OC_EXTEXFUN)
		{
			assert (calltrip->operand[1].oprclass == TRIP_REF);
			if (calltrip->operand[1].oprval.tref->opcode == OC_CDLIT)
				assert (calltrip->operand[1].oprval.tref->operand[0].oprclass == CDLT_REF);
			else
			{
			assert (calltrip->operand[1].oprval.tref->opcode == OC_LABADDR);
			assert (calltrip->operand[1].oprval.tref->operand[1].oprclass == TRIP_REF);
			assert (calltrip->operand[1].oprval.tref->operand[1].oprval.tref->opcode == OC_PARAMETER);
			assert (calltrip->operand[1].oprval.tref->operand[1].oprval.tref->operand[0].oprclass == TRIP_REF);
			assert (calltrip->operand[1].oprval.tref->operand[1].oprval.tref->operand[0].oprval.tref->opcode
				== OC_ILIT);
			assert
			(calltrip->operand[1].oprval.tref->operand[1].oprval.tref->operand[0].oprval.tref->operand[0].oprclass
				== ILIT_REF);
			if
			(calltrip->operand[1].oprval.tref->operand[1].oprval.tref->operand[0].oprval.tref->operand[0].oprval.ilit
				!= 0)
				{
					stx_error (ERR_ACTOFFSET);
					return FALSE;
				}
			}
		}
		else		/* $$ @dlabel [actuallist] */
		{
			assert (calltrip->opcode == OC_COMMARG);
			assert (calltrip->operand[1].oprclass == TRIP_REF);
			assert (calltrip->operand[1].oprval.tref->opcode == OC_ILIT);
			assert (calltrip->operand[1].oprval.tref->operand[0].oprclass == ILIT_REF);
			assert (calltrip->operand[1].oprval.tref->operand[0].oprval.ilit == INDIR_DUMMY);
			assert (calltrip->exorder.fl == &tmpchain);
			routineref = maketriple (OC_CURRHD);
			labelref = maketriple (OC_LABADDR);
			ref0 = maketriple (OC_PARAMETER);
			dqins (calltrip->exorder.bl, exorder, routineref);
			dqins (calltrip->exorder.bl, exorder, labelref);
			dqins (calltrip->exorder.bl, exorder, ref0);
			labelref->operand[0] = calltrip->operand[0];
			labelref->operand[1] = put_tref (ref0);
			ref0->operand[0] = calltrip->operand[1];
			ref0->operand[0].oprval.tref->operand[0].oprval.ilit = 0;
			ref0->operand[1] = put_tref (routineref);
			calltrip->operand[0] = put_tref (routineref);
			calltrip->operand[1] = put_tref (labelref);
			calltrip->opcode = OC_EXTEXFUN;
		}
		ref0 = newtriple (OC_PARAMETER);
		ref0->operand[0] = calltrip->operand[1];
		calltrip->operand[1] = put_tref (ref0);
	}
	if (window_token != TK_LPAREN)
	{
		masktrip = newtriple (OC_PARAMETER);
		counttrip = newtriple (OC_PARAMETER);
		masktrip->operand[0] = put_ilit (0);
		counttrip->operand[0] = put_ilit (0);
		masktrip->operand[1] = put_tref (counttrip);
		ref0->operand[1] = put_tref (masktrip);
	}
	else
		if (!actuallist (&ref0->operand[1])) return FALSE;
	obp = oldchain->exorder.bl;
	dqadd (obp, &tmpchain, exorder);		/*this is a violation of info hiding*/
	if (calltrip->opcode == OC_EXFUN)
	{
		assert(calltrip->operand[0].oprclass == MLAB_REF);
		triptr = newtriple (OC_JMP);
		triptr->operand[0] = put_mfun (&calltrip->operand[0].oprval.lab->mvname);
		calltrip->operand[0].oprclass = ILIT_REF;	/* dummy placeholder */
		tripsize->operand[0].oprval.tsize->ct = triptr;
	}

	funret = newtriple (OC_EXFUNRET);
	funret->operand[0] = *a = put_tref (calltrip);
	return TRUE;
}
