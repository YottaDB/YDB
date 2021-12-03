/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_signal.h"
#ifdef GTM_PTHREAD
#   include "gtm_pthread.h"
#endif
#include "ctrlc_handler.h"
#include "std_dev_outbndset.h"
#include "have_crit.h"
#include "deferred_events_queue.h"

void ctrlc_handler(int sig)
{
	int4     ob_char;
	int	 save_errno;

	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig);
	save_errno = errno;
	ob_char = (SIGINT == sig) ? CTRLC : CTRLD;	/* borrowing CTRLD for SIGHUP */
	std_dev_outbndset(ob_char);
	errno = save_errno;
}
