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

#include "mdef.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include <iodef.h>
#include <ssdef.h>
#include <efndef.h>


uint4 iomt_rdlblk(
	d_mt_struct    *mt_ptr,
	uint4		mask,
	iosb	       *stat_blk,
	void	       *buff,
	int	       size)

{
    uint4 status;

    mask |= IO$_READLBLK;
    status = sys$qiow(EFN$C_ENF,mt_ptr->access_id,mask,stat_blk,0,0,
				    buff,size,0,0,0,0);
   return status;
}

