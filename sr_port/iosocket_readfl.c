/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_readfl.c */
#include "mdef.h"
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_time.h"
#ifdef __MVS__
#include <sys/time.h>
#endif
#include "gtm_socket.h"
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
#include "iotcproutine.h"
#include "stringpool.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "outofband.h"
#include "wake_alarm.h"

#define	TIMEOUT_INTERVAL	200000
GBLREF	io_pair 		io_curr_device;
GBLREF	bool			out_of_time;
GBLREF	spdesc 			stringpool;
GBLREF	tcp_library_struct	tcp_routines;
GBLREF	volatile int4		outofband;
/* VMS uses the UCX interface; should support others that emulate it */
short	iosocket_readfl(mval *v, int4 width, int4 timeout)
					/* width == 0 is a flag for non-fixed length read */
					/* timeout in seconds */
{
	bool		ret;
	boolean_t 	timed, vari, more_data, terminator, term_mode;
	int		flags, len, real_errno, save_errno;
	short int	i;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	int4		msec_timeout; /* timeout in milliseconds */
	TID		timer_id;
	ABS_TIME	cur_time, end_time, save_time_for_read, time_for_read, zero;
	char		*errptr;
	int4		errlen;
	unsigned char	inchar;
	int 		ii;
	ssize_t		status;

	error_def(ERR_IOEOF);
	error_def(ERR_TEXT);
	error_def(ERR_CURRSOCKOFR);
	error_def(ERR_NOSOCKETINDEV);
	error_def(ERR_GETSOCKOPTERR);
	error_def(ERR_SETSOCKOPTERR);

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	if (0 == width)
	{	/* op_readfl won't do this; must be a call from iosocket_read */
		vari = TRUE;
		width = MAX_STRLEN;
	} else
	{
		vari = FALSE;
		width = (width < MAX_STRLEN) ? width : MAX_STRLEN;
	}
	if (stringpool.free + width > stringpool.top)
		stp_gcol(width);
	iod = io_curr_device.in;
	assert(dev_open == iod->state);
	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)(iod->dev_sp);
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	if (0 >= dsocketptr->n_socket)
	{
		rts_error(VARLSTCNT(1) ERR_NOSOCKETINDEV);
		return 0;
	}
	if (dsocketptr->n_socket <= dsocketptr->current_socket)
        {
		rts_error(VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
                return 0;
        }
	if (iod->dollar.x  &&  (TCP_WRITE == socketptr->lastop))
	{	/* switching from write to read */
		if (!iod->dollar.za)
			iosocket_flush(iod);
		iod->dollar.x = 0;
	}
	socketptr->lastop = TCP_READ;
	ret = TRUE;
	if (0 >= socketptr->n_delimiter)
		term_mode = FALSE;
	else
		term_mode = TRUE;
	time_for_read.at_sec  = 0;
	time_for_read.at_usec = ((0 == timeout) ? 0 : TIMEOUT_INTERVAL);
	/* ATTN: the 200000 above is mystery. This should be machine dependent 	*/
	/*       When setting it, following aspects needed to be considered	*/
	/*	 1. Not too long to let users at direct mode feel uncomfortable */
	/*		i.e. r x:0 or r x  will wait for this long to return	*/
	/*		     	even when there is something to read from the 	*/
	/*			socket 						*/
	/*	 2. Not too short so that when it is waiting for something 	*/
	/* 	    from a socket, it won't load up the CPU. This shall be able */
	/*	    to be omited when the next item is considered.		*/
	/*	 3. Not too short so that it won't cut one message into pieces	*/
	/*	    when the read is issued first.				*/
	/*		for w "abcd", 10 us will do it				*/
	/* 		for w "ab",!,"cd",!,"ef"  it will have to be larger     */
	/*			than 50000 us on beowulf.			*/
	/* This is gonna be a headache.						*/
	timer_id = (TID)iosocket_readfl;
	out_of_time = FALSE;
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
			FCNTL2(socketptr->sd, F_GETFL, flags);
			if (flags < 0)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error(VARLSTCNT(7) ERR_GETSOCKOPTERR, 5, LEN_AND_LIT("F_GETFL FOR NON BLOCKING I/O"),
						save_errno, LEN_AND_STR(errptr));
			}
			FCNTL3(socketptr->sd, F_SETFL, flags & (~(O_NDELAY | O_NONBLOCK)), fcntl_res);
			if (fcntl_res < 0)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("F_SETFL FOR NON BLOCKING I/O"),
						save_errno, LEN_AND_STR(errptr));
			}
