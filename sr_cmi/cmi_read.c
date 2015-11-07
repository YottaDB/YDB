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

#include <iodef.h>
#include "cmihdr.h"
#include "cmidef.h"

uint4 cmi_read(lnk)
struct CLB *lnk;
{
	lnk->cbl = lnk->mbl;
	return cmj_iostart(lnk,IO$_READVBLK, CM_CLB_READ);
}
