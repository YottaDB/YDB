/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

GBLREF	io_pair		io_curr_device;

int	iott_read (mval *v, int4 msec_timeout)   /* timeout in milliseconds */

{
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct*) (io_curr_device.in->dev_sp);
	return iott_readfl(v, tt_ptr->in_buf_sz, msec_timeout);
}
