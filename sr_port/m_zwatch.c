/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include "advancewindow.h"
#include "cmd.h"

#define CANCEL_ONE -1
#define CANCEL_ALL -2

GBLREF char 	window_token;
GBLREF mident 	window_ident;
LITREF mident 	zero_ident;

int m_zwatch(void)
{

	triple *ref,*next;
	opctype op;
	oprtype name,action,count;
	bool is_count;
	error_def(ERR_VAREXPECTED);

	if (window_token == TK_MINUS)
	{
		advancewindow();
		switch(window_token)
		{
		case TK_ASTERISK:
			name = put_str(zero_ident.addr, zero_ident.len);
			count = put_ilit(CANCEL_ALL);
			advancewindow();
			break;
		case TK_IDENT:
			name = put_str(window_ident.addr, window_ident.len);
			count = put_ilit(CANCEL_ONE);
			advancewindow();
			break;
		case TK_ATSIGN:
			if (!indirection(&name))
				return FALSE;
			count = put_ilit(CANCEL_ONE);
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		action = put_str("",0);
		op = OC_WATCHREF;
	}
	else
	{
		if (window_token == TK_EQUAL)
		{
			advancewindow();
			op = OC_WATCHMOD;
		}
		else
			op = OC_WATCHREF;
		switch(window_token)
		{
		case TK_IDENT:
			name = put_str(window_ident.addr, window_ident.len);
			advancewindow();
			break;
		case TK_ATSIGN:
			if (!indirection(&name))
				return FALSE;
			if (op == OC_WATCHREF && window_token != TK_COLON)
			{
				ref = maketriple(OC_COMMARG);
				ref->operand[0] = name;
				ref->operand[1] = put_ilit((mint) indir_zwatch);
				ins_triple(ref);
				return TRUE;
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		if (window_token != TK_COLON)
		{
			action = put_str("",0);
			count = put_ilit(0);
		}
		else
		{
			advancewindow();
			if (window_token == TK_COLON)
			{
				is_count = TRUE;
				action = put_str("",0);
			}
			else
			{
				if (!strexpr(&action))
					return FALSE;
				is_count = window_token == TK_COLON;
			}
			if (is_count)
			{
				advancewindow();
				if (!intexpr(&count))
					return FALSE;
			}
			else
				count = put_ilit(0);
		}
	}
	ref = newtriple(op);
	ref->operand[0] = name;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = action;
	next->operand[1] = count;
	return TRUE;

}
