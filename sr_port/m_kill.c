/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

error_def(ERR_ALIASEXPECTED);
error_def(ERR_NOALIASLIST);
error_def(ERR_RPARENMISSING);
error_def(ERR_VAREXPECTED);

int m_kill(void)
{
	boolean_t	alias_processing;
	int		count;
	mvar		*mvarptr;
	oprtype		tmparg;
	triple		*next, *org, *ref, *s;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (alias_processing = (TK_ASTERISK == TREF(window_token)))	/* NOTE assignment */
		advancewindow();
	switch (TREF(window_token))
	{
	case TK_IDENT:
		/* If doing alias processing, we need to pass the index of the var rather than its lv_val
		   but do the common case first. Note that a kill of an alias container is handled the same
		   as the kill of any other regular local variable.
		*/
		if (!alias_processing || (TK_LPAREN == TREF(director_token)))
		{
			if (!lvn(&tmparg,OC_SRCHINDX,0))
				return FALSE;
			ref = newtriple(OC_KILL);
			ref->operand[0] = tmparg;
		} else
		{	/* alias (unsubscripted var) kill */
			ref = newtriple(OC_KILLALIAS);
			mvarptr = get_mvaddr(&(TREF(window_ident)));
			ref->operand[0] = put_ilit(mvarptr->mvidx);
			advancewindow();
		}
		break;
	case TK_CIRCUMFLEX:
		if (alias_processing)
		{
			stx_error(ERR_ALIASEXPECTED);
			return FALSE;
		}
		if (!gvn())
			return FALSE;
		ref = newtriple(OC_GVKILL);
		break;
	case TK_ATSIGN:
		if (alias_processing)
		{
			stx_error(ERR_ALIASEXPECTED);
			return FALSE;
		}
		if (!indirection(&tmparg))
			return FALSE;
		ref = maketriple(OC_COMMARG);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit((mint)indir_kill);
		ins_triple(ref);
		return TRUE;
	case TK_EOL:
	case TK_SPACE:
		newtriple(alias_processing ? OC_KILLALIASALL : OC_KILLALL);
		break;
	case TK_LPAREN:
		if (alias_processing)
		{
			stx_error(ERR_NOALIASLIST);
			return FALSE;
		}
		ref = org = maketriple(OC_XKILL);
		count = 0;
		do
		{
			advancewindow();
			next = maketriple(OC_PARAMETER);
			ref->operand[1] = put_tref(next);
			switch (TREF(window_token))
			{
			case TK_IDENT:
				next->operand[0] = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
				advancewindow();
				break;
			case TK_ATSIGN:
				if (!indirection(&tmparg))
					return FALSE;
				s = newtriple(OC_INDLVARG);
				s->operand[0] = tmparg;
				next->operand[0] = put_tref(s);
				break;
			case TK_ASTERISK:
				stx_error(ERR_NOALIASLIST);
				return FALSE;
			default:
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			ins_triple(next);
			ref = next;
			count++;
		} while (TK_COMMA == TREF(window_token));
		if (TK_RPAREN != TREF(window_token))
		{
			stx_error(ERR_RPARENMISSING);
			return FALSE;
		}
		advancewindow();
		org->operand[0] = put_ilit((mint)count);
		ins_triple(org);
		return TRUE;
	default:
		stx_error(alias_processing ? ERR_ALIASEXPECTED : ERR_VAREXPECTED);
		return FALSE;
	}
	return TRUE;
}
