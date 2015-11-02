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
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "nametabtyp.h"
#include "toktyp.h"
#include "merge_def.h"
#include "cmd.h"
#include "mvalconv.h"
#include "advancewindow.h"
#include "glvn_pool.h"

error_def(ERR_EQUAL);
error_def(ERR_RPARENMISSING);
error_def(ERR_VAREXPECTED);

int m_merge(void)
{
	int		type;
	boolean_t	used_glvn_slot;
	mval		mv;
	opctype 	put_oc;
	oprtype 	mopr, control_slot;
	triple		*obp, *ref, *restart, *s1, *sub, tmpchain;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	used_glvn_slot = FALSE;
	sub = NULL;
	restart = newtriple(OC_RESTARTPC);	/* Here is where a restart should pick up */
	dqinit(&tmpchain, exorder);
	/* Left Hand Side of EQUAL sign */
	switch (TREF(window_token))
	{
	case TK_IDENT:
		if (!lvn(&mopr, OC_PUTINDX, 0))
			return FALSE;
		if (OC_PUTINDX == mopr.oprval.tref->opcode)
		{	/* we insert left hand side argument into tmpchain. */
			sub = mopr.oprval.tref;
			put_oc = OC_PUTINDX;
			dqdel(mopr.oprval.tref, exorder);
			dqins(tmpchain.exorder.bl, exorder, mopr.oprval.tref);
		}
		ref = maketriple(OC_MERGE_LVARG);
		ref->operand[0] = put_ilit(MARG1_LCL);
		ref->operand[1] = mopr;
		dqins(tmpchain.exorder.bl, exorder, ref);
		break;
	case TK_CIRCUMFLEX:
		s1 = (TREF(curtchain))->exorder.bl;
		if (!gvn())
			return FALSE;
		for (sub = (TREF(curtchain))->exorder.bl; sub != s1; sub = sub->exorder.bl)
		{
			put_oc = sub->opcode;
			if (OC_GVNAME == put_oc || OC_GVNAKED == put_oc || OC_GVEXTNAM == put_oc)
				break;
		}
		assert((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc) || (OC_GVEXTNAM == put_oc));
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
		if (TK_EQUAL != TREF(window_token))
		{
			ref = newtriple(OC_COMMARG);
			ref->operand[0] = mopr;
			ref->operand[1] = put_ilit((mint) indir_merge);
			return TRUE;
		}
		type = MARG1_LCL | MARG1_GBL;
		MV_FORCE_MVAL(&mv, type);
		MV_FORCE_STRD(&mv);
		if (TREF(side_effect_handling))
		{	/* save and restore the variable lookup for true left-to-right evaluation */
			used_glvn_slot = TRUE;
			INSERT_INDSAVGLVN(control_slot, mopr, ANY_SLOT, 0);	/* 0 flag to defer global reference */
			ref = maketriple(OC_INDMERGE2);
			ref->operand[0] = control_slot;
		} else
		{	/* quick and dirty old way */
			ref = maketriple(OC_INDMERGE);
			ref->operand[0] = put_lit(&mv);
			ref->operand[1] = mopr;
		}
		/* we insert left hand side argument into tmpchain. */
		dqins(tmpchain.exorder.bl, exorder, ref);
		break;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	if (TREF(window_token) != TK_EQUAL)
	{
		stx_error(ERR_EQUAL);
		return FALSE;
	}
	advancewindow();
	/* Right Hand Side of EQUAL sign */
	TREF(temp_subs) = FALSE;
	switch (TREF(window_token))
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
		TREF(temp_subs) = TRUE;
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
	obp = (TREF(curtchain))->exorder.bl;
	dqadd(obp, &tmpchain, exorder);
	if (TREF(temp_subs) && TREF(side_effect_handling) && sub)
		create_temporaries(sub, put_oc);
	TREF(temp_subs) = FALSE;
	if (used_glvn_slot)
	{
		ref = newtriple(OC_GLVNPOP);
		ref->operand[0] = control_slot;
	}
	ref = newtriple(OC_MERGE);
	return TRUE;
}
