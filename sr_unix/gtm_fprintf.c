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

#include "gtm_stdio.h"
#include <signal.h>
#include <stdarg.h>

GBLREF	boolean_t	blocksig_initialized;
GBLREF  sigset_t	block_sigsent;

/* This is a wrapper for the system "fprintf" and is needed to prevent signal interrupts from occurring in the middle
 * of fprintf since that is not signal-safe (i.e. not re-entrant). The reason why this is considered okay is that if
 * the code is anyways ready to do a fprintf(), it should not be in a performance-sensitive area so doing extra work
 * for signal blocking/unblocking should not be a big issue.
 */
int gtm_fprintf(FILE *stream, const char *format, ...)
{
	va_list		printargs;
	int		retval;
	sigset_t	savemask;

	/* blocksig_initialized could not be set at this point for a variety of cases such as the following.
	 * 	1) If the current image is "dbcertify"
	 * 	2) At startup of GTCM_SERVER or GTCM_GNP_SERVER
	 * 	3) At startup of GT.M (e.g. bad command line "mumps -bad")
	 * Because of this, we dont have an assert(blocksig_initialized) that similar code in dollarh.c has.
	 */
	if (blocksig_initialized)	/* In pro, dont take chances and handle case where it is not initialized */
		sigprocmask(SIG_BLOCK, &block_sigsent, &savemask);
	va_start(printargs, format);
	retval = VFPRINTF(stream, format, printargs);
	va_end(printargs);
	if (blocksig_initialized)
		sigprocmask(SIG_SETMASK, &savemask, NULL);
	return retval;
}
