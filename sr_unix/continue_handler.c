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
	gtmsiginfo_t	sig_info;

	error_def(ERR_REQ2RESUME);

	assert(SIGCONT == sig);
	extract_signal_info(sig, info, context, &sig_info);
	switch(suspend_status)
	{
		case NOW_SUSPEND:
			/* Don't bother checking if user info is available. If info isn't available,
			   the value zero will be printed for pid and uid. Note that the following
			   message was previously put out even when the process was not suspended but
			   with the changes in spin locks that send continue messages "just in case",
			   I thought it better to restrict this message to when the process was actually
			   suspended and being continued. SE 7/01
			*/
			send_msg(VARLSTCNT(4) ERR_REQ2RESUME, 2, sig_info.send_pid, sig_info.send_uid);
			if (NULL != io_std_device.in && tt == io_std_device.in->type)
				setterm(io_std_device.in);
			/* Fall through */
		case DEFER_SUSPEND:
			/* If suspend was deferred, this continue/resume overrides/cancels it */
			suspend_status = NO_SUSPEND;
			break;
	}
}
