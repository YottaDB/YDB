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
#include <rms.h>
#include "io.h"
#include "iormdef.h"

void iorm_flush(io_desc *iod)
{
	d_rm_struct   *rm_ptr;

	rm_ptr = (d_rm_struct *) iod->dev_sp;
	if (rm_ptr->outbuf < rm_ptr->outbuf_pos)
	{	iorm_wteol(1,iod);
	}
	return;
}

