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
#include "dollarx.h"

GBLREF io_pair io_curr_device;

void ionl_write(mstr *v)
{
	io_desc *io_ptr;

	io_ptr = io_curr_device.out;
	io_ptr->dollar.zeof = FALSE;
	dollarx(io_ptr, (uchar_ptr_t)v->addr, (uchar_ptr_t)v->addr + v->len);
	return;
}
