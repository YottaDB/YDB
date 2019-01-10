/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
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

#ifndef GENERIC_SIGNAL_HANDLER_INCLUDED
#define GENERIC_SIGNAL_HANDLER_INCLUDED

#include "gtm_string.h"

#include "gtmsiginfo.h"

GBLREF	siginfo_t		exi_siginfo;
GBLREF	gtm_sigcontext_t 	exi_context;
GBLREF	int			exi_signal_forwarded;

#define	SET_EXI_SIGINFO_AS_APPROPRIATE(INFO)			\
{								\
	if (NULL != INFO)					\
		exi_siginfo = *(siginfo_t *)INFO;		\
	else							\
		memset(&exi_siginfo, 0, SIZEOF(siginfo_t));	\
}

#define	SET_EXI_CONTEXT_AS_APPROPRIATE(CONTEXT)			\
{								\
	if (NULL != CONTEXT)					\
		exi_context = *(gtm_sigcontext_t *)CONTEXT;	\
	else							\
		memset(&exi_context, 0, SIZEOF(exi_context));	\
}

#define	IS_EXI_SIGNAL_FALSE	FALSE
#define	IS_EXI_SIGNAL_TRUE	TRUE

#ifdef GTM_PTHREAD
/* If we detect a case when the signal came to a thread other than the main GT.M thread, this macro will redirect the signal to the
 * main thread if such is defined. Such scenarios is possible, for instance, if we are running along a JVM, which, upon receiving a
 * signal, dispatches a new thread to invoke signal handlers other than its own. The pthread_kill() enables us to target the signal
 * to a specific thread rather than rethrow it to the whole process.
 */
#define FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(SIG, IS_EXI_SIGNAL, INFO, CONTEXT)					\
{														\
	GBLREF pthread_t	gtm_main_thread_id;								\
	GBLREF boolean_t	gtm_main_thread_id_set;								\
														\
	if (gtm_main_thread_id_set && !pthread_equal(gtm_main_thread_id, pthread_self()))			\
	{	/* Only redirect the signal if the main thread ID has been defined, and we are not that. */	\
		if (IS_EXI_SIGNAL)										\
		{	/* Caller wants INFO and CONTEXT to be recorded as forwarding the signal would lose	\
			 * that information in the forwarded call to the signal handler. Do that first.		\
			 * Then forward the signal.								\
			 */											\
			exi_signal_forwarded = SIG;								\
			SET_EXI_SIGINFO_AS_APPROPRIATE(INFO);							\
			SET_EXI_CONTEXT_AS_APPROPRIATE(CONTEXT);						\
		}												\
		pthread_kill(gtm_main_thread_id, SIG);								\
		return;												\
	}													\
}
#else
#define FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(SIG)
#endif

void generic_signal_handler(int sig, siginfo_t *info, void *context);

#endif /* GENERIC_SIGNAL_HANDLER_INCLUDED */
