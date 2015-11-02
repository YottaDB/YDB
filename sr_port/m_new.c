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
#include "svnames.h"
#include "nametabtyp.h"
#include "funsvn.h"
#include "advancewindow.h"
#include "cmd.h"
#include "namelook.h"

GBLREF triple 		*curr_fetch_trip, *curr_fetch_opr;
GBLREF int4 		curr_fetch_count;

LITREF unsigned char 	svn_index[];
LITREF nametabent 	svn_names[];
LITREF svn_data_type 	svn_data[];

error_def(ERR_INVSVN);
error_def(ERR_RPARENMISSING);
error_def(ERR_SVNEXPECTED);
error_def(ERR_SVNONEW);
error_def(ERR_VAREXPECTED);

int m_new(void)
{
	oprtype		tmparg;
	triple		*ref, *next, *org, *tmp, *s, *fetch;
	int		n;
	int		count;
	mvar		*var;
	boolean_t	parse_warn;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (TREF(window_token))
	{
	case TK_IDENT:
		var = get_mvaddr(&(TREF(window_ident)));
		if (var->last_fetch != curr_fetch_trip)
		{
			fetch = newtriple(OC_PARAMETER);
			curr_fetch_opr->operand[1] = put_tref(fetch);
			fetch->operand[0] = put_ilit(var->mvidx);
			curr_fetch_count++;
			curr_fetch_opr = fetch;
			var->last_fetch = curr_fetch_trip;
		}
		tmp = maketriple(OC_NEWVAR);
		tmp->operand[0] = put_ilit(var->mvidx);
		ins_triple(tmp);
		advancewindow();
		return TRUE;
	case TK_ATSIGN:
		if (!indirection(&tmparg))
			return FALSE;
		ref = maketriple(OC_COMMARG);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit((mint) indir_new);
		ins_triple(ref);
		start_fetches(OC_FETCH);
		return TRUE;
	case TK_DOLLAR:
		advancewindow();
		if (TK_IDENT == TREF(window_token))
		{
			parse_warn = FALSE;
			if ((0 <= (n = namelook(svn_index, svn_names, (TREF(window_ident)).addr, (TREF(window_ident)).len))))
			{	/* NOTE assignment above */
				switch (svn_data[n].opcode)
				{
				case SV_ZTRAP:
				case SV_ETRAP:
				case SV_ESTACK:
				case SV_ZYERROR:
				case SV_ZGBLDIR:
				GTMTRIG_ONLY(case SV_ZTWORMHOLE:)
					tmp = maketriple(OC_NEWINTRINSIC);
					tmp->operand[0] = put_ilit(svn_data[n].opcode);
					break;
				default:
						STX_ERROR_WARN(ERR_SVNONEW);	/* sets "parse_warn" to TRUE */
				}
			} else
			{
				STX_ERROR_WARN(ERR_INVSVN);	/* sets "parse_warn" to TRUE */
			}
			advancewindow();
			if (!parse_warn)
				ins_triple(tmp);
			else
			{	/* OC_RTERROR triple would have been inserted in curtchain by ins_errtriple
				 * (invoked by stx_error). No need to do anything else.
				 */
				assert(OC_RTERROR == (TREF(curtchain))->exorder.bl->exorder.bl->exorder.bl->opcode);
			}
			return TRUE;
		}
		stx_error(ERR_SVNEXPECTED);
		return FALSE;
	case TK_EOL:
	case TK_SPACE:
		tmp = maketriple(OC_XNEW);
		tmp->operand[0] = put_ilit((mint) 0);
		ins_triple(tmp);
		if (TREF(for_stack_ptr) == TADR(for_stack))
			start_fetches (OC_FETCH);
		else
			start_for_fetches ();
		return TRUE;
	case TK_LPAREN:
		ref = org = maketriple(OC_XNEW);
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
		org->operand[0] = put_ilit((mint) count);
		ins_triple(org);
		if (TREF(for_stack_ptr) == TADR(for_stack))
			start_fetches (OC_FETCH);
		else
			start_for_fetches ();
		return TRUE;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
}
