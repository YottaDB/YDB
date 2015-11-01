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
#include "advancewindow.h"
#include "cmd.h"

GBLREF char window_token;

int m_zhelp (void)
{
	triple		*ref;
	int		rval;
	oprtype		text, lib;

	if (window_token == TK_EOL || window_token == TK_SPACE || window_token == TK_COLON)
	{
		text = put_str("",0);
		if (window_token == TK_COLON)
		{
			advancewindow();
			if (!strexpr(&lib))
				return FALSE;
		}
		else
			lib = put_str("",0);
	}
	else
	{
		if (!(rval = strexpr(&text)))
			return FALSE;
		if (window_token != TK_COLON)
		{
			if (rval == EXPR_INDR)
			{
				make_commarg(&text,indir_zhelp);
				return TRUE;
			}
			lib = put_str("",0);
		}
		else
		{
			advancewindow();
			if (!strexpr(&lib))
				return FALSE;
		}
	}
	ref = newtriple(OC_ZHELP);
	ref->operand[0] = text;
	ref->operand[1] = lib;
	return TRUE;
}
