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

#include <rms.h>
#include "gtm_string.h"

#include "io.h"
#include "iormdef.h"
#include "iotimer.h"
#include "stringpool.h"

GBLREF io_pair		io_curr_device;

#define fl_copy(a, b) (a > b ? b : a)

int iorm_readfl(mval *v, int4 length, int4 timeout)
{
	io_desc		*iod;
	d_rm_struct	*d_rm;
	struct RAB	*rab;
	int4		stat;
	uint4		len, cp_len;

	error_def(ERR_IOEOF);

	assert(timeout >= 0);
	v->mvtype = MV_STR;
	iod = io_curr_device.in;

	d_rm = (d_rm_struct *)io_curr_device.in->dev_sp;
	/* if write buffer not empty, and there was no error */
	if ((FAB$M_PUT == d_rm->r.rab$l_ctx) && d_rm->outbuf != d_rm->outbuf_pos
		&& !iod->dollar.za)
		iorm_wteol(1, iod);
	d_rm->r.rab$l_ctx = FAB$M_GET;
	len = d_rm->inbuf_top - d_rm->inbuf_pos;
	if ((d_rm->inbuf != d_rm->inbuf_pos) && (0 != len))
	{
		cp_len = fl_copy(length, len);
		memcpy(v->str.addr, d_rm->inbuf_pos, cp_len);
		if((d_rm->inbuf_pos += cp_len) >= d_rm->inbuf_top)
		{
			d_rm->inbuf_pos = d_rm->inbuf;
			iod->dollar.y++;
			iod->dollar.x = 0;
		} else
			iod->dollar.x += cp_len;
		v->str.len = cp_len;
	} else
	{
		rab = &d_rm->r;
		rab->rab$l_ubf = d_rm->inbuf;
		d_rm->l_usz = iod->width;
		stat = iorm_get(iod, timeout);
		switch (stat)
		{
		case RMS$_NORMAL:
			d_rm->inbuf_top = d_rm->inbuf + d_rm->l_rsz;
			if (length > d_rm->l_rsz)
				length = d_rm->l_rsz;
			v->str.len = length;
			memcpy(v->str.addr, d_rm->inbuf, length);
			if ((d_rm->inbuf_pos += length) >= d_rm->inbuf_top)
			{
				d_rm->inbuf_pos = d_rm->inbuf;
				iod->dollar.y++;
				iod->dollar.x = 0;
			} else
				iod->dollar.x += length;
			iod->dollar.za = 0;
			break;
		case RMS$_TMO:
			v->str.len = 0;
			iod->dollar.za = 9;
			return FALSE;
		case RMS$_EOF:
			v->str.len = 0;
			if (iod->dollar.zeof)
			{
				iod->dollar.za = 9;
				rts_error(VARLSTCNT(1) ERR_IOEOF);
			}
			iod->dollar.x = 0;
			iod->dollar.y++;
			iod->dollar.za = 0;
			iod->dollar.zeof = TRUE;
			if (iod->error_handler.len > 0)
				rts_error(VARLSTCNT(1) ERR_IOEOF);
			break;
		default:
			v->str.len = 0;
			iod->dollar.za = 9;
			rts_error(VARLSTCNT(2) stat, rab->rab$l_stv);
		}
	}
	return TRUE;
}
