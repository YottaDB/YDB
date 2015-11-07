/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"

GBLREF io_pair io_curr_device;

void iomb_wtff(void)
{
	mstr	temp;
	char	p[1];

	p[0] = '\14';
	temp.len = 1;
	temp.addr = p;
	iomb_write(&temp);
	io_curr_device.out->dollar.x = 0;
	io_curr_device.out->dollar.y = 0;
	return;
}
