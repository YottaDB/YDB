/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

# ifdef __MVS__
# define _OPEN_THREADS
# include <stdio.h>
# include <pthread.h>
# else
# include <sched.h>
# endif
#include "rel_quant.h"

/* relinquish the processor to the next process in the scheduling queue */
void rel_quant(void)
{
# ifdef __MVS__
	pthread_yield(NULL);
# else
	sched_yield();	/* we do not need to link with libpthreads, so the usage of pthread_yield() is avoided where possible */
# endif
}
