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
#include "gtm_unistd.h"

#include <errno.h>

#include "io_params.h"
#include "io.h"
#include "iombdef.h"
#include "eintr_wrappers.h"
#include "outofband.h"

GBLREF int4	outofband;

void iomb_write(mstr *v)
{
	GBLREF	io_pair io_curr_device;
	uint4	status, efn_mask;
	d_mb_struct *mb_ptr;
	char	*ch;
	short 	len,out_len;
	error_def(ERR_MBXRDONLY);

	mb_ptr = (d_mb_struct *) io_curr_device.out->dev_sp;
	if (mb_ptr->promsk == IO_RD_ONLY)
		rts_error(VARLSTCNT(1) ERR_MBXRDONLY);

	for (ch = v->addr,len = v->len ; ;  )
	{
		if (len > mb_ptr->maxmsg)
			out_len = mb_ptr->maxmsg;
		else
			out_len = len;

		WRITE_FILE(mb_ptr->channel, ch, out_len, status);
		while (-1 == status)
		{
			if((mb_ptr->write_mask == 1) && (errno == EPIPE))
			{
				if (outofband)
					outofband_action(TRUE);
			}
			else
				rts_error(VARLSTCNT(1) errno);
			WRITE_FILE(mb_ptr->channel, ch, out_len, status);
		}
		if ((len -= out_len) == 0)
			break;
		ch += out_len;
	}
	io_curr_device.out->dollar.x += v->len;
}
