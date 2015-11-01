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
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "svnames.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF char window_token;

int m_zedit(void)
{

	int4 rval;
	triple *ref;
	oprtype file,opts;

	if (window_token == TK_EOL || window_token == TK_SPACE ||
		window_token == TK_COLON)
	{
		ref = newtriple(OC_SVGET);
		ref->operand[0] = put_ilit(SV_ZSOURCE);
		file = put_tref(ref);
		if (window_token == TK_COLON)
		{
			advancewindow();
			if (!strexpr(&opts))
				return FALSE;
		}
		else
			opts = put_str("",0);
	}
	else
	{
		if (!(rval = strexpr(&file)))
			return FALSE;
		if (window_token != TK_COLON)
		{
			if (rval == EXPR_INDR)
			{
				make_commarg(&file,indir_zedit);
				return TRUE;
			}
			opts = put_str("",0);
		}
		else
		{
			advancewindow();
			if (!strexpr(&opts))
				return FALSE;
		}
	}
	ref = newtriple(OC_ZEDIT);
	ref->operand[0] = file;
	ref->operand[1] = opts;
	return TRUE;
}
