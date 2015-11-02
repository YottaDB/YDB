/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "nametabtyp.h"
#include "toktyp.h"
#include "merge_def.h"
#include "cmd.h"
#include "mvalconv.h"
#include "advancewindow.h"

GBLREF triple *curtchain;
GBLREF char window_token;

int m_merge(void)
{
	error_def(ERR_VAREXPECTED);
	error_def(ERR_RPARENMISSING);
	error_def(ERR_EQUAL);

	opctype 	put_oc;
	oprtype 	mopr;
	triple		*sub, *ref,  *obp, *s1, *restart, tmpchain;
	mval		mv;
	int		type;

	restart = newtriple(OC_RESTARTPC);	/* Here is where a restart should pick up */

	dqinit(&tmpchain, exorder);
	/* Left Hand Side of EQUAL sign */
	switch (window_token)
	{
        case TK_IDENT:
                if (!lvn(&mopr, OC_PUTINDX, 0))
                        return FALSE;
		if (OC_PUTINDX == mopr.oprval.tref->opcode)
		{	/* we insert left hand side argument into tmpchain. */
			dqdel(mopr.oprval.tref, exorder);
			dqins(tmpchain.exorder.bl, exorder, mopr.oprval.tref);
		}
		ref = maketriple(OC_MERGE_LVARG);
		ref->operand[0] = put_ilit(MARG1_LCL);
		ref->operand[1] = mopr;
		dqins(tmpchain.exorder.bl, exorder, ref);
                break;
        case TK_CIRCUMFLEX:
		s1 = curtchain->exorder.bl;
                if (!gvn())
                        return FALSE;
		for (sub = curtchain->exorder.bl; sub != s1; sub = sub->exorder.bl)
		{
			put_oc = sub->opcode;
			if (OC_GVNAME == put_oc || OC_GVNAKED == put_oc || OC_GVEXTNAM == put_oc)
				break;
		}
		assert(OC_GVNAME == put_oc || OC_GVNAKED == put_oc || OC_GVEXTNAM == put_oc);
		/* we insert left hand side argument into tmpchain. */
		dqdel(sub, exorder);
		dqins(tmpchain.exorder.bl ,exorder, sub);
		ref = maketriple(OC_MERGE_GVARG);
		ref->operand[0] = put_ilit(MARG1_GBL);
		dqins(tmpchain.exorder.bl, exorder, ref);
                break;
        case TK_ATSIGN:
                if (!indirection(&mopr))
                        return FALSE;
		if (window_token != TK_EQUAL)
		{
			ref = newtriple(OC_COMMARG);
			ref->operand[0] = mopr;
                	ref->operand[1] = put_ilit((mint) indir_merge);
			return TRUE;
		}
		type = MARG1_LCL | MARG1_GBL;
		MV_FORCE_MVAL(&mv, type);
		MV_FORCE_STRD(&mv);
                ref = maketriple(OC_INDMERGE);
                ref->operand[0] = put_lit(&mv);
                ref->operand[1] = mopr;
		/* we insert left hand side argument into tmpchain. */
		dqins(tmpchain.exorder.bl, exorder, ref);
                break;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}

	if (window_token != TK_EQUAL)
	{
		stx_error(ERR_EQUAL);
		return FALSE;
	}
	advancewindow();

	/* Right Hand Side of EQUAL sign */
	switch (window_token)
	{
        case TK_IDENT:
                if (!lvn(&mopr, OC_M_SRCHINDX, 0))
                        return FALSE;
		ref = newtriple(OC_MERGE_LVARG);
		ref->operand[0] = put_ilit(MARG2_LCL);
		ref->operand[1] = mopr;
		break;
        case TK_CIRCUMFLEX:
                if (!gvn())
                        return FALSE;
		ref = newtriple(OC_MERGE_GVARG);
		ref->operand[0] = put_ilit(MARG2_GBL);
		break;
        case TK_ATSIGN:
                if (!indirection(&mopr))
		{
			stx_error(ERR_VAREXPECTED);
                        return FALSE;
		}
		type = MARG2_LCL | MARG2_GBL;
		MV_FORCE_MVAL(&mv, type);
		MV_FORCE_STRD(&mv);
                ref = maketriple(OC_INDMERGE);
                ref->operand[0] =  put_lit(&mv);
                ref->operand[1] = mopr;
                ins_triple(ref);
		break;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	/*
	 * Make sure that during runtime right hand side argument is processed first.
	 * This is specially important if global naked variable is used .
	 */
	obp = curtchain->exorder.bl;
	dqadd(obp, &tmpchain, exorder);
	ref = newtriple(OC_MERGE);
	return TRUE;
}
