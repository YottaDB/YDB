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
#include <ssdef.h>
#include <rms.h>
#include "job.h"

static unsigned char *deffsbuf;

void ojdefdeffs (mstr *deffs)
{
	int4		status;
	struct FAB	fab;
	struct NAM	nam;

	if (!deffsbuf)
		deffsbuf = malloc(MAX_FILSPC_LEN);
	fab = cc$rms_fab;
	nam = cc$rms_nam;
	fab.fab$l_nam = &nam;
	fab.fab$l_fna = "[]";
	fab.fab$b_fns = 2;
	nam.nam$l_esa = &deffsbuf[0];
	nam.nam$b_ess = MAX_FILSPC_LEN;
	status = sys$parse (&fab);
	if (status != RMS$_NORMAL) rts_error(VARLSTCNT(1) status);

	deffs->len = nam.nam$b_esl - 2;
	deffs->addr = &deffsbuf[0];
	return;
}
