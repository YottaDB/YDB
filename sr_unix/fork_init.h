/****************************************************************
 *								*
 *	Copyright 2011, 2013 Fidelity Information Services, Inc.*
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

/* This macro takes care of safely clearing the timer queue and resetting the
 * timer-related globals when we need to fork-off a process. Please note that
 * it is unnecessary to use this macro when the fork is immediately followed
 * by an exec (of any flavor), because the globals would get cleared
 * automatically in that case. To avoid warning messages from ftpput.csh in such
 * situations, append BYPASSOK comment directives to the lines with fork
 * instances.
 */

#define FORK_CLEAN(pid)			\
{					\
	FORK(pid);			\
	if (0 == pid)			\
		clear_timers();		\
}

#define FORK(pid)                       		\
{                                       		\
	DEFER_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM)	\
	pid = fork();                   		\
	ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM)	\
}

#endif
