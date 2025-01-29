/****************************************************************
 *								*
 * Copyright (c) 2015-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_pthread.h - interlude to <pthread.h> system header file.  */
#ifndef GTM_PTHREAD_H
#define GTM_PTHREAD_H

#include <pthread.h>	/* BYPASSOK(gtm_pthread.h) */

#if ((_POSIX_C_SOURCE >= 200809L) && !defined(_AIX))
#	define PTHREAD_MUTEX_ROBUST_SUPPORTED		(1)
#	define PTHREAD_MUTEX_CONSISTENT_SUPPORTED	(1)
#else
#	define PTHREAD_MUTEX_ROBUST_SUPPORTED		(0)
#	define PTHREAD_MUTEX_CONSISTENT_SUPPORTED	(0)
#endif

#endif
