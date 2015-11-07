/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"
#include "stringpool.h"

GBLREF io_pair		io_curr_device;

void get_dlr_zkey(mval *v)
{
	(io_curr_device.out->disp_ptr->dlr_zkey)(&v->str);
	v->mvtype = MV_STR;
}
