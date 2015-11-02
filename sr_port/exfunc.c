/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

error_def(ERR_ACTOFFSET);

int exfunc(oprtype *a, boolean_t alias_target)
{
	triple		*calltrip, *calltrip_opr1_tref, *counttrip, *funret, *labelref, *masktrip;
	triple		*oldchain, *ref0, *routineref, tmpchain, *triptr;
#	if defined(USHBIN_SUPPORTED) || defined(VMS)
	triple		*tripsize;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TK_DOLLAR == TREF(window_token));
	advancewindow();
	dqinit(&tmpchain, exorder);
	oldchain = setcurtchain(&tmpchain);
	calltrip = entryref(OC_EXFUN, OC_EXTEXFUN, INDIR_DUMMY, TRUE, TRUE, FALSE);
	setcurtchain(oldchain);
	if (!calltrip)
		return FALSE;
	if (OC_EXFUN == calltrip->opcode)
	{
		assert(MLAB_REF == calltrip->operand[0].oprclass);
#		if defined(USHBIN_SUPPORTED) || defined(VMS)
		ref0 = newtriple(OC_PARAMETER);
		ref0->operand[0] = put_tsiz();		/* Need size of following code gen triple here */
		calltrip->operand[1] = put_tref(ref0);
		tripsize = ref0->operand[0].oprval.tref;
		assert(OC_TRIPSIZE == tripsize->opcode);
#		else
		ref0 = calltrip;
#		endif
	} else
	{
		assert(TRIP_REF == calltrip->operand[1].oprclass);
		calltrip_opr1_tref = calltrip->operand[1].oprval.tref;
		if (OC_EXTEXFUN == calltrip->opcode)
		{
			if (OC_CDLIT == calltrip_opr1_tref->opcode)
				assert(CDLT_REF == calltrip_opr1_tref->operand[0].oprclass);
			else
			{
				assert(OC_LABADDR == calltrip_opr1_tref->opcode);
				assert(TRIP_REF == calltrip_opr1_tref->operand[1].oprclass);
				assert(OC_PARAMETER == calltrip_opr1_tref->operand[1].oprval.tref->opcode);
				assert(TRIP_REF == calltrip_opr1_tref->operand[1].oprval.tref->operand[0].oprclass);
				assert(OC_ILIT == calltrip_opr1_tref->operand[1].oprval.tref->operand[0].oprval.tref->opcode);
				assert(ILIT_REF
				       == calltrip_opr1_tref->operand[1].oprval.tref->operand[0].oprval.tref->operand[0].oprclass);
				if (0 != calltrip_opr1_tref->operand[1].oprval.tref->operand[0].oprval.tref->operand[0].oprval.ilit)
				{
					stx_error(ERR_ACTOFFSET);
					return FALSE;
				}
			}
		} else		/* indirect: $$@(glvn)[(actuallist)]; note disabiguating parens around glvn specifying dlabel*/
		{
			assert(OC_COMMARG == calltrip->opcode);
			assert(TRIP_REF == calltrip->operand[1].oprclass);
			assert(OC_ILIT == calltrip_opr1_tref->opcode);
			assert(ILIT_REF == calltrip_opr1_tref->operand[0].oprclass);
			assert(INDIR_DUMMY == calltrip_opr1_tref->operand[0].oprval.ilit);
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
			calltrip->opcode = OC_EXTEXFUN;
		}
		ref0 = newtriple(OC_PARAMETER);
		ref0->operand[0] = calltrip->operand[1];
		calltrip->operand[1] = put_tref(ref0);
	}
	if (TK_LPAREN != TREF(window_token))
	{
		masktrip = newtriple(OC_PARAMETER);
		counttrip = newtriple(OC_PARAMETER);
		masktrip->operand[0] = put_ilit(0);
		counttrip->operand[0] = put_ilit(0);
		masktrip->operand[1] = put_tref(counttrip);
		ref0->operand[1] = put_tref(masktrip);
	} else
		if (!actuallist(&ref0->operand[1]))
			return FALSE;
	triptr = oldchain->exorder.bl;
	dqadd(triptr, &tmpchain, exorder);		/*this is a violation of info hiding*/
	if (OC_EXFUN == calltrip->opcode)
	{
		assert(MLAB_REF == calltrip->operand[0].oprclass);
		triptr = newtriple(OC_JMP);
		triptr->operand[0] = put_mfun(&calltrip->operand[0].oprval.lab->mvname);
		calltrip->operand[0].oprclass = ILIT_REF;	/* dummy placeholder */
#		if defined(USHBIN_SUPPORTED) || defined(VMS)
		tripsize->operand[0].oprval.tsize->ct = triptr;
#		endif
	}
	/* If target is an alias, use special container-expecting routine OC_EXFUNRETALS, else regular OC_EXFUNRET */
	funret = newtriple((alias_target ? OC_EXFUNRETALS : OC_EXFUNRET));
	funret->operand[0] = *a = put_tref(calltrip);
	return TRUE;
}
