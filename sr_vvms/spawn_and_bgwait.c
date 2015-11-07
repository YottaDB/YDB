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

#include <descrip.h>
#include <ssdef.h>
#include <clidef.h>

#include "efn.h"

uint4 spawn_and_bgwait(struct dsc$descriptor_s *d_cmd, struct dsc$descriptor_s *d_infile, struct dsc$descriptor_s *d_outfile,
		uint4 *flags, struct dsc$descriptor_s *d_prcname, uint4 *pid, int4 *completion_status)
{
	uint4 status;
	uint4 flags_with_nowait = CLI$M_NOWAIT;
	unsigned char ef = efn_sys_wait;

	if (flags)
		flags_with_nowait |= *flags;
	status = lib$spawn(d_cmd, d_infile, d_outfile, &flags_with_nowait, d_prcname, pid, completion_status, &ef);
	if (SS$_NORMAL == status)
		status = sys$waitfr(ef);
	return status;
}
