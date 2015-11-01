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
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"

GBLREF io_pair  io_curr_device;

int iomt_rdone(mint *v, int4 t)
{
	error_def(ERR_MTRDTHENWRT);
	unsigned char   x;
	io_desc        *dv;
	d_mt_struct    *mt_ptr;

	dv = io_curr_device.in;
	mt_ptr = (d_mt_struct *)dv->dev_sp;

#ifdef UNIX
	if (mt_ptr->mode != MT_M_READ)
	{
		uint4 status;

		status = iomt_reopen(dv, MT_M_READ, FALSE);
		if (status != SS_NORMAL)
			rts_error(VARLSTCNT (1) status);
	}
#endif
	switch (mt_ptr->last_op)
	{
		case mt_rewind:
			if (mt_ptr->labeled == MTLAB_DOS11)
				iomt_rddoslab(dv);
			else if (mt_ptr->labeled == MTLAB_ANSI)
				iomt_rdansistart (dv);
			/* CAUTION: FALL THROUGH  */
		case mt_null:
			if (iomt_readblk(dv))
			{
				if (!mt_ptr->stream)
					iomt_getrec(dv);
			}
			break;
		case mt_read:
			break;
		default:
			rts_error(VARLSTCNT (1) ERR_MTRDTHENWRT);
	}
	if (io_curr_device.in->dollar.zeof)
	{
		*v = -1;
		return TRUE;
	}
	if (mt_ptr->stream)
	{
		if (mt_ptr->buffptr >= mt_ptr->bufftop)
		{
			iomt_readblk (dv);
			if (io_curr_device.in->dollar.zeof)
			{
				*v = -1;
				return TRUE;
			}
		}
		iomt_rdstream(1, &x, dv);
		if (io_curr_device.in->dollar.zeof)
			*v = -1;
		else if (mt_ptr->rec.len == 0)
		{
			*v = 11;
			io_curr_device.in->dollar.x = 0;
			io_curr_device.in->dollar.y++;
		} else
			*v = (unsigned int)x;
	} else
	{
		if (mt_ptr->rec.len == 0)
		{
			*v = 13;
			io_curr_device.in->dollar.x = 0;
			io_curr_device.in->dollar.y++;
			mt_ptr->last_op = mt_null;
			return TRUE;
		}
		*v = (unsigned int)(*mt_ptr->rec.addr++);
		mt_ptr->rec.len--;
	}
	mt_ptr->last_op = mt_read;
	return TRUE;
}
