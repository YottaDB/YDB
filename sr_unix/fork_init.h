/****************************************************************
 *								*
 * Copyright (c) 2011-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _FORK_INIT_H
#define _FORK_INIT_H

#include "have_crit.h"

/* This macro takes care of safely clearing the timer queue and resetting the timer-related globals when we need
 * to fork-off a process. Note that it is necessary to use this macro EVEN WHEN the fork is immediately followed
 * by an exec (of any flavor), because of the ENABLE_INTERRUPTS macro usage right after the "fork" call.
 * If this call is done in the child process (0 == pid), and timer variables are not cleared, it is possible
 * (if the right conditions are met) that "have_crit" gets invoked as part of the ENABLE_INTERRUPTS macro in
 * the child process and ends up with a GTMASSERT -- BYPASSOK -- (GTM-8050). If timers have not been initialized at all,
 * then "clear_timers" does no system calls so the only overhead is a function call invocation which is okay considering
 * we anyways have invoked the "fork()" system call just now.
 */

#define FORK(pid)							\
{									\
	intrpt_state_t	prev_intrpt_state;				\
									\
	DEFER_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state)	\
	pid = fork();							\
	if (0 == pid)							\
		clear_timers();						\
	ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state)	\
}

#endif
