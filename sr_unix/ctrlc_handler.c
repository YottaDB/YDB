/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019 YottaDB LLC. and/or its subsidiaries.	*
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
#include <signal.h>
#ifdef GTM_PTHREAD
#  include <pthread.h>
#endif

#include "ctrlc_handler.h"
#include "std_dev_outbndset.h"
#include "generic_signal_handler.h"

void ctrlc_handler(int sig)
{
	int4     ob_char;
	int	 save_errno;

	if (SIGINT == sig)
	{
		FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig, IS_EXI_SIGNAL_FALSE, NULL, NULL);
		save_errno = errno;
		ob_char = 3;
		std_dev_outbndset(ob_char);
		errno = save_errno;
	}
}
