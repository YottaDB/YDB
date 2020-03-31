/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information 		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_SIGNAL_H
#define GTM_SIGNAL_H

#include <signal.h>	/* BYPASSOK(gtm_signal.h) */

#include "gtm_multi_thread.h"	/* for INSIDE_THREADED_CODE macro */

/* Describe the flags needed when it replaces/sets a signal handler. SA_ONSTACK is required by Go so that signal handlers
 * use an alternate stack if available. Go provides a (too small) stack that we replace at the top of sig_init().
 * The SA_SIGINFO flag gives us full information about the signal interrupt when we enter a signal handler. This is
 * needed both by us and to effectively forward a signal we receive to the caller's signal handler that we replaced.
 */
#define YDB_SIGACTION_FLAGS (SA_SIGINFO | SA_ONSTACK)

#define SIGPROCMASK(FUNC, NEWSET, OLDSET, RC)							\
MBSTART {											\
	GBLREF	boolean_t	multi_thread_in_use;						\
	GBLREF	boolean_t	simpleThreadAPI_active;						\
												\
	char	*rname;										\
												\
	/* Use the right system call based on threads are in use or not */			\
	if (!INSIDE_THREADED_CODE(rname) && !simpleThreadAPI_active)				\
		RC = sigprocmask(FUNC, NEWSET, OLDSET);	/* BYPASSOK(sigprocmask) */		\
	else											\
		RC = pthread_sigmask(FUNC, NEWSET, OLDSET);					\
	assert(0 == RC);									\
} MBEND

#endif
