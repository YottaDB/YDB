/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

error_def(ERR_IOEOF);

int ionl_readfl(mval *val, int4 width, int4 timeout)
{
	io_desc *dv;

	val->str.len = 0;
	dv = io_curr_device.in;
	dv->dollar.x = 0;
	dv->dollar.y++;
	if (dv->dollar.zeof || (dv->error_handler.len > 0))
	{
		dv->dollar.zeof = TRUE;
		dv->dollar.za = ZA_IO_ERR;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IOEOF);
	}
	dv->dollar.za = 0;
	dv->dollar.zeof = TRUE;
	return TRUE;
}
