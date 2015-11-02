/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include <netinet/in.h>

#include "gtm_string.h"
#include "io.h"
#include "iotcpdef.h"

GBLREF io_pair		io_curr_device;

void	iotcp_iocontrol(mstr *d)
{
	return;
}


void	iotcp_dlr_device(mstr *d)
{
	io_desc		*iod;
	int		len;
	d_tcp_struct	*tcpptr;

	iod = io_curr_device.out;
	tcpptr = (d_tcp_struct *)iod->dev_sp;

	len = STRLEN(tcpptr->dollar_device);
	/* verify internal buffer has enough space for $DEVICE string value */
	assert((int)d->len > len);
	memcpy(d->addr, tcpptr->dollar_device, len);
	d->len = len;
	return;
}


void	iotcp_dlr_key(mstr *d)
{
	io_desc		*iod;
	int		len;
	d_tcp_struct	*tcpptr;

	iod = io_curr_device.out;
	tcpptr = (d_tcp_struct *)iod->dev_sp;

    	len = STRLEN(tcpptr->saddr);
	/* verify internal buffer has enough space for $KEY string value */
	assert((int)d->len > len);
	memcpy(d->addr, tcpptr->saddr, len);
	d->len = len;
	return;
}
