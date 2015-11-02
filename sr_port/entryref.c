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

#include "gtm_string.h"

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "mlabel2xtern.h"
#include "mrout2xtern.h"
#include "gtmimagename.h"
#include <rtnhdr.h>
#include "stack_frame.h"

GBLREF stack_frame	*frame_pointer;
GBLREF mident		routine_name;

error_def(ERR_LABELEXPECTED);
error_def(ERR_RTNNAME);

triple *entryref(opctype op1, opctype op2, mint commargcode, boolean_t can_commarg, boolean_t labref, boolean_t textname)
{
	oprtype 	offset, label, routine, rte1;
	char		rtn_text[SIZEOF(mident_fixed)], lab_text[SIZEOF(mident_fixed)];
	mident		rtnname, labname;
	mstr 		rtn_str, lbl_str;
	triple 		*ref, *next, *rettrip;
	boolean_t	same_rout;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	rtnname.len = labname.len = 0;
	rtnname.addr = &rtn_text[0];
	labname.addr = &lab_text[0];
	/* These cases don't currently exist but if they start to exist, the code in this
	 * routine needs to be revisited for proper operation as the textname conditions
	 * were assumed not to happen if can_commarg was FALSE (which it is in the one
	 * known use of textname TRUE - in m_zgoto).
	 */
	assert(!(can_commarg && textname));
	switch (TREF(window_token))
	{
	case TK_INTLIT:
		int_label();
		/* caution: fall through */
	case TK_IDENT:
		memcpy(labname.addr, (TREF(window_ident)).addr, (TREF(window_ident)).len);
		labname.len = (TREF(window_ident)).len;
		advancewindow();
		if ((TK_PLUS != TREF(window_token)) && (TK_CIRCUMFLEX != TREF(window_token)) && !IS_MCODE_RUNNING && can_commarg)
		{
			rettrip = newtriple(op1);
			rettrip->operand[0] =  put_mlab(&labname);
			return rettrip;
		}
		label.oprclass = NO_REF;
		break;
	case TK_ATSIGN:
		if(!indirection(&label))
			return NULL;
		if ((TK_PLUS != TREF(window_token)) && (TK_CIRCUMFLEX != TREF(window_token)) && (TK_COLON != TREF(window_token))
		    && can_commarg)
		{
			rettrip = ref = maketriple(OC_COMMARG);
			ref->operand[0] = label;
			ref->operand[1] = put_ilit(commargcode);
			ins_triple(ref);
			return rettrip;
		}
		labname.len = 0;
		break;
	case TK_PLUS:
		stx_error(ERR_LABELEXPECTED);
		return NULL;
	default:
		labname.len = 0;
		label.oprclass = NO_REF;
		break;
	}
	if (!labref && (TK_PLUS == TREF(window_token)))
	{	/* Have line offset specified */
		advancewindow();
		if (EXPR_FAIL == expr(&offset, MUMPS_INT))
			return NULL;
	} else
		offset.oprclass = NO_REF;
	if (TK_CIRCUMFLEX == TREF(window_token))
	{	/* Have a routine name specified */
		advancewindow();
		switch (TREF(window_token))
		{
		case TK_IDENT:
			MROUT2XTERN((TREF(window_ident)).addr, rtnname.addr, (TREF(window_ident)).len);
			rtn_str.len = rtnname.len = (TREF(window_ident)).len;
			rtn_str.addr = rtnname.addr;
			advancewindow();
			if (!IS_MCODE_RUNNING)
			{	/* Triples for indirect code */
				same_rout = (MIDENT_EQ(&rtnname, &routine_name) && can_commarg);
				if (!textname)
				{	/* Resolve routine and label names to addresses for most calls */
					if (!label.oprclass && !offset.oprclass)
					{	/* Routine only (no label or offset) */
						if (same_rout)
						{
							rettrip = newtriple(op1);
							rettrip->operand[0] =  put_mlab(&labname);
						} else
						{
							rettrip = maketriple(op2);
							if (rtnname.addr[0] == '%')
								rtnname.addr[0] = '_';
							rettrip->operand[0] = put_cdlt(&rtn_str);
							mlabel2xtern(&lbl_str, &rtnname, &labname);
							rettrip->operand[1] = put_cdlt(&lbl_str);
							ins_triple(rettrip);
						}
						return rettrip;
					} else if (!same_rout)
					{
						rte1 = put_str(rtn_str.addr, rtn_str.len);
						if (rtnname.addr[0] == '%')
							rtnname.addr[0] = '_';
						routine = put_cdlt(&rtn_str);
						ref = newtriple(OC_RHDADDR);
						ref->operand[0] = rte1;
						ref->operand[1] = routine;
						routine = put_tref(ref);
					} else
						routine = put_tref(newtriple(OC_CURRHD));
				} else
				{	/* Return the actual names used */
					if (!label.oprclass && !offset.oprclass)
					{	/* Routine only (no label or offset) */
						rettrip = maketriple(op2);
						rettrip->operand[0] = put_str(rtn_str.addr, rtn_str.len);
						ref = newtriple(OC_PARAMETER);
						ref->operand[0] = put_str(labname.addr, labname.len);
						ref->operand[1] = put_ilit(0);
						rettrip->operand[1] = put_tref(ref);
						ins_triple(rettrip);
						return rettrip;
					} else
						routine = put_str(rtn_str.addr, rtn_str.len);
				}
			} else
			{	/* Triples for normal compiled code */
				routine = put_str(rtn_str.addr, rtn_str.len);
				if (!textname)
				{	/* If not returning text name, convert text name to routine header address */
					ref = newtriple(OC_RHDADDR1);
					ref->operand[0] = routine;
					routine = put_tref(ref);
				}
			}
			break;
		case TK_ATSIGN:
			if (!indirection(&routine))
				return NULL;
			if (!textname)
			{	/* If not returning text name, convert text name to routine header address */
				ref = newtriple(OC_RHDADDR1);
				ref->operand[0] = routine;
				routine = put_tref(ref);
			}
			break;
		default:
			stx_error(ERR_RTNNAME);
			return NULL;
		}
	} else
	{
		if (!label.oprclass && (0 == labname.len))
		{
			stx_error(ERR_LABELEXPECTED);
			return NULL;
		}
		if (!textname)
			routine = put_tref(newtriple(OC_CURRHD));
		else
		{	/* If we need a name, the mechanism to retrieve it differs between normal and indirect compilation */
			if (!IS_MCODE_RUNNING)
				/* For normal compile, use routine name set when started compile */
				routine = put_str(routine_name.addr, routine_name.len);
			else
				/* For an indirect compile, obtain the currently running routine header and pull the routine
				 * name out of that.
				 */
				routine = put_str(frame_pointer->rvector->routine_name.addr,
						  frame_pointer->rvector->routine_name.len);
		}
	}
	if (!offset.oprclass)
		offset = put_ilit(0);
	if (!label.oprclass)
		label = put_str(labname.addr, labname.len);
	ref = textname ? newtriple(OC_PARAMETER) : newtriple(OC_LABADDR);
	ref->operand[0] = label;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = offset;
	if (!textname)
		next->operand[1] = routine;	/* Not needed if giving text names */
	rettrip = next = newtriple(op2);
	next->operand[0] = routine;
	next->operand[1] = put_tref(ref);
	return rettrip;
}
