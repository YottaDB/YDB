/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#  include "gtm_pthread.h"
#endif
#include "gtm_stdio.h"

#include "ctrlc_handler.h"
#include "std_dev_outbndset.h"
#include "sig_init.h"
#include "gtmio.h"
#include "io.h"
#include "invocation_mode.h"
#include "libyottadb_int.h"
#include "have_crit.h"
#include "deferred_events_queue.h"

GBLREF	struct sigaction	orig_sig_action[];

void ctrlc_handler(int sig, siginfo_t *info, void *context)
{
	int4    ob_char;
	int	save_errno;

	/* Note we don't need to bypass this like in other handlers because this handler is not in use when using
	 * simple[Threaded]API.
	 */
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig_hndlr_ctrlc_handler, sig, IS_EXI_SIGNAL_FALSE, info, context);
	assert((SIGINT == sig) || (SIGHUP == sig));
	assert(!(MUMPS_CALLIN & invocation_mode));
	/* Normal procedure from MUMPS is to set our outofband trigger to handle this signal */
	save_errno = errno;
	ob_char = (SIGINT == sig) ? CTRLC : CTRLD;	/* borrowing CTRLD for SIGHUP */
	std_dev_outbndset(ob_char);
	errno = save_errno;
}
