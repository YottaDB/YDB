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

#include <sys/types.h>
#include "gtm_inet.h"

#include "gtm_string.h"
#include "io.h"
#include "iotcpdef.h"

GBLREF io_pair		io_curr_device;

void	iott_iocontrol(mstr *d)
{
	return;
}

void	iott_dlr_device(mstr *d)
{
	io_desc		*iod;
	int		len;

	iod = io_curr_device.out;
	len = STRLEN(iod->dollar.device);
	/* verify internal buffer has enough space for $DEVICE string value */
	assert((int)d->len > len);
	memcpy(d->addr, iod->dollar.device, len);
	d->len = len;
	return;
}

void	iott_dlr_key(mstr *d)
{
	io_desc		*iod;
	int		len;

	iod = io_curr_device.out;
    	len = STRLEN(iod->dollar.key);
	/* verify internal buffer has enough space for $KEY string value */
	assert((int)d->len > len);
	memcpy(d->addr, iod->dollar.key, len);
	d->len = len;
	return;
}
