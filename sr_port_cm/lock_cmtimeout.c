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
#include "gt_timer.h"
#include "lock_cmtimeout.h"

GBLREF bool lk_cm_noresponse;

void lock_cmtimeout(void)
{
	lk_cm_noresponse = TRUE;
	GT_WAKE;
	return;
}
