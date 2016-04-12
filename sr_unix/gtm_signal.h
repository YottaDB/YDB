/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information 		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#define SIGPROCMASK(FUNC, NEWSET, OLDSET, RC)							\
{												\
	GBLREF	boolean_t	multi_thread_in_use;						\
												\
	char	*rname;										\
												\
	/* Use the right system call based on threads are in use or not */			\
	if (!INSIDE_THREADED_CODE(rname))							\
		RC = sigprocmask(FUNC, NEWSET, OLDSET);	/* BYPASSOK(sigprocmask) */		\
	else											\
		RC = pthread_sigmask(FUNC, NEWSET, OLDSET);					\
	assert(0 == RC);									\
}

#endif
