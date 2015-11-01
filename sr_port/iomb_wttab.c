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
#include "iosp.h"

GBLREF io_pair io_curr_device;

void iomb_wttab(short x)
{
	mstr	temp;
	int i;

	temp.addr = (char *) SPACES_BLOCK;
	if ((i = x / TAB_BUF_SZ) != 0)
	{
		temp.len = TAB_BUF_SZ;
		while (i-- != 0)
		{	iomb_write(&temp);
		}
	}
	if ((i = x % TAB_BUF_SZ) != 0)
	{
		temp.len = i;
		iomb_write(&temp);
	}
	return;
}
