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

/* iomt_wtlblk.c - UNIX (low-level) Write block to tape */
#include "mdef.h"

#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "gtmio.h"

uint4 iomt_wtlblk (uint4 channel, uint4 mask, iosb *stat_blk, void *buff, int size)
{
	int4   status;

#ifdef DP
	FPRINTF(stderr, "-> Writing %d bytes.\n", size);
#endif
	DOWRITERC(channel, buff, size, status);
	if (0 != status)
	{
#ifdef DP
		PERROR("iomt_wtlblk");
#endif
		if (ENXIO == status)
		{
			stat_blk->status = SS_ENDOFTAPE;
			status = SS_ENDOFTAPE;
		} else
		{
			stat_blk->status = MT_TAPERROR;
			status = MT_TAPERROR;
		}
	} else
	{
		stat_blk->status = SS_NORMAL;
		status = SS_NORMAL;
	}
	return (uint4)status;
}
