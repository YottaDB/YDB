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
#include "iousdef.h"
#include <descrip.h>

GBLREF io_pair		io_curr_device;

void ious_write(mstr *v)
{
	struct dsc$descriptor dsc;

	dsc.dsc$w_length = v->len;
	dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	dsc.dsc$b_class = DSC$K_CLASS_S;
	dsc.dsc$a_pointer = v->addr;
	((void(*)())(((d_us_struct*)(io_curr_device.out->dev_sp))->disp->write))(&dsc);
}
