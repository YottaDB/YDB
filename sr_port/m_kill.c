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
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF char 	window_token;
GBLREF mident 	window_ident;

int m_kill(void)
{
oprtype tmparg;
triple *ref,*next,*org,*s;
int count;
error_def(ERR_RPARENMISSING);
error_def(ERR_VAREXPECTED);

switch (window_token)
	{
	case TK_IDENT:
		if (!lvn(&tmparg,OC_SRCHINDX,0))
			return FALSE;
		ref = newtriple(OC_KILL);
		ref->operand[0] = tmparg;
		break;
	case TK_CIRCUMFLEX:
		if (!gvn())
			return FALSE;
		ref = newtriple(OC_GVKILL);
		break;
	case TK_ATSIGN:
		if (!indirection(&tmparg))
			return FALSE;
		ref = maketriple(OC_COMMARG);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit((mint) indir_kill);
		ins_triple(ref);
		return TRUE;
	case TK_EOL:
	case TK_SPACE:
		newtriple(OC_KILLALL);
		break;
	case TK_LPAREN:
		ref = org = maketriple(OC_XKILL);
		count = 0;
		do
		{
			advancewindow();
			next = maketriple(OC_PARAMETER);
			ref->operand[1] = put_tref(next);
			switch (window_token)
			{
			case TK_IDENT:
				next->operand[0] = put_str(window_ident.addr, window_ident.len);
				advancewindow();
				break;
			case TK_ATSIGN:
				if (!indirection(&tmparg))
					return FALSE;
				s = newtriple(OC_INDLVARG);
				s->operand[0] = tmparg;
				next->operand[0] = put_tref(s);
				break;
			default:
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			ins_triple(next);
			ref = next;
			count++;
		} while (window_token == TK_COMMA);
		if (window_token != TK_RPAREN)
		{
			stx_error(ERR_RPARENMISSING);
			return FALSE;
		}
		advancewindow();
		org->operand[0] = put_ilit((mint) count);
		ins_triple(org);
		return TRUE;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
return TRUE;
}
