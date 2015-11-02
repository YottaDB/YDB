/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>

#include "mlkdef.h"
#include "error.h"
#include "cmidef.h"
#include "lke.h"
#include "have_crit.h"

GBLREF VSIG_ATOMIC_T	util_interrupt;
GBLREF volatile int4 	fast_lock_count;

/* Only allow the process to be interrupted if we are not in crit and not in the process
   of obtaining it. Otherwise, we cannot service this interruption at this time and must
   return (but will set the interruption flag. */
CONDITION_HANDLER(lke_ctrlc_handler)
{
	int	dummy1, dummy2;

	START_CH;				/* Drive top level condition handler if we can */
	util_interrupt = 1;
	if (0 == fast_lock_count && 0 == have_crit(CRIT_HAVE_ANY_REG))
	{
		UNWIND(dummy1, dummy2);		/* This will do a longjmp and not 'return' here */
	}
	CONTINUE;				/* Unconditional return */
}
