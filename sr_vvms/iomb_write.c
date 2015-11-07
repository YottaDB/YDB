/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <ssdef.h>
#include <iodef.h>
#include "efn.h"
#include "io.h"
#include "iombdef.h"
#include "outofband.h"

#define SHFT_MSK 0x00000001

GBLREF int4	outofband;
GBLREF io_pair	io_curr_device;

void iomb_write(mstr *v)
{
	unsigned char 		*ch;
	int			len, out_len;
	uint4		status, efn_mask;
	d_mb_struct 		*mb_ptr;
	mb_iosb 		stat_blk;
	error_def(ERR_MBXRDONLY);

	mb_ptr = (d_mb_struct *)io_curr_device.out->dev_sp;
	if (mb_ptr->promsk == IO_RD_ONLY)
		rts_error(VARLSTCNT(1) ERR_MBXRDONLY);

	efn_mask = (SHFT_MSK << efn_immed_wait | SHFT_MSK << efn_outofband);
	for (ch = v->addr, len = v->len ; ; )
	{	if (len > mb_ptr->maxmsg)
			out_len = mb_ptr->maxmsg;
		else
			out_len = len;
		status = sys$qio(efn_immed_wait ,mb_ptr->channel
	  			,mb_ptr->write_mask ,&stat_blk
				,NULL,0
				,ch ,out_len ,0 ,0 ,0 ,0);
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);

		status = sys$wflor(efn_immed_wait,efn_mask);
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);

		if (outofband)
		{
			status = sys$cancel(mb_ptr->channel);
			if (status != SS$_NORMAL)
				rts_error(VARLSTCNT(1) status);
			status = sys$synch(efn_immed_wait, &stat_blk);
			if (status != SS$_NORMAL)
				rts_error(VARLSTCNT(1) status);
			assert(stat_blk.status == SS$_CANCEL || stat_blk.status == SS$_ABORT);
			outofband_action(FALSE);
			assert(FALSE);
		}

		if (stat_blk.status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);

		if ((len -= out_len) == 0)
			break;
		ch += out_len;
	}
	if ((io_curr_device.out->dollar.x += v->len) > io_curr_device.out->width && io_curr_device.out->wrap)
	{
		io_curr_device.out->dollar.y += (io_curr_device.out->dollar.x / io_curr_device.out->width);
		if (io_curr_device.out->length)
			io_curr_device.out->dollar.y %= io_curr_device.out->length;
		io_curr_device.out->dollar.x %= io_curr_device.out->width;
	}
}
