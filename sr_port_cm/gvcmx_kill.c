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
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gvcmx.h"
#include "gvcmz.h"

void gvcmx_kill(bool do_subtree)
{
	if (do_subtree)
		gvcmz_doop(CMMS_Q_KILL, CMMS_R_KILL, 0);
	else
		gvcmz_doop(CMMS_Q_ZWITHDRAW, CMMS_R_ZWITHDRAW, 0);
}
