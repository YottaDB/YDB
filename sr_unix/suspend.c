/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_signal.h"
#include <errno.h>
#include "gtm_unistd.h"

#include "gtmsiginfo.h"
#include "io.h"
#include "iott_setterm.h"
#include "send_msg.h"

GBLREF volatile int 	suspend_status;
GBLREF io_pair		io_std_device;
GBLREF uint4		process_id;
GBLREF uint4 		sig_count;

error_def(ERR_SUSPENDING);
error_def(ERR_TEXT);

void suspend(int sig)
{
	int	status;

	/* Clear suspend_status first to prevent recursion */
	suspend_status = NOW_SUSPEND;
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SUSPENDING, 1, sig);
	/* Dont call flush_pio() if the received signal is SIGTTOU OR if the received signal is SIGTTIN
	 * and the $P.out is terminal. Arrival of SIGTTIN and SIGTTOU indicates that current process is
	 * in the background. Hence it does not make sense to do flush_pio() $P.out is terminal.
	 */
	if (!(sig == SIGTTOU || ((sig == SIGTTIN) && (NULL != io_std_device.out) && (tt == io_std_device.out->type))))
		flush_pio();
	if (NULL != io_std_device.in && tt == io_std_device.in->type)
		iott_resetterm(io_std_device.in);
	sig_count = 0;
	status = kill(process_id, SIGSTOP);
	assert(0 == status);
	return;
}
