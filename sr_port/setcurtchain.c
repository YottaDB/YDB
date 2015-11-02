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

GBLREF	int4		pending_errtriplecode;	/* if non-zero contains the error code to invoke ins_errtriple with */
GBLREF	triple		t_orig;

triple *setcurtchain(triple *x)
{
	triple	*y;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	y = TREF(curtchain);
	TREF(curtchain) = x;
	if (pending_errtriplecode && (TREF(curtchain) == &t_orig))
	{	/* A compile error was seen while curtchain was temporarily switched and hence an ins_errtriple did not
		 * insert a OC_RTERROR triple then. Now that curtchain is back in the same chain as pos_in_chain, reissue
		 * the ins_errtriple call.
		 */
		 assert(!IS_STX_WARN(pending_errtriplecode) GTMTRIG_ONLY( || TREF(trigger_compile)));
		 ins_errtriple(pending_errtriplecode);
		 pending_errtriplecode = 0;
	}
	return y;
}
