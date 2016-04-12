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

#ifndef GTM_SEMAPHORE_H
#define GTM_SEMAPHORE_H

#include <semaphore.h>

/* "sem_init" does not return EINTR so no do/while loop needed like for "sem_wait" */
#define GTM_SEM_INIT(POSIX_SEM, PSHARED, VALUE, RC)		\
{								\
	RC = sem_init(POSIX_SEM, PSHARED, VALUE);		\
	assert((-1 != RC) || (EINTR != errno));			\
}

#define GTM_SEM_WAIT(POSIX_SEM, RC)				\
{								\
	do							\
	{							\
		RC = sem_wait(POSIX_SEM);			\
	} while ((-1 == RC) && (EINTR == errno));		\
}

#define GTM_SEM_TRYWAIT(POSIX_SEM, RC)				\
{								\
	do							\
	{							\
		RC = sem_trywait(POSIX_SEM);			\
	} while ((-1 == RC) && (EINTR == errno));		\
}

/* "sem_post" does not return EINTR so no do/while loop needed like for "sem_wait" */
#define GTM_SEM_POST(POSIX_SEM, RC)				\
{								\
	RC = sem_post(POSIX_SEM);				\
	assert((-1 != RC) || (EINTR != errno));			\
}

#endif
