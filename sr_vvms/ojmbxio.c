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

void ojmbxio (int4 func, short chan, mstr *msg, short *iosb, bool now)
{
	int4		status;

	assert (func == IO$_READVBLK ||
		func == IO$_WRITEVBLK ||
		func == IO$_WRITEOF);
	if (now) func |= IO$M_NOW;
	status = sys$qio (efn_immed_wait, chan, func, iosb, 0, 0,
				msg->addr, msg->len, 0, 0, 0, 0);
	if (!(status & 1))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}
	sys$synch (efn_immed_wait, iosb);
	if (!(*iosb & 1))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) *iosb);
	}
	return;
}
