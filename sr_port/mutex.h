/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUTEX_H
#define MUTEX_H

#include "mutexsp.h"

#if defined(UNIX)
#define MUTEX_HARD_SPIN_COUNT	128
#elif defined(VMS)
#define MUTEX_HARD_SPIN_COUNT	1024 /* VMS mutex does not have a sleep spin loop, so compensate with larger hard spin count */
#else
#error Unsupported Platform
#endif

#define MUTEXLCKALERT_INTERVAL		32	/* seconds [UNIX only] */
#define MUTEX_SLEEP_SPIN_COUNT		128
#define MUTEX_SPIN_SLEEP_MASK		0	/* default to cause rel_quant */
#define MUTEX_WRITE_HARD_SPIN_COUNT	MUTEX_HARD_SPIN_COUNT
#define MUTEX_WRITE_SLEEP_SPIN_COUNT	MUTEX_SLEEP_SPIN_COUNT
#define MUTEX_WRITE_SPIN_SLEEP_MASK	MUTEX_SPIN_SLEEP_MASK

#define MUTEX_MAX_OPTIMISTIC_ATTEMPTS 		1024

#define MUTEX_MAX_WAIT_FOR_PROGRESS_CNTR	3

#endif /* MUTEX_H */