#endif
			sys_get_curr_time(&cur_time);
			add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
		} else
			out_of_time = TRUE;
	}
	more_data = TRUE;
	dsocketptr->dollar_key[0] = '\0';
	iod->dollar.zb[0] = '\0';
	for (i = 0, status = 0;  status >= 0;)
	{
		status = iosocket_snr(socketptr, &inchar, 1, 0, &time_for_read);
		if (0 == status)
			more_data = FALSE;
		else if (0 < status)
		{
			terminator = FALSE;
			*(stringpool.free + i) = inchar;
			i += status;
			/* ------- check to see if it is a delimiter -------- */
			for (ii = 0; term_mode && (ii < socketptr->n_delimiter); ii++)
			{
				if (i < socketptr->delimiter[ii].len)
					continue;
				if (0 == memcmp(socketptr->delimiter[ii].addr,
						stringpool.free + i - socketptr->delimiter[ii].len,
						socketptr->delimiter[ii].len))
				{
					terminator = TRUE;
					i -= socketptr->delimiter[ii].len;
					memcpy(iod->dollar.zb, socketptr->delimiter[ii].addr,
						MIN(socketptr->delimiter[ii].len, ESC_LEN - 1));
					iod->dollar.zb[MIN(socketptr->delimiter[ii].len, ESC_LEN - 1)] = '\0';
					memcpy(dsocketptr->dollar_key, socketptr->delimiter[ii].addr,
						MIN(socketptr->delimiter[ii].len, DD_BUFLEN - 1));
					dsocketptr->dollar_key[MIN(socketptr->delimiter[ii].len, DD_BUFLEN - 1)] = '\0';
					break;
				}
			}
			if (!terminator)
				more_data = TRUE;
		} else if (EINTR == errno && !out_of_time)	/* unrelated timer popped */
		{
			status = 0;
			continue;
		} else
		{
			real_errno = errno;
			break;
		}
		if (OUTOFBANDNOW(outofband) || (status > 0 && terminator))
			break;
		if (timed)
		{
			if (msec_timeout > 0)
			{
				sys_get_curr_time(&cur_time);
				cur_time = sub_abs_time(&end_time, &cur_time);
				if (0 > cur_time.at_sec)
				{
					out_of_time = TRUE;
					cancel_timer(timer_id);
					break;
				}
			} else if (!more_data)
				break;
		}
		if (vari)
		{
			if (!term_mode && 0 != i && !more_data)
				break;
		} else if (i >= width)
			break;
	}
	if (EINTR == real_errno)
		status = 0;	/* don't treat a <CTRL-C> or timeout as an error */
	if (timed)
	{
		if (0 < msec_timeout)
		{
#ifdef UNIX
			FCNTL3(socketptr->sd, F_SETFL, flags, fcntl_res);
			if (fcntl_res < 0)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("F_SETFL FOR RESTORING SOCKET OPTIONS"),
					  	save_errno, LEN_AND_STR(errptr));
			}
#endif
			if (out_of_time)
				ret = FALSE;
			else
				cancel_timer(timer_id);
		} else if ((i < width) && !(term_mode && terminator))
			ret = FALSE;
	}
	if (i > 0)
	{	/* there's somthing to return */
		v->str.len = i;
		v->str.addr = (char *)stringpool.free;
		if (((iod->dollar.x += i) >= iod->width) && iod->wrap)
		{
			iod->dollar.y += (iod->dollar.x / iod->width);
			if (0 != iod->length)
				iod->dollar.y %= iod->length;
			iod->dollar.x %= iod->width;
		}
	} else
	{
		v->str.len = 0;
		v->str.addr = dsocketptr->dollar_key;
	}
	len = sizeof("1,") - 1;
	if (status >= 0)
	{	/* no real problems */
		iod->dollar.zeof = FALSE;
		iod->dollar.za = 0;
		memcpy(dsocketptr->dollar_device, "0", sizeof("0"));
	} else
	{	/* there's a significant problem */
		if (0 == i)
			iod->dollar.x = 0;
		iod->dollar.za = 9;
		memcpy(dsocketptr->dollar_device, "1,", len);
		errptr = (char *)STRERROR(real_errno);
		errlen = strlen(errptr);
		memcpy(&dsocketptr->dollar_device[len], errptr, errlen);
		if (iod->dollar.zeof || -1 == status || 0 < iod->error_handler.len)
		{
			iod->dollar.zeof = TRUE;
			if (socketptr->ioerror)
				rts_error(VARLSTCNT(6) ERR_IOEOF, 0, ERR_TEXT, 2, errlen, errptr);
		} else
			iod->dollar.zeof = TRUE;
	}
	return ((short)ret);
}
