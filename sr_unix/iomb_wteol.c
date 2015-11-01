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

void iomb_wteol(int4 v, io_desc *iod)
{
	mstr	temp;
	int4 	s;
	char 	ptr[1];

	temp.len = 1;
	ptr[0] = NATIVE_LF;
	temp.addr = ptr;
	for (s = 0; s++ < v ; )
	{
		if (iod->dollar.x > 0)
		    iod->dollar.x -= 1;
		iomb_write(&temp);
	}
	iod->dollar.x = 0;
	iod->dollar.y += v;
	if(iod->length)
		iod->dollar.y %= iod->length;
	return;
}
