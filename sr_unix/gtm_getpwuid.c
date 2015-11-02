/****************************************************************
 *								*
 *	Copyright 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_pwd.h"

#undef	getpwuid	/* since we are going to use the system level "getpwuid" function, undef the alias to "gtm_getpwuid" */

#include <signal.h>

GBLREF	boolean_t	blocksig_initialized;
GBLREF  sigset_t	block_sigsent;

/* This is a wrapper for the system "getpwuid" and is needed to prevent signal interrupts from occurring in the middle
 * of getpwuid since that is not signal-safe (i.e. could hold system library related locks that might prevent a signal
 * handler from running other system library calls which use the same lock).
 */
struct passwd	*gtm_getpwuid(uid_t uid)
{
	struct passwd	*retval;
	sigset_t	savemask;

	assert(blocksig_initialized);	/* the set of blocking signals should be initialized at process startup */
	if (blocksig_initialized)	/* In pro, dont take chances and handle case where it is not initialized */
		sigprocmask(SIG_BLOCK, &block_sigsent, &savemask);
	retval = getpwuid(uid);
	if (blocksig_initialized)
		sigprocmask(SIG_SETMASK, &savemask, NULL);
	return retval;
}
