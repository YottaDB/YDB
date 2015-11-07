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
#include <iodef.h>
#include <ssdef.h>

void iomt_tm( io_desc *dev)
{
	d_mt_struct  *mt_ptr;

	iomt_flush(dev);
	iomt_qio(dev, IO$_WRITEMARK, 0); /* writes eof without ext gap */
	mt_ptr = (d_mt_struct *) dev->dev_sp;
	mt_ptr->last_op = ((mt_ptr->last_op == mt_tm || mt_ptr->last_op == mt_tm2)
		? mt_tm2 : mt_tm);
	return;
}
