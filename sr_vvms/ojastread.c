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
#include <accdef.h>
#include <stsdef.h>
#include <iodef.h>
#include "efn.h"
#include "job.h"

GBLREF	short		ojpchan;
GBLREF	int4		ojcpid;

void ojastread (int expected)
{
	int4		status;
	mstr		stsdsc;
	struct
	{
		pmsg_type	msg;
		char		filler[ACC$K_TERMLEN - sizeof (pmsg_type)];
	}		stsmsg;
	mbx_iosb	iosb;
	error_def	(ERR_JOBFAIL);
	error_def	(ERR_UIDMSG);
	error_def	(ERR_UIDSND);

	stsdsc.addr = &stsmsg.msg;
	stsdsc.len = sizeof stsmsg;
	ojmbxio (IO$_READVBLK, ojpchan, &stsdsc, &iosb, TRUE);
	if (iosb.pid != ojcpid)
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) ERR_UIDSND);
	}
	if ((iosb.byte_count != sizeof stsmsg.msg) &&
	    (iosb.byte_count != ACC$K_TERMLEN))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) ERR_UIDMSG);
	}
	if (stsmsg.msg.finalsts != expected)
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(3) ERR_JOBFAIL, 0, (stsmsg.msg.finalsts & ~STS$M_INHIB_MSG));
	}
	status = sys$setef (efn_op_job);
	if (!(status & 1))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(3) ERR_JOBFAIL, 0, status);
	}
	return;
}
