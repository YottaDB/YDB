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
#include <fcntl.h>

#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iotimer.h"
#include "iormdef.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "wake_alarm.h"

GBLREF	io_pair	io_curr_device;
GBLREF	bool	out_of_time;

short	iorm_rdone (mint *v, int4 timeout)
{
	bool		timed;
	unsigned char	inchar;
	int		flags;
	int		fcntl_res;
	int4		msec_timeout;	/* timeout in milliseconds */
	io_desc		*io_ptr;
	d_rm_struct	*rm_ptr;
	int4		status;
	TID		timer_id;


	error_def(ERR_IOEOF);

	io_ptr = io_curr_device.in;
	assert (io_ptr->state == dev_open);
	rm_ptr = (d_rm_struct*) (io_ptr->dev_sp);
	if (io_ptr->dollar.x  &&  rm_ptr->lastop == RM_WRITE)
	{
		if (!io_ptr->dollar.za)
			iorm_wteol(1, io_ptr);
		io_ptr->dollar.x = 0;
	}

	rm_ptr->lastop = RM_READ;
	*v = EOF;
	out_of_time = FALSE;
	timer_id = (TID) iorm_rdone;
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
			start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
		}
		else
		{
			out_of_time = TRUE;
			FCNTL2(rm_ptr->fildes, F_GETFL, flags);
			FCNTL3(rm_ptr->fildes, F_SETFL, (flags | O_NDELAY), fcntl_res);
		}
	}
	errno = 0;
	if (rm_ptr->fixed)
	{
	/*
	 * the check for EINTR below is valid and should not be converted to an EINTR
	 * wrapper macro, since action is taken on EINTR, not a retry.
	 */
		DOREADRLTO(rm_ptr->fildes, &inchar, 1, out_of_time, status);
		if (status == EOF  &&  errno == EINTR  &&  out_of_time)
			status = -2;
	}
	else
	{
		/*
		 * the check for EINTR below is valid and should not be converted to an EINTR
		 * wrapper macro, since it might be a timeout.
		 */
		while((EOF == (status = getc(rm_ptr->filstr))) && (EINTR == errno) && !out_of_time);
		if (EOF != status)
		{
			inchar = (unsigned char) status;
			status = 1;		/* one character returned */
		}
		else if (errno == 0)
			status = 0;
		else if (errno == EINTR  &&  out_of_time)
			status = -2;
	}
	if (status == EOF)
	{
		if ((timed)  &&  (!out_of_time))
			cancel_timer(timer_id);
		if (errno == EINTR)
			return FALSE;
		io_ptr->dollar.za = 9;
		rts_error(VARLSTCNT(1) errno);
	}

	if (timed)
	{
		if (msec_timeout == 0)
		{
			FCNTL3(rm_ptr->fildes, F_SETFL, flags, fcntl_res);
			if (rm_ptr->fifo  &&  status == 0)
				return FALSE;
		}
		else
		{
			if (out_of_time)
				return FALSE;
			else
				cancel_timer(timer_id);
		}
	}

	if (status == 0)
	{
		if (!rm_ptr->fifo)
		{
			if (io_ptr->dollar.zeof == TRUE)
			{
				io_ptr->dollar.za = 9;
				rts_error(VARLSTCNT(1) ERR_IOEOF);
			}
			io_ptr->dollar.zeof = TRUE;
			io_ptr->dollar.x = 0;
			io_ptr->dollar.za = 0;
			io_ptr->dollar.y++;
			if (io_ptr->error_handler.len > 0)
				rts_error(VARLSTCNT(1) ERR_IOEOF);
		}
	}
	else
	{
		*v = inchar;
		if (!rm_ptr->fixed  &&  inchar == NATIVE_NL)
		{
		    	io_ptr->dollar.x = 0;
			io_ptr->dollar.y++;
		}
		else
		{
			if (io_ptr->dollar.x++ >= io_ptr->width && io_ptr->wrap)
			{
				io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
				if(io_ptr->length)
					io_ptr->dollar.y %= io_ptr->length;
				io_ptr->dollar.x %= io_ptr->width;
			}
		}
	}
	io_ptr->dollar.za = 0;
	return TRUE;
}
