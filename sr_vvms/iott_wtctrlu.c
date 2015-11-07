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

#define BS 8

GBLREF io_pair 		io_curr_device;

#define ERASE_BUF_SZ 64
static readonly unsigned char eraser[ERASE_BUF_SZ * 3] =
{
	BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS,
	BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS,
	BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS,
	BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS,
	BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS,
	BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS,
	BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS,
	BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS, BS,SP,BS
};

void iott_wtctrlu( short v, io_desc *iod)
{
mstr	temp;
int	x, y;
short 	s;

	temp.addr = &eraser[0];
	x = iod->dollar.x;
	y = iod->dollar.y;
	if (iod->wrap)
		v %= iod->width;

	if ((s = v / ERASE_BUF_SZ) != 0)
	{
		temp.len = ERASE_BUF_SZ * 3;
		while (s-- != 0)
		{
			iott_write(&temp);
			iod->dollar.x = x;
			iod->dollar.y = y;
		}
	}
	if ((s = v % ERASE_BUF_SZ) != 0)
	{
		temp.len = s * 3;
		iott_write(&temp);
		iod->dollar.x = x;
		iod->dollar.y = y;
	}
	iott_flush(io_curr_device.out);
	return;
}
