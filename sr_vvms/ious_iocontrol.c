/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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

void ious_iocontrol(mstr *d)
{
	struct dsc$descriptor val;

	val.dsc$w_length = d->len;
	val.dsc$b_dtype = DSC$K_DTYPE_T;
	val.dsc$b_class = DSC$K_CLASS_S;
	val.dsc$a_pointer = d->addr;

	(((d_us_struct*)(io_curr_device.out->dev_sp))->disp->iocontrol)(&val);
	return;
}

void ious_dlr_device(mstr *d)
{
	struct dsc$descriptor val;

	val.dsc$w_length = d->len;
	val.dsc$b_dtype = DSC$K_DTYPE_T;
	val.dsc$b_class = DSC$K_CLASS_S;
	val.dsc$a_pointer = d->addr;

	(((d_us_struct*)(io_curr_device.out->dev_sp))->disp->dlr_device)(&val);
	d->len = val.dsc$w_length;
	return;
}

void ious_dlr_key(mstr *d)
{
	struct dsc$descriptor val;

	val.dsc$w_length = d->len;
	val.dsc$b_dtype = DSC$K_DTYPE_T;
	val.dsc$b_class = DSC$K_CLASS_S;
	val.dsc$a_pointer = d->addr;

	(((d_us_struct*)(io_curr_device.out->dev_sp))->disp->dlr_key)(&val);
	d->len = val.dsc$w_length;
	return;
}
