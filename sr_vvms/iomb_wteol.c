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

void iomb_wteol(int4 v, io_desc *iod)
{
	mstr	temp;
	int4	s;
	char 	ptr[2];

	temp.len = 2;
	ptr[0] = '\15';
	ptr[1] = '\12';
	temp.addr = ptr;
	for (s = 0; s++ < v ; )
	{
		iod->dollar.x -= 2;
		iomb_write(&temp);
	}
	iod->dollar.x = 0;
	iod->dollar.y += v;
	if(iod->length)
		iod->dollar.y %= iod->length;
	return;
}
