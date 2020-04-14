/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 04cc1b83 (GT.M V6.3-011)
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
<<<<<<< HEAD
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

GBLREF	struct sigaction	orig_sig_action[];
=======
#   include "gtm_pthread.h"
#endif
#include "ctrlc_handler.h"
#include "std_dev_outbndset.h"
#include "outofband.h"		/* for CTRLC and CTRLD */
>>>>>>> 04cc1b83 (GT.M V6.3-011)

void ctrlc_handler(int sig, siginfo_t *info, void *context)
{
	int4    ob_char;
	int	save_errno;

<<<<<<< HEAD
	/* Note we don't need to bypass this like in other handlers because this handler is not in use when using
	 * simple[Threaded]API.
	 */
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig_hndlr_ctrlc_handler, sig, IS_EXI_SIGNAL_FALSE, info, context);
	assert(SIGINT == sig);
	assert(!(MUMPS_CALLIN & invocation_mode));
	/* Normal procedure from MUMPS is to set our outofband trigger to handle this signal */
	save_errno = errno;
	ob_char = 3;
=======
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig);
	save_errno = errno;
	ob_char = (SIGINT == sig) ? CTRLC : CTRLD;	/* borrowing CTRLD for SIGHUP */
>>>>>>> 04cc1b83 (GT.M V6.3-011)
	std_dev_outbndset(ob_char);
	errno = save_errno;
}
