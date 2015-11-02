/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "mlkdef.h"
#include "zshow.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF char window_token;

int m_zshow(void)
{
	static readonly char def_str[]="S";
	int	rval, code;
	triple	*r,*outtype,*lvar;
	oprtype func,output;
	error_def(ERR_VAREXPECTED);

	if (window_token == TK_SPACE || window_token == TK_EOL || window_token == TK_COLON)
	{
		code = ZSHOW_NOPARM;
	}else
	{
		code = ZSHOW_DEVICE;
		switch (strexpr(&func))
		{
		case EXPR_FAIL:
			return FALSE;
		case EXPR_GOOD:
			break;
		case EXPR_INDR:
			make_commarg(&func,indir_zshow);
			return TRUE;
			break;
		default:
			assert(FALSE);
		}
	}
	if (window_token == TK_COLON)
	{
		advancewindow();
		switch(window_token)
		{
		case TK_CIRCUMFLEX:
			if (!gvn())
				return FALSE;
			r = maketriple(OC_ZSHOW);
			outtype = newtriple(OC_PARAMETER);
			r->operand[1] = put_tref(outtype);
			if (code == ZSHOW_NOPARM)
			{
				r->operand[0] = put_str(&def_str[0], (SIZEOF(def_str) - 1));
			}else
			{
				r->operand[0] = func;
			}
			outtype->operand[0] = put_ilit(ZSHOW_GLOBAL);
			ins_triple(r);
			return TRUE;
		case TK_IDENT:
			if (!lvn(&output, OC_PUTINDX, 0))
			{
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			r = maketriple(OC_ZSHOWLOC);
			outtype = newtriple(OC_PARAMETER);
			r->operand[1] = put_tref(outtype);
			if (code == ZSHOW_NOPARM)
			{
				r->operand[0] = put_str(&def_str[0], (SIZEOF(def_str) - 1));
			}else
			{
				r->operand[0] = func;
			}
			lvar = newtriple(OC_PARAMETER);
			outtype->operand[1] = put_tref(lvar);
			lvar->operand[0] = output;
			outtype->operand[0] = put_ilit(ZSHOW_LOCAL);
			ins_triple(r);
			return TRUE;
		case TK_ATSIGN:
			if (!indirection(&output))
			{
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			r = newtriple(OC_INDRZSHOW);
			if (code == ZSHOW_NOPARM)
			{
				r->operand[0] = put_str(&def_str[0], (SIZEOF(def_str) - 1));
			}else
			{
				r->operand[0] = func;
			}
			r->operand[1] = output;
			return TRUE;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
	}
	r = maketriple(OC_ZSHOW);
	outtype = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(outtype);
	if (code == ZSHOW_NOPARM)
	{
		r->operand[0] = put_str(&def_str[0], (SIZEOF(def_str) - 1));
	}else
	{
		r->operand[0] = func;
	}
	outtype->operand[0] = put_ilit(ZSHOW_DEVICE);
	ins_triple(r);
	return TRUE;

}
