/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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

void m_usleep(int useconds)
{
	SLEEP_USEC(useconds, FALSE);
}
