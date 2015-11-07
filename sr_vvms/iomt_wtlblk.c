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
#include <iodef.h>
#include <efndef.h>


uint4 iomt_wtlblk(uint4 channel, uint4 mask, iosb *stat_blk, void *buff, int size)
{
	return sys$qiow(EFN$C_ENF, channel, mask | IO$_WRITELBLK,stat_blk, 0,0, (char*)buff,size, 0,0,0,0 );
}
