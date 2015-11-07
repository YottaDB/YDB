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
#include <rms.h>
#include "io.h"
#include "job.h"

int4	ojchkfs (char *addr, int4 len, bool exist)
{
	int4		status;
	struct FAB	fab;
	struct NAM	nam;
	char		es[MAX_FILSPC_LEN];
	mstr		tn;			/* translated name 	  */
	enum io_dev_type dev_typ;

	fab = cc$rms_fab;
	fab.fab$l_fna = addr;
	fab.fab$b_fns = len;
	if (exist)
	{
		nam = cc$rms_nam;
		nam.nam$l_esa = &es[0];
		nam.nam$b_ess = MAX_FILSPC_LEN;
		fab.fab$l_nam = &nam;
	}
	status = sys$parse (&fab);
	if (exist)
	{
		tn.addr = addr;
		tn.len = len;
		dev_typ = io_type(&tn);

		if (dev_typ == rm)
			status = sys$search (&fab);
	}
	return status;
}
