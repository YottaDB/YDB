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

#include <mdef.h>

#include <signal.h>

#include "gtmsiginfo.h"
#include "io.h"
#include "send_msg.h"
#include "setterm.h"
#include "continue_handler.h"

GBLREF volatile int	suspend_status;
GBLREF io_pair		io_std_device;

void continue_handler(int sig, siginfo_t *info, void *context)
{
	void		extract_signal_info();
	gtmsiginfo_t	sig_info;

	error_def(ERR_REQ2RESUME);

	assert(SIGCONT == sig);
	extract_signal_info(sig, info, context, &sig_info);
	if (DEFER_SUSPEND != suspend_status)
	{
		/* Don't bother checking if user info is available. If info isn't available, 0 will be printed for pid and uid */
		send_msg(VARLSTCNT(4) ERR_REQ2RESUME, 2, sig_info.send_pid, sig_info.send_uid);
		if (NOW_SUSPEND == suspend_status)
		{
			suspend_status = NO_SUSPEND;
			if (io_std_device.in->type == tt)
				setterm(io_std_device.in);
		}
	}
	return;
}
