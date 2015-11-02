/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>

#include "mupip_stop.h"
#include "mu_signal_process.h"

void mupip_stop(void)
{
	VMS_ONLY(error_def(ERR_FORCEDHALT);)
	mu_signal_process("STOP", UNIX_ONLY(SIGTERM) VMS_ONLY(ERR_FORCEDHALT));
}
