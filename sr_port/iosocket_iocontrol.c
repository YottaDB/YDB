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
#include "stringpool.h"

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;

error_def(ERR_INVCTLMNE);

void	iosocket_iocontrol(mstr *d)
{
	char 		action[MAX_DEVCTL_LENGTH];
	unsigned short 	depth; /* serve as depth for LISTEN and timeout for WAIT */
	int		length, n, timeout;

	if (0 == d->len)
		return;
	/* The contents of d->addr are passed to us from op_iocontrol. That routine sets up these parms
	 * but does not zero terminate the string we are passing to SSCANF. If this is not a complex parm
	 * with parens (as is the case with WRITE /WAIT type statements), SSCANF() will get into trouble
	 * if the stringpool contains extra junk. For that reason, we now add a null terminator and make
	 * sure the argument is not too big to parse into our buffer above.
	 */
	assert(d->addr == (char *)stringpool.free); 	/* Verify string is where we think it is so we don't corrupt something */
	assert(MAX_DEVCTL_LENGTH > d->len);
	assert(IS_IN_STRINGPOOL(d->addr, d->len));
	*(d->addr + d->len) = '\0';
	if (0 == (n = SSCANF(d->addr, "%[^(](%hu)", &action[0], &depth)))
		memcpy(&action[0], d->addr, d->len);
	if (0 == (length = STRLEN(&action[0])))
		return;
	lower_to_upper((uchar_ptr_t)&action[0], (uchar_ptr_t)&action[0], length);
	if (0 == memcmp(&action[0], "LISTEN", length))
	{
		if (2 > n)
			depth = DEFAULT_LISTEN_DEPTH;
		iosocket_listen(io_curr_device.out, depth);
	} else if (0 == memcmp(&action[0], "WAIT", length))
	{
		timeout = depth;
		if (2 > n)
			timeout = NO_M_TIMEOUT;
		iosocket_wait(io_curr_device.out, timeout); /* depth really means timeout here. */
	} else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVCTLMNE);

	return;
}

void	iosocket_dlr_device(mstr *d)
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

void	iosocket_dlr_key(mstr *d)
{
	io_desc         *iod;
        int             len;

        iod = io_curr_device.out;
        len = STRLEN(iod->dollar.key);
        /* verify internal buffer has enough space for $DEVICE string value */
        assert((int)d->len > len);
	if (len > 0)
	        memcpy(d->addr, iod->dollar.key, len);
        d->len = len;
        return;
}
