/****************************************************************
 *								*
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_string.h"
#include <errno.h>

#include "cmidef.h"
#include "generic_signal_handler.h"
#include "sig_init.h"

GBLDEF struct NTD *ntd_root;

/* CLEAN-UP: 1. replace CMI_CMICHECK with new message
	2. find correct size and location of MBX_SIZE
*/

error_def(CMI_CMICHECK);

cmi_status_t cmj_netinit(void)
{
	struct NTD *tsk;
	struct sigaction sa;
	int rval;

	ASSERT_IS_LIBCMISOCKETTCP;
	if (ntd_root)
		return CMI_CMICHECK;
	ntd_root = (struct NTD *)malloc(SIZEOF(*ntd_root));
	tsk = ntd_root;
	memset(tsk, 0, SIZEOF(*tsk));
	tsk->listen_fd = FD_INVALID;
	FD_ZERO(&tsk->rs);
	FD_ZERO(&tsk->ws);
	FD_ZERO(&tsk->es);
	/* To support CMI_MUTEX_ macros */
	sigemptyset(&tsk->mutex_set);
	sigaddset(&tsk->mutex_set, SIGIO);
	sigaddset(&tsk->mutex_set, SIGURG);
	/* Setup signal handlers */
	if (!USING_ALTERNATE_SIGHANDLING)
	{	/* I/O - SIGURG, SIGIO are I/O interrupts - highest priority */
		sa.sa_flags = SA_SIGINFO;
		sa.sa_handler = NULL;
		sa.sa_sigaction = cmj_handler;
		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, SIGURG);
		rval = sigaction(SIGIO, &sa, NULL);
		if (rval < 0)
			return errno;
		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, SIGIO);
		rval = sigaction(SIGURG, &sa, NULL);
		if (rval < 0)
			return errno;
	} else
	{	/* Set the handlers for these signals into the dispatch array for alternate signal handling */
		SET_ALTERNATE_SIGHANDLER(SIGIO, &ydb_altio_sighandler);
		SET_ALTERNATE_SIGHANDLER(SIGURG, &ydb_altio_sighandler);
	}
	return SS_NORMAL;
}
