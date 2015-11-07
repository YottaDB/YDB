/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <iodef.h>
#include "io.h"
#include "iombdef.h"
#include "stringpool.h"

GBLREF io_pair		io_curr_device;
GBLREF spdesc 		stringpool;

int iomb_read(mval *v, int4 t)
{
	short		not_timed_out;
	io_desc		*io_ptr;
	d_mb_struct	*mb_ptr;

	io_ptr = io_curr_device.in;
	assert (io_ptr->state == dev_open);
	mb_ptr = (d_mb_struct *) io_ptr->dev_sp;

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	ENSURE_STP_FREE_SPACE(mb_ptr->maxmsg);
	v->str.addr = stringpool.free;

	if (mb_ptr->in_top == mb_ptr->in_pos)
		not_timed_out = iomb_dataread(t);
	else
		not_timed_out = TRUE;

	memcpy(v->str.addr,mb_ptr->in_pos,(v->str.len = mb_ptr->in_top - mb_ptr->in_pos));
	mb_ptr->in_pos = mb_ptr->in_top = mb_ptr->inbuf;

	if (io_ptr->wrap && ((io_ptr->dollar.x += v->str.len) > io_ptr->width))
	{
		io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
		if(io_ptr->length)
			io_ptr->dollar.y %= io_ptr->length;
		io_ptr->dollar.x %= io_ptr->width;
	}
	io_ptr->dollar.x = 0;
	io_ptr->dollar.y++;
	return not_timed_out;
}
