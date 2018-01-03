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

#include <sys/types.h>
#include "gtm_inet.h"

#include "gtm_string.h"
#include "io.h"

GBLREF io_pair		io_curr_device;

void	iott_iocontrol(mstr *mn, int4 argcnt, va_list args)
{
	return;
}

void	iott_dlr_device(mstr *d)
{
	io_desc		*iod;

	iod = io_curr_device.in;
	PUT_DOLLAR_DEVICE_INTO_MSTR(iod, d);
	return;
}

void	iott_dlr_key(mstr *d)
{
	io_desc		*iod;
	int		len;

	iod = io_curr_device.in;
    	len = STRLEN(iod->dollar.key);
	/* verify internal buffer has enough space for $KEY string value */
	assert((int)d->len > len);
	memcpy(d->addr, iod->dollar.key, len);
	d->len = len;
	return;
}
