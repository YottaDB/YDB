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
#include "io.h"

GBLREF io_pair io_curr_device;

short ionl_read(mval *v, int4 t)
{
	io_desc *dv;
	error_def(ERR_IOEOF);

	dv = io_curr_device.in;
	if (dv->dollar.zeof == TRUE)
	{
		dv->dollar.za = 9;
		rts_error(VARLSTCNT(1) ERR_IOEOF);
	}
	dv->dollar.zeof = TRUE;
	dv->dollar.x = 0;
	dv->dollar.za = 0;
	dv->dollar.y++;
	if (dv->error_handler.len > 0)
	{	rts_error(VARLSTCNT(1) ERR_IOEOF);
	}
	return TRUE;
}
