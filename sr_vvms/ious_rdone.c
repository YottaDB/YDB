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
#include "iousdef.h"
#include <descrip.h>

GBLREF io_pair		io_curr_device;

int ious_rdone(mint *v, int4 t)
{
	struct dsc$descriptor dsc;
	unsigned char	p;

	p = 0;
	dsc.dsc$w_length = 1;
	dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	dsc.dsc$b_class = DSC$K_CLASS_S;
	dsc.dsc$a_pointer = &p;
	((void(*)())(((d_us_struct*)(io_curr_device.in->dev_sp))->disp->rdone))(&dsc);
	*v = p;
	return TRUE;
}
