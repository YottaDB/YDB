/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *
 *	This function returns the maximum number
 *	of file descriptors (channels) per process.
 *
 */
#include <sys/resource.h>
#include "getmaxfds.h"

int getmaxfds(void)
{
	struct rlimit rlp;

	if (getrlimit(RLIMIT_NOFILE, &rlp) < 0)
		return -1;
	return (int)rlp.rlim_cur;
}
