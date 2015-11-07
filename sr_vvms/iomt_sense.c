/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include <ssdef.h>
#include <iodef.h>
#include <efndef.h>


uint4   iomt_sense(d_mt_struct *mt, iosb *io_status_blk)
{
	uint4 status;

	status =  sys$qiow(EFN$C_ENF,mt->access_id, IO$_SENSEMODE, io_status_blk,0,0,0,0,0,0,0,0);
	if (status == SS$_NORMAL)
		status = io_status_blk->status;
	return status;
}
