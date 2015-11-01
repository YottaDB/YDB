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

/* iosocket_snr.c
 * 	description:
 * 		-- takes care of the buffering of the recv for the socket device (and possible tcp device)
 * 	parameters:
 * 		-- socketptr		pointer to a socket device, where we get socket descriptor, buffer and offset in buffer
 * 		-- buffer		pointer to the buffer where to return stuff
 * 		-- maxlength		maximum number of bytes to get
 * 		-- flags		flags to be passed to recv()
 * 		-- time_for_read	pointer to the timeout structure used by select()
 * 		-- extra_status		reports either a timeout or a lost-of-connection
 * 	return:
 * 		-- got some stuff to return 					return number of bytes received
 * 		-- got nothing and timed out					return 0
 * 		-- lost-of-connection						return -2
 * 		-- error condition						return -1, with errno set
 *	side note:
 * 		-- use our own buffer if the requested size is smaller than our own buffer size
 * 		-- use the caller's buffer if the requested size is bigger than our own buffer
 * 	control flow:
 * 		-- if we already have some leftover, use it and figure out how much is still needed
 * 		-- if there is still need to read, figure out whether to use our buffer or the passed-in buffer
 * 		-- select so that the this operation is timed
 * 		-- if select returns positive, recv, otherwise, return timeout
 * 		-- if recv gets nothing, return lost-of-connection
 * 		-- if the device buffer is used, move it over to the return buffer and update the device buffer pointer
 */
#include "mdef.h"
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#ifdef __MVS__
#include <sys/time.h>
#endif
#include <netinet/in.h>
#include "gtm_string.h"
#ifdef UNIX
#include <fcntl.h>
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
#define	TIMEOUT_INTERVAL	200000
GBLREF	io_pair 		io_curr_device;
GBLREF	bool			out_of_time;
GBLREF	spdesc 			stringpool;
GBLREF	tcp_library_struct	tcp_routines;
GBLREF	int4			outofband;
ssize_t iosocket_snr(socket_struct *socketptr, void *buffer, size_t maxlength, int flags, ABS_TIME *time_for_read)
{
	fd_set		tcp_fd;
	ABS_TIME	lcl_time_for_read;
	int		status;
	ssize_t		bytesread, recvsize;
	void		*recvbuff;
	/* ====================== use leftover from the previous read, if there is any ========================= */
	assert(maxlength > 0);
	if (socketptr->buffered_length > 0)
	{
		bytesread = MIN(socketptr->buffered_length, maxlength);
		memcpy(buffer, (void *)(socketptr->buffer + socketptr->buffered_offset), bytesread);
		socketptr->buffered_offset += bytesread;
		socketptr->buffered_length -= bytesread;
		return bytesread;
	}
	/* ===================== decide on which buffer to use and the size of the recv ======================== */
	if (socketptr->buffer_size > maxlength)
	{
		recvbuff = socketptr->buffer;
		recvsize = socketptr->buffer_size;
	}
	else
	{
		recvbuff = buffer;
		recvsize = maxlength;
	}
	/* =================================== select and recv ================================================= */
	socketptr->buffered_length = 0;
	FD_ZERO(&tcp_fd);
	FD_SET(socketptr->sd, &tcp_fd);
	assert(0 != FD_ISSET(socketptr->sd, &tcp_fd));
	lcl_time_for_read = *time_for_read;
	status = tcp_routines.aa_select(socketptr->sd + 1, (void *)(&tcp_fd), (void *)0, (void *)0, &lcl_time_for_read);
	if (0 < status)
	{
		bytesread = tcp_routines.aa_recv(socketptr->sd, recvbuff, recvsize, flags);
		if (0 < bytesread)
		{
			/* -------- got something this time ----------- */
			if (recvbuff == socketptr->buffer)
			{
				if (bytesread <= maxlength)
					memcpy(buffer, socketptr->buffer, bytesread);
				else
				{
					/* -------- got some stuff for future recv -------- */
					memcpy(buffer, socketptr->buffer, maxlength);
					socketptr->buffered_length = bytesread - maxlength;
					bytesread = socketptr->buffered_offset = maxlength;
				}
			}
		} else if ((0 == bytesread) || ((-1 == bytesread) && (ECONNRESET == errno || EPIPE == errno || EINVAL == errno)))
		{ /* ----- lost connection ------- */
			if (0 == bytesread)
				errno = ECONNRESET;
			return (ssize_t)(-2);
		}
		return bytesread;
	}
	return (ssize_t)(status);
}
