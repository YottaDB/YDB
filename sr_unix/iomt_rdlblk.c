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

/* iomt_rdlblk.c UNIX (low-level) read a block of data from mag tape. */

#include "mdef.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "io.h"
#include "iottdef.h"
#include "iosp.h"
#include "iomtdef.h"
#include "gtmio.h"

uint4 iomt_rdlblk (d_mt_struct *mt_ptr,
		   uint4 mask,
		   iosb *stat_blk,
		   void *buff,
		   int size)
{
	int4            status;

	DOREADRL(mt_ptr->access_id, buff, size, status);
	stat_blk->char_ct = status;

#ifdef DP
	FPRINTF(stderr, "-> Reading - size: %d status: %d\n", size, status);
#endif
	if (status < 0)
	{
#ifdef DP
		PERROR("iomt_rdlblk");
#endif
		status = errno;
		stat_blk->status = MT_TAPERROR;
	} else if (status == 0)
	{
		status = SS_NORMAL;
		stat_blk->status = SS_ENDOFFILE;
		mt_ptr->filepos += 1;
	}
	/* DCK or end of tape */
	else if (status > 0)
	{
		stat_blk->status = SS_NORMAL;
		mt_ptr->recpos += 1;
		status = SS_NORMAL;
	}
	return status;
}

