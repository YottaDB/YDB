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
#include "indir_enum.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "cmd.h"

int m_zhelp (void)
{
	int		rval;
	oprtype		lib, text;
	triple		*ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TK_EOL == TREF(window_token)) || (TK_SPACE == TREF(window_token)) || (TK_COLON == TREF(window_token)))
	{
		text = put_str("",0);
		if (TK_COLON == TREF(window_token))
		{
			advancewindow();
			if (EXPR_FAIL == expr(&lib, MUMPS_STR))
				return FALSE;
		} else
			lib = put_str("",0);
	} else
	{
		if (EXPR_FAIL == (rval = expr(&text, MUMPS_STR)))	/* NOTE asignment */
			return FALSE;
		if (TK_COLON != TREF(window_token))
		{
			if (EXPR_INDR == rval)
			{
				make_commarg(&text,indir_zhelp);
				return TRUE;
			}
			lib = put_str("",0);
		} else
		{
			advancewindow();
			if (EXPR_FAIL == expr(&lib, MUMPS_STR))
				return FALSE;
		}
	}
	ref = newtriple(OC_ZHELP);
	ref->operand[0] = text;
	ref->operand[1] = lib;
	return TRUE;
}
