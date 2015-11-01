/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <string.h>

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "io.h"
#include "iormdef.h"
#include "iormdefsp.h"
#include "gtmio.h"
#include "min_max.h"

GBLREF io_pair		io_curr_device;

void iorm_write(mstr *v)
{
	io_desc		*iod;
	char		*out;
	int		inlen, outlen, status, len;
	d_rm_struct	*rm_ptr;
	error_def(ERR_NOTTOEOFONPUT);
	error_def(ERR_RMSRDONLY);

	iod = io_curr_device.out;
#ifdef __MVS__
	if (((d_rm_struct *)iod->dev_sp)->fifo)
		rm_ptr = (d_rm_struct *) (iod->pair.out)->dev_sp;
	else
#endif
	rm_ptr = (d_rm_struct *) iod->dev_sp;
	if (rm_ptr->noread)
		rts_error(VARLSTCNT(1) ERR_RMSRDONLY);
	if (!iod->dollar.zeof && !rm_ptr->fifo)
	{
	 	iod->dollar.za = 9;
		rts_error(VARLSTCNT(1) ERR_NOTTOEOFONPUT);
	}

	rm_ptr->lastop = RM_WRITE;
	inlen = v->len;
	if (rm_ptr->stream && !iod->wrap)
		outlen = iod->width;
	else
		outlen = iod->width - iod->dollar.x;

	if (!iod->wrap && inlen > outlen)
		inlen = outlen;
	if (!inlen)
		return;
	if (outlen > inlen)
		outlen = inlen;
	for (out = v->addr; ; out += len)
	{
		len = MIN(inlen, outlen);
		DOWRITERC(rm_ptr->fildes, out, len, status);
		if (0 != status)
		{
			iod->dollar.za = 9;
			rts_error(VARLSTCNT(1) status);
		}
		iod->dollar.x += len;
		if ((inlen -= len) <= 0)
			break;

		if (!rm_ptr->stream || iod->wrap)
		/* implicit record termination for non-stream files
		 * or stream files with the "wrap" option.
		 */
		{
			if (!rm_ptr->fixed && iod->wrap)
			{
				DOWRITERC(rm_ptr->fildes, RMEOL, strlen(RMEOL), status);
				if (0 != status)
				{	iod->dollar.za = 9;
					rts_error(VARLSTCNT(1) status);
				}
			}

			iod->dollar.x = 0;	/* don't use wteol to terminate wrapped records for fixed. */
			iod->dollar.y++;	/* \n is reserved as an end-of-rec delimiter for variable format */
			if (iod->length)	/* and fixed format requires no padding for wrapped records */
				iod->dollar.y %= iod->length;

			outlen = iod->width;
		}
		if (outlen > inlen)
			outlen = inlen;
	}
	iod->dollar.za = 0;
        return;
}
