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

GBLREF io_pair io_curr_device;

int ionl_readfl(mval *val, int4 width, int4 timeout)
{
	io_desc *dv;
	error_def(ERR_IOEOF);

	val->str.len = 0;
	dv = io_curr_device.in;
	dv->dollar.x = 0;
	dv->dollar.y++;
	if (dv->dollar.zeof || (dv->error_handler.len > 0))
	{
		dv->dollar.zeof = TRUE;
		dv->dollar.za = 9;
		rts_error(VARLSTCNT(1) ERR_IOEOF);
	}
	dv->dollar.za = 0;
	dv->dollar.zeof = TRUE;
	return TRUE;
}
