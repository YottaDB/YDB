/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_inet.h"
#include "gtm_string.h"

#ifdef UNIX
#include "gtm_fcntl.h"
#include "eintr_wrappers.h"
static int fcntl_res;
#endif

#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iotcp_select.h"
#include "iotcproutine.h"
#include "stringpool.h"
#include "wake_alarm.h"

/* Maximum we will read in one read for this device */
#define MAX_READLEN 32767

GBLREF	io_pair			io_curr_device;
GBLREF	bool			out_of_time;
GBLREF	spdesc			stringpool;
GBLREF	tcp_library_struct	tcp_routines;
GBLREF	int4			outofband;

error_def(ERR_IOEOF);
error_def(ERR_TEXT);
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_SETSOCKOPTERR);

int	iotcp_readfl(mval *v, int4 width, int4 timeout)
/* 0 == width is a flag that the caller is read and the length is not actually fixed */
/* timeout in seconds */
{
	/* VMS uses the UCX interface; should support others that emulate it */
	boolean_t	ret, timed, vari;
	int		flags, len, real_errno, save_errno;
	int		i;
	io_desc		*io_ptr;
	d_tcp_struct	*tcpptr;
	int4		status;
	int4		msec_timeout;	/* timeout in milliseconds */
	TID		timer_id;
	ABS_TIME	cur_time, end_time, time_for_read, lcl_time_for_read, zero;
	fd_set		tcp_fd;
	char		*errptr;
	int4		errlen;

#ifdef DEBUG_TCP
	PRINTF("%s >>>\n", __FILE__);
#endif
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	if (0 == width)
	{	/* op_readfl won't do this; must be a call from iotcp_read */
		vari = TRUE;
		width = MAX_READLEN;
	} else
	{
		vari = FALSE;
		width = (width < MAX_READLEN) ? width : MAX_READLEN;
	}
	ENSURE_STP_FREE_SPACE(width);
	io_ptr = io_curr_device.in;
	assert(dev_open == io_ptr->state);
	tcpptr = (d_tcp_struct *)(io_ptr->dev_sp);
	if (io_ptr->dollar.x && (TCP_WRITE == tcpptr->lastop))
	{	/* switching from write to read */
#ifdef C9A06001531
		/* pending change request C9A06-001531 */
		if (!io_ptr->dollar.za)
			iotcp_wteol(1, io_ptr);
#endif
		io_ptr->dollar.x = 0;
	}
	tcpptr->lastop = TCP_READ;
	ret = TRUE;
	timer_id = (TID)iotcp_readfl;
	out_of_time = FALSE;
	time_for_read.at_sec = ((0 == timeout) ? 0 : 1);
	time_for_read.at_usec = 0;
	if (NO_M_TIMEOUT == timeout)
	{
		timed = FALSE;
		msec_timeout = NO_M_TIMEOUT;
	} else
	{
		timed = TRUE;
		msec_timeout = timeout2msec(timeout);
		if (msec_timeout > 0)
		{	/* there is time to wait */
#ifdef UNIX
			/* set blocking I/O */
			FCNTL2(tcpptr->socket, F_GETFL, flags);
			if (flags < 0)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
						LEN_AND_LIT("F_GETFL FOR NON BLOCKING I/O"),
						save_errno, LEN_AND_STR(errptr));
			}
			FCNTL3(tcpptr->socket, F_SETFL, flags & (~(O_NDELAY | O_NONBLOCK)), fcntl_res);
			if (fcntl_res < 0)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
						LEN_AND_LIT("F_SETFL FOR NON BLOCKING I/O"),
						save_errno, LEN_AND_STR(errptr));
			}
