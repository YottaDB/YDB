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
#include "gtm_savetraps.h"
#include "gtm_newintrinsic.h"

GBLREF mval	dollar_ztrap;
GBLREF mval	dollar_etrap;

/* Routine called when we need to save the current Xtrap (etrap or ztrap) but
   don't know which to save.
*/
void gtm_savetraps(void)
{
	mval	*intrinsic;

	if (dollar_ztrap.str.len)
		intrinsic = &dollar_ztrap;
	else
		intrinsic = &dollar_etrap;
	gtm_newintrinsic(intrinsic);
        return;
}
