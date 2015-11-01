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

#include <signal.h>
#include <errno.h>

#include "gtmsiginfo.h"
#include "io.h"
#include "send_msg.h"
#include "setterm.h"

GBLREF volatile int 	suspend_status;
GBLREF io_pair		io_std_device;
GBLREF uint4		process_id;

void suspend(void)
{
	int	status;

	error_def(ERR_SUSPENDING);

	send_msg(VARLSTCNT(1) ERR_SUSPENDING);
	suspend_status = NOW_SUSPEND;
	flush_pio();
	if (NULL != io_std_device.in && tt == io_std_device.in->type)
		resetterm(io_std_device.in);
	status = kill(process_id, SIGSTOP);
	assert(0 == status);
	return;
}
