/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 52a92dfd (GT.M V7.0-001)
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_time.h"
#include <sys/time.h>
#include <errno.h>
#include "sleep.h"

#ifdef __MVS__
/* OS390 versions must use usleep */
#  include "gtm_unistd.h"
#endif

/* Formerly a separate sleep high performance mechanism that is now a wrapper around
 * SLEEP_USEC in sleep.h
 *
 * Input:
 * 	useconds - sleep duration in microseconds. Callers frequently sleep in
 *	 	   terms of milliseconds and so multiply the input parameter by
 *	 	   1000 to turn milliseconds into microseconds
 */
void m_usleep(int useconds)
{
	SLEEP_USEC(useconds, TRUE);
}
