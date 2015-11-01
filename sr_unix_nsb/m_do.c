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
#include "mdq.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF	bool		for_temps[MAX_FOR_STACK],
			run_time;
GBLREF	char		window_token;
GBLREF	oprtype		*for_stack[MAX_FOR_STACK],
			**for_stack_ptr;
GBLREF	triple		*expr_start,
			*expr_start_orig;

error_def(ERR_ACTOFFSET);


int m_do(void)
{
	triple		tmpchain, *oldchain, *obp, *ref0,
			*triptr, *ref1, *calltrip, *routineref, *labelref;
	oprtype		*cr;


	if (window_token == TK_EOL || window_token == TK_SPACE)
	{
		if (!run_time)	/* DO SP SP is a noop at run time */
		{
			calltrip = newtriple(OC_CALLSP);
			calltrip->operand[0] = put_mnxl();
		}
		return TRUE;
	}
	else if (window_token == TK_AMPERSAND)
	{	if (!extern_func(0))
			return FALSE;
		else
			return TRUE;
	}
	dqinit(&tmpchain, exorder);
	oldchain = setcurtchain(&tmpchain);
	calltrip = entryref(OC_CALL, OC_EXTCALL, (mint) indir_do, TRUE, FALSE);
	setcurtchain(oldchain);
	if (!calltrip)
		return FALSE;
	if (window_token == TK_LPAREN)
	{
		if (calltrip->opcode == OC_CALL)
		{
			assert (calltrip->operand[0].oprclass == MLAB_REF);
			calltrip->opcode = OC_EXCAL;
			ref0 = calltrip;
		}
		else
		{
			if (calltrip->opcode == OC_EXTCALL)
			{
				assert (calltrip->operand[1].oprclass == TRIP_REF);
				if (calltrip->operand[1].oprval.tref->opcode == OC_CDLIT)
					assert (calltrip->operand[1].oprval.tref->operand[0].oprclass == CDLT_REF);
				else
				{
					assert (calltrip->operand[1].oprval.tref->opcode == OC_LABADDR);
					assert (calltrip->operand[1].oprval.tref->operand[1].oprclass == TRIP_REF);
					assert (calltrip->operand[1].oprval.tref->operand[1].oprval.tref->opcode
						== OC_PARAMETER);
					assert (calltrip->operand[1].oprval.tref->operand[1].oprval.tref->operand[0].oprclass
						== TRIP_REF);
					assert (calltrip->operand[1].oprval.tref->operand[1].oprval.tref->
						operand[0].oprval.tref->opcode == OC_ILIT);
					assert (calltrip->operand[1].oprval.tref->operand[1].oprval.tref->
						operand[0].oprval.tref->operand[0].oprclass == ILIT_REF);
					if (calltrip->operand[1].oprval.tref->
						operand[1].oprval.tref->operand[0].oprval.tref->
						operand[0].oprval.ilit != 0)
					{
						stx_error (ERR_ACTOFFSET);
						return FALSE;
					}
				}
			}
			else		/* DO _ @dlabel actuallist */
			{
				assert (calltrip->opcode == OC_COMMARG);
				assert (calltrip->operand[1].oprclass == TRIP_REF);
				assert (calltrip->operand[1].oprval.tref->opcode == OC_ILIT);
				assert (calltrip->operand[1].oprval.tref->operand[0].oprclass == ILIT_REF);
				assert (calltrip->operand[1].oprval.tref->operand[0].oprval.ilit == (mint) indir_do);
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
			}
			calltrip->opcode = OC_EXTEXCAL;
			ref0 = newtriple (OC_PARAMETER);
			ref0->operand[0] = calltrip->operand[1];
			calltrip->operand[1] = put_tref (ref0);
		}
		if (!actuallist (&ref0->operand[1]))
			return FALSE;
	}
	else if (calltrip->opcode == OC_CALL)
	{
		if (for_stack_ptr != for_stack)
		{
			if (for_temps[ (for_stack_ptr - &for_stack[0]) ])
				calltrip->opcode = OC_FORLCLDO;
		}
	}

	if (window_token == TK_COLON)
	{
		advancewindow();
		cr = (oprtype *) mcalloc(sizeof(oprtype));
		if (!bool_expr((bool) FALSE, cr))
			return FALSE;
		if (expr_start != expr_start_orig)
		{
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(expr_start);
		}
		obp = oldchain->exorder.bl;
		dqadd(obp, &tmpchain, exorder);   /*this is a violation of info hiding*/
		if (calltrip->opcode == OC_EXCAL)
		{
			triptr = newtriple (OC_JMP);
			triptr->operand[0] = put_mfun (&calltrip->operand[0].oprval.lab->mvname);
			calltrip->operand[0].oprclass = ILIT_REF;	/* dummy placeholder */
		}
		if (expr_start != expr_start_orig)
		{
			ref0 = newtriple(OC_JMP);
			ref1 = newtriple(OC_GVRECTARG);
			ref1->operand[0] = put_tref(expr_start);
			*cr = put_tjmp(ref1);
			tnxtarg(&ref0->operand[0]);
		}
		else
			tnxtarg(cr);
	}
	else
	{
		obp = oldchain->exorder.bl;
		dqadd(obp, &tmpchain, exorder);   /*this is a violation of info hiding*/
		if (calltrip->opcode == OC_EXCAL)
		{
			triptr = newtriple (OC_JMP);
			triptr->operand[0] = put_mfun (&calltrip->operand[0].oprval.lab->mvname);
			calltrip->operand[0].oprclass = ILIT_REF;	/* dummy placeholder */
		}
	}

	return TRUE;
}
