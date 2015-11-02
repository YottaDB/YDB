/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <sched.h>
#include "rel_quant.h"

/* relinquish the processor to the next process in the scheduling queue */
void rel_quant(void)
{
	sched_yield();	/* we do not need to link with libpthreads, so the usage of pthread_yield() is avoided where possible */
}
