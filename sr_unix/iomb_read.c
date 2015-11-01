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

#include "gtm_string.h"

#include "io.h"
#include "iombdef.h"
#include "stringpool.h"

GBLREF spdesc 		stringpool;

short iomb_read(mval *v,int4 t)
{
GBLREF io_pair io_curr_device;
io_desc *io_ptr;
d_mb_struct *mb_ptr;
short status;


	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	io_ptr = io_curr_device.in;
	mb_ptr = (d_mb_struct *) io_ptr->dev_sp;
	if (stringpool.free + mb_ptr->maxmsg > stringpool.top)
		stp_gcol (mb_ptr->maxmsg);
	v->str.addr = (char *)stringpool.free;
	status = TRUE;
	assert (io_ptr->state == dev_open);
	if (mb_ptr->in_top == mb_ptr->in_pos)
	{	    status = iomb_dataread(t);
	}
	memcpy(v->str.addr,mb_ptr->in_pos,(v->str.len = mb_ptr->in_top - mb_ptr->in_pos));
	mb_ptr->in_pos = mb_ptr->in_top = mb_ptr->inbuf;
	io_ptr->dollar.x = 0;
	io_ptr->dollar.y++;
	return status;
}
