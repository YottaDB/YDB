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

#include <errno.h>
#include <fcntl.h>

#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iotimer.h"
#include "iormdef.h"
#include "stringpool.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "wake_alarm.h"

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	bool		out_of_time;

short	iorm_readfl (mval *v, int4 width, int4 timeout) /* timeout in seconds */
{
	bool		ret, timed;
	char		inchar, *temp;
	int		flags;
	int		fcntl_res;
	int4		msec_timeout;	/* timeout in milliseconds */
	int4		i;
	io_desc		*io_ptr;
	d_rm_struct	*rm_ptr;
	int4		status, max_width;
	TID		timer_id;


	error_def(ERR_IOEOF);

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);

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
	timer_id = (TID) iorm_readfl;
	max_width = io_ptr->width - io_ptr->dollar.x;
	width = (width < max_width) ? width : max_width;
	i = 0;
	ret = TRUE;
	temp = (char*) stringpool.free;
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
		 * wrapper macro, since it might be a timeout.
		 */
		DOREADRLTO(rm_ptr->fildes, temp, width, out_of_time, status);
		if (0 > status)
		{
			i = 0;
			if (errno == EINTR  &&  out_of_time)
				status = -2;
		}
		else
			i = status;
	}
	else
	{
		do
		{
			if ((status = getc (rm_ptr->filstr)) != EOF)
			{
				inchar = (unsigned char) status;
				if (inchar == NATIVE_NL)
					break;
				*temp++ = inchar;
				i++;
			}
			else
			{
				inchar = 0;
				if (errno == 0)
					status = 0;
				else if (errno == EINTR)
				{
					if (out_of_time)
						status = -2;
					else
						continue;		/* Ignore interrupt if not our wakeup */
				}
				break;
			}
		} while (i < width);
	}
	if (status == EOF  &&  errno != EINTR)
	{
		io_ptr->dollar.za = 9;
		v->str.len = 0;
		if (timed  &&  !out_of_time)
			cancel_timer(timer_id);
		rts_error(VARLSTCNT(1) errno);
	}

	if (timed)
	{
		if (msec_timeout == 0)
		{
			FCNTL3(rm_ptr->fildes, F_SETFL, flags, fcntl_res);
			if (rm_ptr->fifo  &&  status == 0)
				ret = FALSE;
		}
		else
		{
			if (out_of_time)
				ret = FALSE;
			else
				cancel_timer(timer_id);
		}
	}

	if (status == 0  &&  i == 0  &&  !rm_ptr->fifo)
	{
		v->str.len = 0;
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
		{	rts_error(VARLSTCNT(1) ERR_IOEOF);
		}
	}
	else
	{
		v->str.len = i;
		v->str.addr = (char *) stringpool.free;
		if (!rm_ptr->fixed  &&  inchar == NATIVE_NL)
		{
		    	io_ptr->dollar.x = 0;
			io_ptr->dollar.y++;
		}
		else
			if ((io_ptr->dollar.x += i) >= io_ptr->width && io_ptr->wrap)
			{
				io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
				if(io_ptr->length)
					io_ptr->dollar.y %= io_ptr->length;
				io_ptr->dollar.x %= io_ptr->width;
			}
	}
	io_ptr->dollar.za = 0;
	return((short) ret);
}
