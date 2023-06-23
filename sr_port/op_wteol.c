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
#include "op.h"

GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;

void op_wteol(int4 n)
{
	io_desc		*iod;
	iod = io_curr_device.out;

	active_device = io_curr_device.out;
	(iod->disp_ptr->wteol)(n,iod);
	active_device = 0;
}
