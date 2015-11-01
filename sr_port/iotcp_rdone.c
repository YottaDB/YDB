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

short	iotcp_rdone (mint *v, int4 timeout) /* timeout in seconds */
{
	mval	tmp;
	bool	ret;

	ret = iotcp_readfl(&tmp, 1, timeout);
	if (ret)
	{
		*v = (int4)*(unsigned char *)(tmp.str.addr);
	} else
		*v = -1;
	return ((short)ret);
}
