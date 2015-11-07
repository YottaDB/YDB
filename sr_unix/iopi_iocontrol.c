/****************************************************************
 *								*
 *	Copyright 2008, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iopi_iocontrol.c */

#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iormdef.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "gtm_caseconv.h"
#include "min_max.h"
#include "gtmio.h"

GBLREF io_pair		io_curr_device;
error_def(ERR_INVCTLMNE);

void	iopi_iocontrol(mstr *d)
{
	char 		action[MAX_DEVCTL_LENGTH];
	d_rm_struct	*d_rm;
	int		rc;

	d_rm = (d_rm_struct *) io_curr_device.out->dev_sp;
	/* WRITE /EOF only applies to PIPE devices.  Sequential file and FIFO devices should be closed with CLOSE.*/
	if (!d_rm->pipe)
		return;
	/* we should not get here unless there is some string length after write / */
	assert((int)d->len);
	if (0 == d->len)
		return;
	lower_to_upper((uchar_ptr_t)&action[0], (uchar_ptr_t)d->addr, MIN(d->len, SIZEOF(action)));
	if (0 == memcmp(&action[0], "EOF", MIN(d->len, SIZEOF(action))))
	{	/* Implement the write /EOF action. Close the output stream to force any blocked output to complete.
		 * Doing a write /EOF closes the output file descriptor for the pipe device but does not close the
		 * device. Since the M program could attempt this command more than once, check if the file descriptor
		 * is already closed before the actual close.
		 * Ignore the /EOF action if the device is read-only as this is a nop
		 */
		if (d_rm->noread)
			return;
		if (FD_INVALID != d_rm->fildes)
		{
			/* The output will be flushed via iorm_flush() like in iorm_close.c.  After this call returns,
			 * $X will be zero which will keep iorm_readfl() from attempting an iorm_wteol() in the fix mode
			 * after the file descriptor has been closed.
			 */
			iorm_flush(io_curr_device.in);
			CLOSEFILE_RESET(d_rm->fildes, rc);	/* resets "d_rm->fildes" to FD_INVALID */
		}
	} else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVCTLMNE);
	return;
}

void	iopi_dlr_device(mstr *d)
{
	io_desc         *iod;
        int             len;

	/* We will default to the output device for setting $device, since pipe uses both */
        iod = io_curr_device.out;
 	len = STRLEN(iod->dollar.device);
	/* verify internal buffer has enough space for $DEVICE string value */
	assert((int)d->len > len);
	memcpy(d->addr, iod->dollar.device, MIN(len,d->len));
	d->len = len;
	return;
}

void	iopi_dlr_key(mstr *d)
{
	io_desc         *iod;
        int             len;

        iod = io_curr_device.out;

        len = STRLEN(iod->dollar.key);
        /* verify internal buffer has enough space for $KEY string value */
        assert((int)d->len > len);
	if (len > 0)
	        memcpy(d->addr, iod->dollar.key, MIN(len,d->len));
        d->len = len;
        return;
}
