/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_savetraps.h"
#include "gtm_newintrinsic.h"

GBLREF boolean_t	ztrap_explicit_null;

/* Routine called when we need to save the current Xtrap (etrap or ztrap) but
   don't know which to save.
*/
void gtm_savetraps(void)
{
	mval	*intrinsic;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TREF(dollar_ztrap)).str.len || ztrap_explicit_null)
		intrinsic = &(TREF(dollar_ztrap));
	else
		intrinsic = &(TREF(dollar_etrap));
	gtm_newintrinsic(intrinsic);
        return;
}
