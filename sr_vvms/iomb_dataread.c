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

#include <iodef.h>
#include <ssdef.h>

#include "efn.h"
#include "io.h"
#include "iombdef.h"
#include "iotimer.h"
#include "timedef.h"
#include "outofband.h"

#define SHFT_MSK 0x00000001
#define TIMER_FLAGS 0

GBLREF io_pair	io_curr_device;
GBLREF int4	outofband;

int iomb_dataread(int4 timeout)
{
	bool		timed;
	uint4		efn_mask, status, temp_read_mask, time[2];
	io_desc		*io_ptr;
	d_mb_struct	*mb_ptr;

	error_def(ERR_MBXWRTONLY);
	error_def(ERR_IOEOF);

	io_ptr = io_curr_device.in;
	assert(dev_open == io_ptr->state);
	io_ptr->dollar.zeof = FALSE;
	mb_ptr = (d_mb_struct *)io_ptr->dev_sp;
	if (IO_SEQ_WRT == mb_ptr->promsk)
		rts_error(VARLSTCNT(1) ERR_MBXWRTONLY);
	mb_ptr->in_pos = mb_ptr->in_top = mb_ptr->inbuf;
	temp_read_mask = mb_ptr->read_mask;
#ifdef DEBUG
/* this is for an assert that verifies a reliance on VMS IOSB maintenance */
	mb_ptr->timer_on = TRUE;
#endif
	mb_ptr->stat_blk.status = 0;
	efn_mask = (SHFT_MSK << efn_immed_wait | SHFT_MSK << efn_outofband);
	if ((NO_M_TIMEOUT == timeout) || !timeout)
	{
		timed = FALSE;
		if (!timeout)
			temp_read_mask |= IO$M_NOW;
	} else
	{
		timed = TRUE;
		time[0] = -time_low(timeout);
		time[1] = -time_high(timeout) - 1;
		efn_mask |= SHFT_MSK << efn_timer;
		status = sys$setimr(efn_timer, &time, iomb_cancel_read, mb_ptr, TIMER_FLAGS);
		if (SS$_NORMAL != status)
			rts_error(VARLSTCNT(1) status);
	}
	status = sys$qio(efn_immed_wait, mb_ptr->channel
				,temp_read_mask, &mb_ptr->stat_blk
				,NULL, 0
				,mb_ptr->inbuf, mb_ptr->maxmsg
				,0, 0, 0, 0);
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	status = sys$wflor(efn_immed_wait, efn_mask);
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	if (timed)
	{
		status = sys$cantim(mb_ptr, 0);
		if (SS$_NORMAL != status)
			rts_error(VARLSTCNT(1) status);
	}
	if (!mb_ptr->stat_blk.status)
	{
		if (outofband)
		{
			status = sys$dclast(iomb_cancel_read, mb_ptr, 0);
			if (SS$_NORMAL != status)
				rts_error(VARLSTCNT(1) status);
		}
		status = sys$synch(efn_immed_wait, &mb_ptr->stat_blk);
		if (SS$_NORMAL != status)
			rts_error(VARLSTCNT(1) status);
	}
	if (outofband)
	{
		outofband_action(FALSE);
		assert(FALSE);
	}
	io_ptr->dollar.za = 0;
	switch(mb_ptr->stat_blk.status)
	{
	case SS$_BUFFEROVF:
		/* io_ptr->dollar.za = 1;	* data mangled		* this error information is currently discarded */
	case SS$_NORMAL:
		assert(mb_ptr->stat_blk.char_ct <= mb_ptr->maxmsg);
		if ((0 == timeout) && !mb_ptr->stat_blk.char_ct)
			return FALSE;
		mb_ptr->in_top = mb_ptr->inbuf + mb_ptr->stat_blk.char_ct;
		io_ptr->dollar.za = (short)mb_ptr->stat_blk.pid;	/* $za is too small to hold the full VMS pid */
		return TRUE;
		break;
	case SS$_ABORT:
	case SS$_CANCEL:
		assert(!mb_ptr->timer_on);
		return FALSE;
		break;
	case SS$_ENDOFFILE:
		io_ptr->dollar.za = (short)mb_ptr->stat_blk.pid;	/* $za is too small to hold the full VMS pid */
		if (!io_ptr->dollar.za)
			return FALSE;
		io_ptr->dollar.zeof = TRUE;
		if (io_ptr->error_handler.len > 0)
			rts_error(VARLSTCNT(1) ERR_IOEOF);
		return TRUE;
		break;
	default:							/* NOREADER and NOWRITER are currently unsolicited */
		if (!mb_ptr->stat_blk.status)
			GTMASSERT;
		rts_error(VARLSTCNT(1)  mb_ptr->stat_blk.status);
	}
}
