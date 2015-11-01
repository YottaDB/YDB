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
#include "io_params.h"
#include "io.h"
#include "iombdef.h"

GBLREF io_pair io_curr_device;

short iomb_rdone(mint *v,int4 t)
{
	short 		status;
	io_desc 	*io_ptr;
	d_mb_struct 	*mb_ptr;

	status = TRUE;
	io_ptr = io_curr_device.in;
	mb_ptr = (d_mb_struct *) io_ptr->dev_sp;
	assert (io_ptr->state == dev_open);
	if(mb_ptr->in_top == mb_ptr->in_pos)
		status = iomb_dataread(t);
	if (!status)
	{
		*v = -1;
		return status;
	}
	*v = *mb_ptr->in_pos++;
	io_ptr->dollar.x++;
	return TRUE;
}
