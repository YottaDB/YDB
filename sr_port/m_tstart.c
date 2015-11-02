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
#include "opcode.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cmd.h"
#include "namelook.h"

static readonly nametabent tst_param_names[] =
{
	{1,"S"}
	,{6,"SERIAL"}

	,{1,"T"}
	,{8,"TRANSACT*"}
};
/* Offset of letter with dev_param_names */
static readonly unsigned char tst_param_index[27] =
{
/*	A    B    C    D    E    F    G    H    I    J    K    L    M    N   */
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
/*	O    P    Q    R    S    T    U    V    W    X    Y    Z    end	     */
	0,   0,   0,   0,   0,   2,   4,   4,   4,   4,   4,   4,   4
};

error_def(ERR_RPARENMISSING);
error_def(ERR_TSTRTPARM);
error_def(ERR_VAREXPECTED);

int m_tstart(void)
{
	boolean_t	bad_parse, has_lpar, has_ser, has_tid;
	int		count, n;
	mval		dummyid;
	oprtype		tmparg;
	triple		*fetch, *ref, *s, *ser, *tid, *varlst, *varnext, *varp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	count = -1;	/* indicates not restartable */
	varlst = newtriple(OC_PARAMETER);
	switch (TREF(window_token))
	{
	case TK_IDENT:
		count = 1;
		varlst->operand[1] = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
		advancewindow();
		break;
	case TK_ASTERISK:
		count = -2;		/* indicates save all variables */
		advancewindow();
		break;
	case TK_ATSIGN:
		if (!indirection(&tmparg))
			return FALSE;
		if (TK_COLON == TREF(window_token))
		{
			ref = newtriple(OC_COMMARG);
			ref->operand[0] = tmparg;
			ref->operand[1] = put_ilit((mint) indir_tstart);
			return TRUE;
		}
		else
		{	count = 1;
			fetch = newtriple(OC_INDLVARG);
			fetch->operand[0] = tmparg;
			varlst->operand[1] = put_tref(fetch);
		}
		break;
	case TK_EOL:
	case TK_SPACE:
		break;
	case TK_LPAREN:
		varp = varlst;
		for(count = 0; ;)
		{
			advancewindow();
			switch (TREF(window_token))
			{
			case TK_IDENT:
				varnext = newtriple(OC_PARAMETER);
				varnext->operand[0] = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
				varp->operand[1] = put_tref(varnext);
				varp = varnext;
				advancewindow();
				count++;
				break;
			case TK_ATSIGN:
				if (!indirection(&tmparg))
					return FALSE;
				s = newtriple(OC_INDLVARG);
				s->operand[0] = tmparg;
				varnext = newtriple(OC_PARAMETER);
				varnext->operand[0] = put_tref(s);
				varp->operand[1] = put_tref(varnext);
				varp = varnext;
				count++;
				break;
			case TK_RPAREN:
				break;
			default:
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			if (TK_COMMA != TREF(window_token))
				break;
		}
		if (TK_RPAREN != TREF(window_token))
		{
			stx_error(ERR_RPARENMISSING);
			return FALSE;
		}
		advancewindow();
		break;
	}
	varlst->operand[0] = put_ilit(count);
	has_ser = has_tid = FALSE;
	tid = newtriple(OC_PARAMETER);
	if (TK_COLON == TREF(window_token))
	{	advancewindow();
		if (has_lpar = (TK_LPAREN == TREF(window_token)))
			advancewindow();
		for(;;)
		{
			bad_parse = TRUE;
			if (TK_IDENT == TREF(window_token))
			{
				if ((0 <= (n = namelook(tst_param_index, tst_param_names, (TREF(window_ident)).addr,
					(TREF(window_ident)).len))))
				{	/* NOTE assignment above */
					if (n < 2 && !has_ser)
					{	has_ser = TRUE;
						bad_parse = FALSE;
						advancewindow();
					}
					else if (!has_tid)
					{
						advancewindow();
						if (TK_EQUAL == TREF(window_token))
						{	advancewindow();
							if (EXPR_FAIL == expr(&tid->operand[0], MUMPS_EXPR))
								return FALSE;
							has_tid = TRUE;
							bad_parse = FALSE;
						}
					}
				}
			}
			if (bad_parse)
			{
				stx_error(ERR_TSTRTPARM);
				return FALSE;
			}
			if (!has_lpar || (TK_RPAREN == TREF(window_token)))
				break;
			if (TK_COLON != TREF(window_token))
			{
				stx_error(ERR_TSTRTPARM);
				return FALSE;
			}
			advancewindow();
		}
		if (has_lpar)
		{
			assert(TK_RPAREN == TREF(window_token));
			advancewindow();
		}
	}
	if (!has_tid)
	{
		dummyid.mvtype = MV_STR;
		dummyid.str.len = 0;
		tid->operand[0] = put_lit(&dummyid);
	}
	tid->operand[1] = put_tref(varlst);
	ref = newtriple(OC_TSTART);
	ref->operand[0] = put_ilit(has_ser);
	ref->operand[1] = put_tref(tid);
	return TRUE;
}
