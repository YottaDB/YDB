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

void op_wtff(void)
{
	active_device = io_curr_device.out;
	(io_curr_device.out->disp_ptr->wtff)();
	io_curr_device.out->dollar.x = 0;
	io_curr_device.out->dollar.y = 0;
	active_device = 0;
}
