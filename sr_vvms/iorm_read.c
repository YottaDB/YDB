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

#include <rms.h>
#include "gtm_string.h"

#include "io.h"
#include "iormdef.h"
#include "iotimer.h"
#include "stringpool.h"

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;

int iorm_read(mval *v, int4 timeout)
{
	io_desc		*iod;
	d_rm_struct     *rm_ptr;
	struct RAB	*rab;
	int4		stat;
	int		len;

	error_def(ERR_IOEOF);

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	assert(timeout >= 0);
	iod = io_curr_device.in;
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	/* if write buffer not empty, and there was no error */
	if ((FAB$M_PUT == rm_ptr->r.rab$l_ctx) && (rm_ptr->outbuf != rm_ptr->outbuf_pos) && !iod->dollar.za)
		iorm_wteol(1, iod);

	v->mvtype = MV_STR;
	rm_ptr->r.rab$l_ctx = FAB$M_GET;

	if (rm_ptr->inbuf != rm_ptr->inbuf_pos
		&& (0 != (len = rm_ptr->inbuf_top - rm_ptr->inbuf_pos)))
	{
		assert(len > 0);
		ENSURE_STP_FREE_SPACE(len);
		v->str.addr = stringpool.free;
		memcpy(v->str.addr, rm_ptr->inbuf_pos, len);
		rm_ptr->inbuf_pos = rm_ptr->inbuf;
		v->str.len = len;
		iod->dollar.x = 0;
		iod->dollar.y++;
	} else
	{
		/* ensure extra space for pad if not longword aligned */
		ENSURE_STP_FREE_SPACE(iod->width + (rm_ptr->largerecord ? SIZEOF(uint4) : 0));
		/* need to set usz and make sure width == mrs for fix */
		rab = &rm_ptr->r;
		rab->rab$l_ubf = stringpool.free;
		stat = iorm_get(iod, timeout);
		switch (stat)
		{
		case RMS$_NORMAL:
			v->str.addr = stringpool.free;
			v->str.len = rm_ptr->l_rsz;
			iod->dollar.x = 0;
			iod->dollar.y++;
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
			iod->dollar.za = 0;
			iod->dollar.y++;
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
