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

GBLREF char 	window_token;
GBLREF mident 	window_ident;

int m_tstart(void)
{
	triple *ser, *tid, *ref, *varlst, *varp, *varnext, *fetch, *s;
	oprtype tmparg;
	int	count, n;
	bool	has_ser, has_tid, has_lpar, bad_parse;
	mval	dummyid;
	error_def(ERR_RPARENMISSING);
	error_def(ERR_VAREXPECTED);
	error_def(ERR_TSTRTPARM);

	count = -1;	/* indicates not restartable */
	varlst = newtriple(OC_PARAMETER);
	switch (window_token)
	{
	case TK_IDENT:
		count = 1;
		varlst->operand[1] = put_str(window_ident.addr, window_ident.len);
		advancewindow();
		break;
	case TK_ASTERISK:
		count = -2;		/* indicates save all variables */
		advancewindow();
		break;
	case TK_ATSIGN:
		if (!indirection(&tmparg))
			return FALSE;
		if (window_token == TK_COLON)
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
			switch (window_token)
			{
			case TK_IDENT:
				varnext = newtriple(OC_PARAMETER);
				varnext->operand[0] = put_str(window_ident.addr, window_ident.len);
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
			if (window_token != TK_COMMA)
				break;
		}
		if (window_token != TK_RPAREN)
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
	if (window_token == TK_COLON)
	{	advancewindow();
		if (has_lpar = (window_token == TK_LPAREN))
			advancewindow();
		for(;;)
		{
			bad_parse = TRUE;
			if (window_token == TK_IDENT)
			{
				if ((n = namelook(tst_param_index, tst_param_names, window_ident.addr, window_ident.len)) >= 0)
				{
					if (n < 2 && !has_ser)
					{	has_ser = TRUE;
						bad_parse = FALSE;
						advancewindow();
					}
					else if (!has_tid)
					{
						advancewindow();
						if (window_token == TK_EQUAL)
						{	advancewindow();
							if (!expr(&tid->operand[0]))
								return FALSE;
							has_tid = TRUE;
							bad_parse = FALSE;
						}
					}
				}
			}
			if (bad_parse)
			{	stx_error(ERR_TSTRTPARM);
				return FALSE;
			}
			if (!has_lpar || window_token == TK_RPAREN)
				break;
			if (window_token != TK_COLON)
			{	stx_error(ERR_TSTRTPARM);
				return FALSE;
			}
			advancewindow();
		}
		if (has_lpar)
		{	assert(window_token == TK_RPAREN);
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
/*	ser = newtriple(OC_PARAMETER);
	ser->operand[0] = put_ilit(has_ser);
	ser->operand[1] = put_tref(tid);
	s = newtriple(OC_TSTARTPC);
	ref = newtriple(OC_TSTART);
	ref->operand[0] = put_tref(s);
	ref->operand[1] = put_tref(ser);
*/
	ref = newtriple(OC_TSTART);
	ref->operand[0] = put_ilit(has_ser);
	ref->operand[1] = put_tref(tid);

	return TRUE;
}
