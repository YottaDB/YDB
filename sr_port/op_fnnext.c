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

#include "lv_val.h"

void op_fnnext(lv_val *src, mval *key, mval *dst)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!TREF(in_op_fnnext));
	TREF(in_op_fnnext) = TRUE;
	op_fnorder(src, key, dst);
	assert(!TREF(in_op_fnnext)); /* should have been reset by op_fnorder */
	TREF(in_op_fnnext) = FALSE;
}
