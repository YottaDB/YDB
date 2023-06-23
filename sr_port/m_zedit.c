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
#include "svnames.h"
#include "advancewindow.h"
#include "cmd.h"

int m_zedit(void)
{

	int	rval;
	oprtype	file,opts;
	triple	*ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TK_EOL == TREF(window_token)) || (TK_SPACE == TREF(window_token)) || (TK_COLON == TREF(window_token)))
	{
		ref = newtriple(OC_SVGET);
		ref->operand[0] = put_ilit(SV_ZSOURCE);
		file = put_tref(ref);
		if (TK_COLON == TREF(window_token))
		{
			advancewindow();
			if (EXPR_FAIL == expr(&opts, MUMPS_STR))
				return FALSE;
		} else
			opts = put_str("",0);
	} else
	{
		if (EXPR_FAIL == (rval = expr(&file, MUMPS_STR)))	/* NOTE assignment */
			return FALSE;
		if (TK_COLON != TREF(window_token))
		{
			if (EXPR_INDR == rval)
			{
				make_commarg(&file,indir_zedit);
				return TRUE;
			}
			opts = put_str("",0);
		} else
		{
			advancewindow();
			if (EXPR_FAIL == expr(&opts, MUMPS_STR))
				return FALSE;
		}
	}
	ref = newtriple(OC_ZEDIT);
	ref->operand[0] = file;
	ref->operand[1] = opts;
	return TRUE;
}