#endif
			sys_get_curr_time(&cur_time);
			add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
		} else
			out_of_time = TRUE;
	}
	for (i = 0, status = 0; status >= 0; )
	{
		FD_ZERO(&tcp_fd);
		FD_SET(tcpptr->socket, &tcp_fd);
		assert(0 != FD_ISSET(tcpptr->socket, &tcp_fd));
		assert(((1 == time_for_read.at_sec) || (0 == time_for_read.at_sec)) && (0 == time_for_read.at_usec));
		/*
		 * the check for EINTR below is valid and should not be converted to an EINTR
		 * wrapper macro, since it might be a timeout.
		 */
		lcl_time_for_read = time_for_read;
		status = tcp_routines.aa_select(tcpptr->socket + 1, (void *)(&tcp_fd), (void *)0, (void *)0,
							&lcl_time_for_read);
		if (status > 0)
		{
			status = tcp_routines.aa_recv(tcpptr->socket, (char *)(stringpool.free +  i), width - i, 0);
			if ((0 == status) || ((-1 == status) && (ECONNRESET == errno || EPIPE == errno || EINVAL == errno)))
			{ /* lost connection. */
				if (0 == status)
					errno = ECONNRESET;
				real_errno = errno;
				status = -2;
				break;
			}

		}
		if (status < 0)
		{
			if (EINTR == errno && FALSE == out_of_time)
			{	/* interrupted by a signal which is not OUR timer, continue looping */
				status = 0;
			} else
				real_errno = errno;
		} else
			real_errno = 0;
		if (outofband)
			break;
		if (timed)
		{
			if (msec_timeout > 0)
			{
				sys_get_curr_time(&cur_time);
				cur_time = sub_abs_time(&end_time, &cur_time);
				if (cur_time.at_sec <= 0)
				{
					out_of_time = TRUE;
					cancel_timer(timer_id);
					if (status > 0)
						i += status;
					break;
				}
			} else
			{
				if (status > 0)
					i += status;
				break;
			}
		}
		if (0 > status)
			break;
		i += status;
		if ((vari && (0 != i)) || (i >= width))
			break;
	}
	if (EINTR == real_errno)
		status = 0;	/* don't treat a <CTRL-C> or timeout as an error */
	if (timed)
	{
		if (0 == msec_timeout)
		{
			if (0 == status)
				ret = FALSE;
		} else
		{
#ifdef UNIX
			real_errno = errno;
			FCNTL3(tcpptr->socket, F_SETFL, flags, fcntl_res);
			if (fcntl_res < 0)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
						LEN_AND_LIT("F_SETFL FOR RESTORING SOCKET OPTIONS"),
					  	save_errno, LEN_AND_STR(errptr));
			}
			errno = real_errno;
#endif
			if (out_of_time && (i < width))
				ret = FALSE;
			else
				cancel_timer(timer_id);
		}
	}
	if (i > 0)
	{	/* there's somthing to return */
		v->str.len = i;
		v->str.addr = (char *)stringpool.free;
		if (((io_ptr->dollar.x += i) >= io_ptr->width) && (TRUE == io_ptr->wrap))
		{
			io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
			if (0 != io_ptr->length)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x %= io_ptr->width;
		}
	} else
		v->str.len = 0;
#ifdef DEBUG_TCP
	PRINTF("%s <<<\n", __FILE__);
#endif
	len = SIZEOF("1,") - 1;
	if (status >= 0)
	{	/* no real problems */
		io_ptr->dollar.za = 0;
/*	the handling of urgent data doesn't work and never has, the design should be changed to use a /URGENT controlnmemonic
 *	because there is really only one character available at a time
		zero.at_sec = zero.at_usec = 0;
		FD_ZERO(&tcp_fd);
		FD_SET(tcpptr->socket, &tcp_fd);
		if (tcp_routines.aa_select(tcpptr->socket + 1, (void *)(tcpptr->urgent ? &tcp_fd : 0), (void *)0,
								(void *)(tcpptr->urgent ? 0 : &tcp_fd), &zero) > 0)
		{
			memcpy(io_ptr->dollar.device, "1,", len);
			if (tcpptr->urgent)
			{
				memcpy(&io_ptr->dollar.device[len], "No ",SIZEOF("No "));
				len += SIZEOF("No ") - 1;
			}
			memcpy(&io_ptr->dollar.device[len], "Urgent Data", SIZEOF("Urgent Data"));
		} else
*/
			memcpy(io_ptr->dollar.device, "0", SIZEOF("0"));
	} else
	{	/* there's a significant problem */
		if (0 == i)
			io_ptr->dollar.x = 0;
		io_ptr->dollar.za = 9;
		memcpy(io_ptr->dollar.device, "1,", len);
		errptr = (char *)STRERROR(errno);
		errlen = STRLEN(errptr);
		memcpy(&io_ptr->dollar.device[len], errptr, errlen);
		if (io_ptr->dollar.zeof || -1 == status || 0 < io_ptr->error_handler.len)
		{
			io_ptr->dollar.zeof = TRUE;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_IOEOF, 0, ERR_TEXT, 2, errlen, errptr);
		} else
			io_ptr->dollar.zeof = TRUE;
	}
	return (ret);
}
