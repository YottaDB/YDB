/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "start_fetches.h"

GBLREF	boolean_t	run_time;

LITREF	nametabent	svn_names[];
LITREF	svn_data_type	svn_data[];
LITREF	unsigned char	svn_index[];

error_def(ERR_INVSVN);
error_def(ERR_RPARENMISSING);
error_def(ERR_SVNEXPECTED);
error_def(ERR_SVNONEW);
error_def(ERR_VAREXPECTED);

int m_new(void)
{
	boolean_t	parse_warn;
	int		count, n;
	mvar		*var;
	oprtype		tmparg;
	triple		*fetch, *next, *org, *ref, *s, *tmp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(xecute_literal_parse))
		return FALSE;
	switch (TREF(window_token))
	{
	case TK_IDENT:
		var = get_mvaddr(&(TREF(window_ident)));
		if ((var->last_fetch != (TREF(fetch_control)).curr_fetch_trip) && !TREF(discard))
		{	/* This block is identical to one in put_mvar */
			fetch = newtriple(OC_PARAMETER);
			(TREF(fetch_control)).curr_fetch_opr->operand[1] = put_tref(fetch);
			fetch->operand[0] = put_ilit(var->mvidx);
			((TREF(fetch_control)).curr_fetch_count)++;
			(TREF(fetch_control)).curr_fetch_opr = fetch;
			var->last_fetch = (TREF(fetch_control)).curr_fetch_trip;
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
		MID_LINE_REFETCH;
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
				case SV_TEST:
				case SV_ZCMDLINE:
				GTMTRIG_ONLY(case SV_ZTWORMHOLE:)
					tmp = maketriple(OC_NEWINTRINSIC);
					tmp->operand[0] = put_ilit(svn_data[n].opcode);
					break;
				default:
					STX_ERROR_WARN(ERR_SVNONEW);	/* sets "parse_warn" to TRUE */
					tmp = NULL;
				}
			} else
			{
				STX_ERROR_WARN(ERR_INVSVN);	/* sets "parse_warn" to TRUE */
				tmp = NULL;
			}
			advancewindow();
			if (!parse_warn)
			{
				assert(tmp);
				ins_triple(tmp);
			} else if (run_time)
			{	/* OC_RTERROR triple would have been inserted in curtchain by ins_errtriple
				 * (invoked by stx_error). No need to do anything else in that case. But that
				 * happens only if "run_time" is TRUE and not if it is FALSE (e.g. $ZYCOMPILE).
				 * Hence the "else if (run_time)" check above.
				 */
				assert(ALREADY_RTERROR);
			}
			return TRUE;
		}
		stx_error(ERR_SVNEXPECTED);
		return FALSE;
	case TK_EOL:
	case TK_SPACE:
		/* This actually a NEW all, but an XNEW with no arguments does the job */
		tmp = maketriple(OC_XNEW);
		tmp->operand[0] = put_ilit((mint) 0);
		ins_triple(tmp);
		/* start a new fetch for whatever follows on the line */
		MID_LINE_REFETCH;
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
		/* start a new fetch for whatever follows on the line */
		MID_LINE_REFETCH;
		return TRUE;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
}
