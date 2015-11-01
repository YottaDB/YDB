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

/* iomt_erase.c - Erase tape. */
#include "mdef.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"

void
iomt_erase (io_desc *dev)
{
	static readonly int4 mask = IO_WRITELBLK | IO_M_ERASE;

	d_mt_struct    *mt_ptr;

	iomt_flush (dev);
	iomt_rewind (dev);
	iomt_qio (dev, mask, 0);
	mt_ptr = (d_mt_struct *) dev->dev_sp;
	mt_ptr->last_op = mt_rewind;
	return;
}
