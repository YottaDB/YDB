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
#include "iosp.h"
#include "iormdef.h"
#include "iormdefsp.h"
#include "gtmio.h"

void iorm_wteol(short x,io_desc *iod)
{
	int	i, fixed_pad, pad_size, res_size;
	int	status;
	d_rm_struct	*rm_ptr;
	error_def(ERR_NOTTOEOFONPUT);
	error_def(ERR_RMSRDONLY);

#ifdef __MVS__
	if (((d_rm_struct *)iod->dev_sp)->fifo)
		rm_ptr = (d_rm_struct *) (iod->pair.out)->dev_sp;
	else
#endif
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	if (rm_ptr->noread)
		rts_error(VARLSTCNT(1) ERR_RMSRDONLY);
	if (!iod->dollar.zeof && !rm_ptr->fifo)
	{
		iod->dollar.za = 9;
		rts_error(VARLSTCNT(1) ERR_NOTTOEOFONPUT);
	}
	rm_ptr->lastop = RM_WRITE;
	for (i = 0; i < x ; i++)
	{
		if (rm_ptr->fixed)
		{
			for (fixed_pad = iod->width - iod->dollar.x; fixed_pad > 0; fixed_pad -= res_size)
			{
				pad_size = (fixed_pad > TAB_BUF_SZ) ? TAB_BUF_SZ : fixed_pad;
				DOWRITERL(rm_ptr->fildes, RM_SPACES_BLOCK, pad_size, res_size);
				if (-1 == res_size)
				{
					iod->dollar.za = 9;
					rts_error(VARLSTCNT(1) errno);
				}
				assert(res_size == pad_size);
			}
		}
		else
		{
			DOWRITERC(rm_ptr->fildes, RMEOL, strlen(RMEOL), status);
			if (0 != status)
			{
				iod->dollar.za = 9;
				rts_error(VARLSTCNT(1) status);
			}
		}
		iod->dollar.x = 0;
	}
	iod->dollar.za = 0;
	iod->dollar.y += x;
	if (iod->length)
		iod->dollar.y %= iod->length;
	return;
}
