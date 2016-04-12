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
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"

GBLREF io_pair  io_curr_device;

void iomt_wtff (void)
{
	io_desc        *dv;
	d_mt_struct    *mt_ptr;

	dv = io_curr_device.out;
	mt_ptr = (d_mt_struct *) dv->dev_sp;

#ifdef UNIX
	if (mt_ptr->mode != MT_M_WRITE)
	{
		uint4   status;

		status = iomt_reopen (dv, MT_M_WRITE, FALSE);
		if (status != SS_NORMAL)
			return;
	}
#endif
	iomt_wtone (12);
	if (!mt_ptr->stream)
		iomt_wteol (1, dv);

	mt_ptr->rec.len = 0;
	io_curr_device.out->dollar.x = 0;
	io_curr_device.out->dollar.y = 0;
}
