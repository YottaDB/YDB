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
#include <iodef.h>
#include "efn.h"
#include "job.h"

GBLREF	short	ojpchan;

void ojsetattn (int msg)
{
	int4		status;
	short		iosb[4];

	status = sys$clref (efn_op_job);
	if (!(status & 1))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}
	status = sys$qio (efn_immed_wait, ojpchan, IO$_SETMODE | IO$M_WRTATTN,
			iosb, 0, 0, ojastread, msg, 0, 0, 0, 0);
	if (!(status & 1))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}
	sys$synch (efn_immed_wait, iosb);
	if (!(iosb[0] & 1))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) iosb[0]);
	}
	return;
}
