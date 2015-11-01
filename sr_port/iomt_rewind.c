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
#include "io_params.h"

void
iomt_rewind (io_desc *dev)
{
	d_mt_struct    *mt_ptr;

	mt_ptr = (d_mt_struct *) dev->dev_sp;
	if (mt_ptr->labeled == MTLAB_ANSI)
	{
		if (mt_ptr->last_op == mt_write)
		{
			iomt_tm (dev);
			iomt_wtansilab (dev, MTL_EOF1 | MTL_EOF2);
			iomt_tm (dev);
			iomt_tm (dev);
		}
	} else
	{
		if (mt_ptr->last_op == mt_write)
		{
			iomt_flush (dev);
			iomt_eof (dev);
		}
#ifdef VMS
		if (mt_ptr->last_op == mt_eof)
			iomt_eof (dev);
#else		/* check to see if this device requires an extra filemark */
		if (mt_ptr->cap.req_extra_filemark
		    && mt_ptr->last_op == mt_eof)
			iomt_eof (dev);
#endif
	}
	iomt_qio (dev, IO_REWIND, 0);
	mt_ptr->last_op = mt_rewind;
	dev->dollar.zeof = FALSE;
	dev->dollar.x = 0;
	dev->dollar.y = 0;
	return;
}
