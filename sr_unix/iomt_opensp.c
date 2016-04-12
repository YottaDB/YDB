/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iomt_opensp.c - UNIX (low-level) open mag tape device */
#include "mdef.h"

#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_stdio.h"

#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"

uint4 iomt_opensp (io_log_name *dev_name, d_mt_struct *mtdef)
{
	int4            status, channel;

#ifdef DP
	FPRINTF(stderr, ">> iomt_opensp\n");
#endif
	channel = OPEN3(dev_name->dollar_io, O_RDONLY | O_NDELAY, 0666);
	if (channel < 0)
	{
		status = MT_TAPERROR;
		mtdef->access_id = errno;
	} else
	{
		status = SS_NORMAL;
		mtdef->access_id = channel;
	}
	mtdef->filepos = 0;
	mtdef->recpos = 0;
	mtdef->mode = MT_M_READ;

#ifdef DP
	FPRINTF(stderr, "<< iomt_opensp(%d)\n",channel);
#endif

	return status;
}
