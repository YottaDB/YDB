/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_iocontrol.c */

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "gtm_caseconv.h"

GBLREF io_pair		io_curr_device;

void	iosocket_iocontrol(mstr *d)
{
	char 		action[MAX_DEVCTL_LENGTH];
	unsigned short 	depth; /* serve as depth for LISTEN and timeout for WAIT */
	int		length, n;

	error_def(ERR_INVCTLMNE);

	if (0 == d->len)
		return;
	memset(&action[0], 0, sizeof(action));
	if (2 != (n = SSCANF(d->addr, "%[^(](%hu)", &action[0], &depth)))
		memcpy(&action[0], d->addr, d->len);
	if (0 == (length = strlen(&action[0])))
		return;
	lower_to_upper((uchar_ptr_t)&action[0], (uchar_ptr_t)&action[0], length);
	if (0 == memcmp(&action[0], "LISTEN", length))
	{
		if (2 != n)
			depth = DEFAULT_LISTEN_DEPTH;
		iosocket_listen(io_curr_device.out, depth);
	}
	else if (0 == memcmp(&action[0], "WAIT", length))
	{
		if (2 != n)
			depth = NO_M_TIMEOUT;
		iosocket_wait(io_curr_device.out, depth); /* depth really means timeout here. */
	}
	else
	{
		rts_error(VARLSTCNT(1) ERR_INVCTLMNE);
	}

	return;
}

void	iosocket_dlr_device(mstr *d)
{
	io_desc		*iod;
	int		len;
	d_socket_struct *dsocketptr;

	iod = io_curr_device.out;
	dsocketptr = (d_socket_struct *)iod->dev_sp;

	len = strlen(dsocketptr->dollar_device);
	/* verify internal buffer has enough space for $DEVICE string value */
	assert((int)d->len > len);
	memcpy(d->addr, dsocketptr->dollar_device, len);
	d->len = len;
	return;
}

void	iosocket_dlr_key(mstr *d)
{
	io_desc         *iod;
        int             len;
        d_socket_struct *dsocketptr;

        iod = io_curr_device.out;
        dsocketptr = (d_socket_struct *)iod->dev_sp;

        len = strlen(dsocketptr->dollar_key);
        /* verify internal buffer has enough space for $DEVICE string value */
        assert((int)d->len > len);
	if (len > 0)
	        memcpy(d->addr, dsocketptr->dollar_key, len);
        d->len = len;
        return;
}
