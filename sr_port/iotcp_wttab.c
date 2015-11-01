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
#include "gtm_stdio.h"
#include "io.h"
#include "iosp.h"

GBLREF io_pair io_curr_device;

void iotcp_wttab(short len)
{
	mstr	temp;
	int i, j;

#ifdef DEBUG_TCP
    PRINTF("%s >>>\n",__FILE__);
#endif
	temp.addr = (char *)SPACES_BLOCK;
	if ((i = len / TAB_BUF_SZ) != 0)
	{
		temp.len = TAB_BUF_SZ;
		j = i;
		while (j-- != 0)
		{	iotcp_write(&temp);
		}
	}
	if ((i = len - i * TAB_BUF_SZ) != 0)
	{
		temp.len = i;
		iotcp_write(&temp);
	}
#ifdef DEBUG_TCP
    PRINTF("%s <<<\n",__FILE__);
#endif
	return;
}
