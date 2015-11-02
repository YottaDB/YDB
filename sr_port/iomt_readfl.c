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
#include "gtm_stdio.h"

#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"

GBLREF io_pair  io_curr_device;

int iomt_readfl(mval *v, int4 l, int4 t)
{
	error_def (ERR_MTRDTHENWRT);
	io_desc        *dv;
	d_mt_struct    *mt_ptr;

	dv = io_curr_device.in;
	mt_ptr = (d_mt_struct *) dv->dev_sp;

#ifdef UNIX
	if (mt_ptr->mode != MT_M_READ)
	{
		unsigned short  status;

		status = iomt_reopen(dv, MT_M_READ, FALSE);
		if (status != SS_NORMAL)
			return (status);
	}
#endif
	switch (mt_ptr->last_op)
	{
		case mt_rewind:
			if (mt_ptr->labeled == MTLAB_DOS11)
				iomt_rddoslab(dv);
			else if (mt_ptr->labeled == MTLAB_ANSI)
				iomt_rdansistart(dv);
			/* CAUTION: FALL THROUGH  */
		case mt_null:
			if (iomt_readblk(dv))
			{
				if (mt_ptr->stream)
					iomt_rdstream(l, v->str.addr, dv);
				else
					iomt_getrec(dv);
			}
			break;
		case mt_read:
			if (mt_ptr->stream)
				iomt_rdstream(l, v->str.addr, dv);
			break;
		default:
			rts_error(VARLSTCNT (1) ERR_MTRDTHENWRT);
	}
	if (io_curr_device.in->dollar.zeof)
	{
		v->str.len = 0;
		return TRUE;
	}
	if (mt_ptr->stream)
	{
		v->str.len = mt_ptr->rec.len;
		mt_ptr->last_op = (mt_ptr->buffptr >= mt_ptr->bufftop)
			? mt_null : mt_read;
	} else
	{
		if (mt_ptr->rec.len <= l)
		{
			memcpy(v->str.addr, mt_ptr->rec.addr, mt_ptr->rec.len);
			v->str.len = mt_ptr->rec.len;
			io_curr_device.in->dollar.x = 0;
			io_curr_device.in->dollar.y++;
			mt_ptr->last_op = (mt_ptr->buffptr >= mt_ptr->bufftop)
				? mt_null : mt_read;
			mt_ptr->rec.len = 0;
		} else
		{
			memcpy(v->str.addr, mt_ptr->rec.addr, l);
			v->str.len = l;
			mt_ptr->rec.addr += l;
			mt_ptr->rec.len -= l;
			io_curr_device.in->dollar.x += l;
			mt_ptr->last_op = mt_read;
		}
		if (mt_ptr->rec.len <= 0 && mt_ptr->last_op == mt_read)
			iomt_getrec(dv);
	}
	return TRUE;
}
