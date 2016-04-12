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

/* iomt_closesp.c - UNIX (low-level) close mag tape device */
#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "gtmio.h"

void iomt_closesp (int4 channel)
{
	int	rc;

#ifdef DP
	FPRINTF(stderr, "-----> iomt_closesp(%d)\n",channel);
#endif
	CLOSEFILE_RESET(channel, rc);	/* resets "channel" to FD_INVALID */
	return;
}
