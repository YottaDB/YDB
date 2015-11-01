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

short iomb_readfl(mval *v, int4 length, int4 t)
{
    GBLREF io_pair io_curr_device;
    int status, iomb_dataread(int);
    io_desc *io_ptr;
    d_mb_struct *mb_ptr;
    short len;

    status = TRUE;
    io_ptr = io_curr_device.in;
    mb_ptr = (d_mb_struct *) io_ptr->dev_sp;
    assert (io_ptr->state == dev_open);
    if((mb_ptr->in_top - mb_ptr->in_pos) < length)
    {
	status = iomb_dataread(t);
    }
    if(( len = mb_ptr->in_top - mb_ptr->in_pos) > length)
    {
	len = (short) length;
    }
    memcpy(v->str.addr,mb_ptr->in_pos,len);
    v->str.len = len;
    mb_ptr->in_pos += len;
    io_ptr->dollar.x += len;
    return status;
}
