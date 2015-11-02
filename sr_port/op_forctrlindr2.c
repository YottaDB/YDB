/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"
#include "lv_val.h"

LITREF	mval literal_one;
LITREF	mval literal_zero;

/* This provides a second return from op_indlvadr
 * it's built to provide communication at both ends but currently only used in m_for
 */
boolean_t op_forctrlindr2(uint4 jeopardy)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NOARG != jeopardy)
		TREF(for_ctrl_indr_subs) = jeopardy;
	jeopardy = TREF(for_ctrl_indr_subs) ? TRUE : FALSE;
	TREF(for_ctrl_indr_subs) = FALSE;
	return jeopardy;
}
