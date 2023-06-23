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

GBLREF io_pair io_curr_device;

void ionl_wtff(void)
{
	io_curr_device.out->esc_state = START;
	io_curr_device.out->dollar.zeof = FALSE;
	io_curr_device.out->dollar.x = 0;
	io_curr_device.out->dollar.y = 0;
	return;
}
