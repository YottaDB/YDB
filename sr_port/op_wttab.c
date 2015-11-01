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
#include "op.h"

GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;

void op_wttab(mint x)
{
	short n;
	io_desc		*iod;

	active_device = io_curr_device.out;
	iod = io_curr_device.out;
	if ((n = (short)x - iod->dollar.x) > 0)
	{
		(iod->disp_ptr->wttab)(n);
		if (iod->wrap)
		{	iod->dollar.x = x % iod->width;
			iod->dollar.y += (x / iod->width);
			if (iod->length)
				iod->dollar.y %= iod->length;
		}
		else
		{	iod->dollar.x = x;
		}
	}
	active_device = 0;
}
