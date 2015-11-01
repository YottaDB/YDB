/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "iotimer.h"
#include "mdq.h"
#include "advancewindow.h"
#include "cmd.h"
#include "rwformat.h"

GBLREF char window_token;
GBLREF mval window_mval;
GBLREF bool temp_subs;
GBLREF triple *curtchain;

int m_read(void)
{
	oprtype x, *timeout, *sb1;
	opctype read_oc, put_oc;
	triple *ref, tmpchain, *sub, *s0, *s1, *put;
	bool local;
	error_def(ERR_RWARG);

	temp_subs = FALSE;
	local = TRUE;
	sub = (triple *)0;
	dqinit(&tmpchain, exorder);
	switch(window_token)
	{
	case TK_ASTERISK:
		advancewindow();
		switch(window_token)
		{
		default:
		case TK_IDENT:
			if (!lvn(&x, OC_PUTINDX, 0))
				return FALSE;
			if (OC_PUTINDX == x.oprval.tref->opcode)
			{
				dqdel(x.oprval.tref, exorder);
				dqins(tmpchain.exorder.bl, exorder, x.oprval.tref);
				sub = x.oprval.tref;
				put_oc = OC_PUTINDX;
			}
			put = maketriple(OC_STO);
			put->operand[0] = x;
			dqins(tmpchain.exorder.bl, exorder, put);
			break;
		case TK_CIRCUMFLEX:
			local = FALSE;
			s1 = curtchain->exorder.bl;
			if (!gvn())
				return FALSE;
			for (sub = curtchain->exorder.bl;  sub != s1;  sub = sub->exorder.bl)
			{
				put_oc = sub->opcode;
				if ((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc) || (OC_GVEXTNAM == put_oc))
					break;
			}
			assert((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc) || (OC_GVEXTNAM == put_oc));
			dqdel(sub, exorder);
			dqins(tmpchain.exorder.bl, exorder, sub);
			put = maketriple(OC_GVPUT);
			dqins(tmpchain.exorder.bl, exorder, put);
			break;
		case TK_ATSIGN:
			if (!indirection(&x))
				return FALSE;
			put = maketriple(OC_INDSET);
			put->operand[0] = x;
			dqins(tmpchain.exorder.bl, exorder, put);
			break;
		}
		if (TK_HASH == window_token)
		{
			stx_error(ERR_RWARG);
			return FALSE;
		}
		read_oc = OC_RDONE;
		break;
	case TK_QUESTION:
	case TK_EXCLAIMATION:
	case TK_HASH:
	case TK_SLASH:
		return rwformat();
	case TK_STRLIT:
		x = put_lit(&window_mval);
		advancewindow();
		ref = newtriple(OC_WRITE);
		ref->operand[0] = x;
		return TRUE;
	case TK_IDENT:
		if (!lvn(&x, OC_PUTINDX, 0))
			return FALSE;
		read_oc = OC_READ;
		if (OC_PUTINDX == x.oprval.tref->opcode)
		{
			dqdel(x.oprval.tref, exorder);
			dqins(tmpchain.exorder.bl, exorder, x.oprval.tref);
			sub = x.oprval.tref;
			put_oc = OC_PUTINDX;
		}
		put = maketriple(OC_STO);
		put->operand[0] = x;
		dqins(tmpchain.exorder.bl, exorder, put);
		break;
	case TK_CIRCUMFLEX:
		local = FALSE;
		read_oc = OC_READ;
		s1 = curtchain->exorder.bl;
		if (!gvn())
			return FALSE;
		for (sub = curtchain->exorder.bl;  sub != s1;  sub = sub->exorder.bl)
		{
			put_oc = sub->opcode;
			if ((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc) || (OC_GVEXTNAM == put_oc))
				break;
		}
		assert((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc) || (OC_GVEXTNAM == put_oc));
		dqdel(sub, exorder);
		dqins(tmpchain.exorder.bl, exorder, sub);
		put = maketriple(OC_GVPUT);
		dqins(tmpchain.exorder.bl, exorder, put);
		break;
	case TK_ATSIGN:
		if (!indirection(&x))
			return FALSE;
		if ((TK_COLON != window_token) && (TK_HASH != window_token))
		{
			ref = maketriple(OC_COMMARG);
			ref->operand[0] = x;
			ref->operand[1] = put_ilit(indir_read);
			ins_triple(ref);
			return TRUE;
		}
		put = maketriple(OC_INDSET);
		put->operand[0] = x;
		dqins(tmpchain.exorder.bl, exorder, put);
		read_oc = OC_READ;
		break;
	default:
		stx_error(ERR_RWARG);
		return FALSE;
	}
	if (TK_HASH == window_token)
	{
		advancewindow();
		ref = maketriple(OC_READFL);
		if (!intexpr(&ref->operand[0]))
			return FALSE;
		timeout = &ref->operand[1];
	} else
	{
		ref = maketriple(read_oc);
		timeout = &ref->operand[0];
	}
	if (TK_COLON != window_token)
	{
		*timeout = put_ilit(NO_M_TIMEOUT);
		ins_triple(ref);
	} else
	{
		advancewindow();
		if (!intexpr(timeout))
			return FALSE;
		ins_triple(ref);
		newtriple(OC_TIMTRU);
	}
	if (local)
		put->operand[1] = put_tref(ref);
	else
		put->operand[0] = put_tref(ref);

	ref = curtchain->exorder.bl;
	dqadd(ref, &tmpchain, exorder);		/*this is a violation of info hiding*/

	if (sub)
	{
		sb1 = &sub->operand[1];
		if ((OC_GVNAME == put_oc) || (OC_PUTINDX == put_oc))
		{
			sub = sb1->oprval.tref;		/* global name */
			assert(OC_PARAMETER == sub->opcode);
			sb1 = &sub->operand[1];
		} else  if (OC_GVEXTNAM == put_oc)
		{
			sub = sb1->oprval.tref;		/* first env */
			assert(OC_PARAMETER == sub->opcode);
			sb1 = &sub->operand[0];
			assert(TRIP_REF == sb1->oprclass);
			s0 = sb1->oprval.tref;
			if ((temp_subs && (OC_GETINDX == s0->opcode)) || (OC_VAR == s0->opcode))
			{
				s1 = maketriple(OC_STOTEMP);
				s1->operand[0] = *sb1;
				*sb1 = put_tref(s1);
				dqins(s0->exorder.bl, exorder, s1);
			}
			sb1 = &sub->operand[1];
			sub = sb1->oprval.tref;		/* second env */
			assert(OC_PARAMETER == sub->opcode);
			sb1 = &sub->operand[0];
			assert(TRIP_REF == sb1->oprclass);
			s0 = sb1->oprval.tref;
			if ((temp_subs && (OC_GETINDX == s0->opcode)) || (OC_VAR == s0->opcode))
			{
				s1 = maketriple(OC_STOTEMP);
				s1->operand[0] = *sb1;
				*sb1 = put_tref(s1);
				dqins(s0->exorder.bl, exorder, s1);
			}
			sb1 = &sub->operand[1];
			sub = sb1->oprval.tref;		/* global name */
			assert(OC_PARAMETER == sub->opcode);
			sb1 = &sub->operand[1];
		}
		while(sb1->oprclass)
		{
			assert(TRIP_REF == sb1->oprclass);
			sub = sb1->oprval.tref;
			assert(OC_PARAMETER == sub->opcode);
			sb1 = &sub->operand[0];
			assert(TRIP_REF == sb1->oprclass);
			s0 = sb1->oprval.tref;
			if (temp_subs && ((OC_GETINDX == s0->opcode) || (OC_VAR == s0->opcode)))
			{
				s1 = maketriple(OC_STOTEMP);
				s1->operand[0] = *sb1;
				*sb1 = put_tref(s1);
				s0 = s0->exorder.fl;
				dqins(s0->exorder.bl, exorder, s1);
			} else  if (OC_VAR == s0->opcode)
			{
				s1 = maketriple(OC_NAMECHK);
				s1->operand[0] = *sb1;
				s0 = s0->exorder.fl;
				dqins(s0->exorder.bl, exorder, s1);
			}
			sb1 = &sub->operand[1];
		}
	}
	return TRUE;
}
