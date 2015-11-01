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

#include "gtm_string.h"

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "mlabel2xtern.h"
#include "mrout2xtern.h"

GBLREF bool run_time;
GBLREF char window_token;
GBLREF mident window_ident;
GBLREF char routine_name[];

triple *entryref(opctype op1, opctype op2, mint commargcode, boolean_t can_commarg, boolean_t labref)
{
	oprtype offset,label,routine,rte1;
	mident labname,rtnname;
	mstr rtn_str, lbl_str;
	triple *ref,*next,*rettrip;
	error_def(ERR_RTNNAME);
	error_def(ERR_LABELEXPECTED);

	switch (window_token)
	{
	case TK_INTLIT:
		int_label();
		/* caution: fall through */
	case TK_IDENT:
		memcpy(labname.c, window_ident.c, sizeof(mident));
		advancewindow();
		if (window_token != TK_PLUS && window_token != TK_CIRCUMFLEX && !run_time && can_commarg)
		{
			rettrip = newtriple(op1);
			rettrip->operand[0] =  put_mlab(&labname);
			return rettrip;
		}
		label.oprclass = 0;
		break;
	case TK_ATSIGN:
		if(!indirection(&label))
			return 0;
		if (window_token != TK_PLUS && window_token != TK_CIRCUMFLEX && window_token != TK_COLON && can_commarg)
		{
			rettrip = ref = maketriple(OC_COMMARG);
			ref->operand[0] = label;
			ref->operand[1] = put_ilit(commargcode);
			ins_triple(ref);
			return rettrip;
		}
		memset(&labname.c[0],0,sizeof(mident));
		break;
	case TK_PLUS:
		stx_error(ERR_LABELEXPECTED);
		return 0;
	default:
		memset(&labname.c[0],0,sizeof(mident));
		label.oprclass = 0;
		break;
	}
	if (!labref && window_token == TK_PLUS)
	{
		advancewindow();
		if (!intexpr(&offset))
			return 0;
	} else
		offset.oprclass = 0;
	if (window_token == TK_CIRCUMFLEX)
	{
		advancewindow();
		switch(window_token)
		{
		case TK_IDENT:
			mrout2xtern((uchar_ptr_t)&window_ident.c[0], (uchar_ptr_t)&rtnname.c[0]);
			advancewindow();
			rtn_str.addr = rtnname.c;
			rtn_str.len = mid_len(&rtnname);
			if (!run_time)
			{	bool	same_rout;

				same_rout = (!memcmp(&rtnname, routine_name, sizeof(mident)) && can_commarg);

				if (!label.oprclass && !offset.oprclass)
				{
					if (same_rout)
					{	rettrip = newtriple(op1);
						rettrip->operand[0] =  put_mlab(&labname);
					}
					else
					{
						rettrip = maketriple(op2);
						if (rtnname.c[0] == '%')
							rtnname.c[0] = '_';
						rettrip->operand[0] = put_cdlt(&rtn_str);
						mlabel2xtern(&lbl_str, &rtnname, &labname);
						rettrip->operand[1] = put_cdlt(&lbl_str);
						ins_triple(rettrip);
					}
					return rettrip;
				}
				else if (!same_rout)
				{
					rte1 = put_str(rtn_str.addr, rtn_str.len);
					if (rtnname.c[0] == '%')
						rtnname.c[0] = '_';
					routine = put_cdlt(&rtn_str);
					ref = newtriple(OC_RHDADDR);
					ref->operand[0] = rte1;
					ref->operand[1] = routine;
					routine = put_tref(ref);
				}
				else
					routine = put_tref(newtriple(OC_CURRHD));

			}
			else
			{	routine = put_str(rtn_str.addr, rtn_str.len);
				ref = newtriple(OC_RHDADDR1);
				ref->operand[0] = routine;
				routine = put_tref(ref);
			}
			break;
		case TK_ATSIGN:
			if (!indirection(&routine))
				return 0;
			ref = newtriple(OC_RHDADDR1);
			ref->operand[0] = routine;
			routine = put_tref(ref);
			break;
		default:
			stx_error(ERR_RTNNAME);
			return 0;
		}
	}
	else
	{
		if (!label.oprclass && labname.c[0] == 0)
		{	stx_error(ERR_LABELEXPECTED);
			return 0;
		}
		routine = put_tref(newtriple(OC_CURRHD));
	}
	if (!offset.oprclass)
		offset = put_ilit(0);
	if (!label.oprclass)
		label = put_str(labname.c,sizeof(mident));
	ref = newtriple(OC_LABADDR);
	ref->operand[0] = label;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = offset;
	next->operand[1] = routine;
	rettrip = next = newtriple(op2);
	next->operand[0] = routine;
	next->operand[1] = put_tref(ref);
	return rettrip;
}
