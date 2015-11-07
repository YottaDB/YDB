/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "stringpool.h"
#include <descrip.h>

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;

int ious_readfl( mval *v, int4 length, int4 t)
{
	struct dsc$descriptor dsc;

	if (length > MAX_US_READ)
		length = MAX_US_READ;
	ENSURE_STP_FREE_SPACE(length);
	dsc.dsc$w_length = length;
	dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	dsc.dsc$b_class = DSC$K_CLASS_S;
	dsc.dsc$a_pointer = stringpool.free;
	((void(*)())(((d_us_struct*)(io_curr_device.in->dev_sp))->disp->readfl))(&dsc);
	v->str.len = dsc.dsc$w_length;
	v->str.addr = stringpool.free;
	return TRUE;
}
