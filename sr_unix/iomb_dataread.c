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

#include "gtm_unistd.h"
#include "io_params.h"
#include "io.h"
#include "iombdef.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "gtmio.h"

static	bool	out_of_time;
void	ring_alarm2 (void);

int	iomb_dataread (int timeout)
{

	GBLREF		io_pair	io_curr_device;
	bool		ret, timed;
	int		status, i;
	int4		msec_timeout;	/* timeout in milliseconds */
	io_desc		*io_ptr;
	d_mb_struct	*mb_ptr;
	TID		timer_id;


	error_def(ERR_IOEOF);

	io_ptr = io_curr_device.in;
	io_ptr->dollar.zeof = FALSE;
	mb_ptr = (d_mb_struct *) io_ptr->dev_sp;
	mb_ptr->in_top = mb_ptr->inbuf;
	assert (io_ptr->state == dev_open);
	mb_ptr->timer_on = TRUE;
	timer_id = (TID) iomb_dataread;
	i = 0;
	ret = TRUE;
	out_of_time = FALSE;
	if (timeout == NO_M_TIMEOUT)
	{
		timed = FALSE;
		msec_timeout = NO_M_TIMEOUT;
	}
	else
	{
		timed = TRUE;
		msec_timeout = timeout2msec(timeout);
		if (msec_timeout > 0)
		{
			start_timer(timer_id, msec_timeout, ring_alarm2, 0, NULL);
		}
	}
	while (!out_of_time)
	{
		DOREADRL(mb_ptr->channel, mb_ptr->inbuf, mb_ptr->maxmsg, status);
		if (1 > status)
		{
			if (status == 0)
			{
				if (!timed  ||  msec_timeout == 0)
				{
					io_ptr->dollar.za = 0;
					io_ptr->dollar.zeof = TRUE;
					if (io_ptr->error_handler.len > 0)
					rts_error(VARLSTCNT(1) ERR_IOEOF);
					return FALSE;
				}
			}
			else if (status == -1)
			{
				io_ptr->dollar.za = 9;
				if ((timed) && (!out_of_time))
					cancel_timer(timer_id);
				rts_error(VARLSTCNT(1) errno);
			}
		}
		else
		{
			mb_ptr->in_top += status;
			mb_ptr->in_pos = mb_ptr->inbuf;
			return TRUE;
		}
	}
}


void	ring_alarm2 (void)
{
	out_of_time = TRUE;
	return;
}
