/****************************************************************
 *								*
 *	Copyright 2008, 2009 Fidelity Information Services, Inc	*
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

void	iopi_iocontrol(mstr *d)
{
	char 		action[MAX_DEVCTL_LENGTH];
	d_rm_struct	*d_rm;
	int		rc;

	error_def(ERR_INVCTLMNE);

	d_rm = (d_rm_struct *) io_curr_device.out->dev_sp;
	if (!d_rm->pipe && !d_rm->fifo)
		return;
	if (0 == d->len)
		return;
	lower_to_upper((uchar_ptr_t)&action[0], (uchar_ptr_t)d->addr, MIN(d->len, SIZEOF(action)));
	if (0 == memcmp(&action[0], "EOF", MIN(d->len, SIZEOF(action))))
	{	/* Implement the write /EOF action. Close the output stream to force any blocked output to complete.
		 * Doing a write /EOF closes the output file descriptor for the current device but does not close the
		 * device (for a pipe this forces any blocked io to be complete). Since the M program could attempt
		 * this command more than once, check if the file is not already closed before the actual close.
		 */
		if (FD_INVALID != d_rm->fildes)
			CLOSEFILE_RESET(d_rm->fildes, rc);	/* resets "d_rm->fildes" to FD_INVALID */
	} else
		rts_error(VARLSTCNT(1) ERR_INVCTLMNE);
	return;
}

void	iopi_dlr_device(mstr *d)
{
	io_desc         *iod;
        int             len;
 	d_rm_struct	*d_rm;

	/* We will default to the output device for setting $device, since pipe uses both */
        iod = io_curr_device.out;
        d_rm = (d_rm_struct *)iod->dev_sp;
 	len = STRLEN(d_rm->dollar_device);
	/* verify internal buffer has enough space for $DEVICE string value */
	assert((int)d->len > len);
	memcpy(d->addr, d_rm->dollar_device, MIN(len,d->len));
	d->len = len;
	return;
}

void	iopi_dlr_key(mstr *d)
{
	io_desc         *iod;
        int             len;
 	d_rm_struct	*d_rm;

        iod = io_curr_device.out;
        d_rm = (d_rm_struct *)iod->dev_sp;

        len = STRLEN(d_rm->dollar_key);
        /* verify internal buffer has enough space for $KEY string value */
        assert((int)d->len > len);
	if (len > 0)
	        memcpy(d->addr, d_rm->dollar_key, MIN(len,d->len));
        d->len = len;
        return;
}
