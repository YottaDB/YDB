/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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

GBLREF io_pair	io_curr_device;

int iomb_readfl(mval *v,int4 length,int4 t)
{
	int		not_timed_out;
	int		len;
	io_desc		*io_ptr;
	d_mb_struct	*mb_ptr;

	io_ptr = io_curr_device.in;
	assert (io_ptr->state == dev_open);
	mb_ptr = (d_mb_struct *) io_ptr->dev_sp;

	if (mb_ptr->in_top == mb_ptr->in_pos)
		not_timed_out = iomb_dataread(t);
	else
		not_timed_out = TRUE;

	if ((len = mb_ptr->in_top - mb_ptr->in_pos) > length)
		len = length;

	memcpy(v->str.addr,mb_ptr->in_pos,len);
	v->str.len = len;
	mb_ptr->in_pos += len;
	if (io_ptr->dollar.zeof)
	{
		io_ptr->dollar.x = 0;
		io_ptr->dollar.y++;
	}else
	{
		if ((io_ptr->dollar.x += len) > io_ptr->width && io_ptr->wrap)
		{
			io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
			if(io_ptr->length)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x %= io_ptr->width;
		}
	}
	return not_timed_out;
}
