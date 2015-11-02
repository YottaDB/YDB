/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
 * 		-- got some stuff to return 		return number of bytes received
 * 		-- got nothing and timed out		return 0
 * 		-- loss-of-connection			return -2
 * 		-- error condition			return -1, with errno set
 *	side note:
 * 		-- use our own buffer if the requested size is smaller than our own buffer size
 * 		-- use the caller's buffer if the requested size is bigger than our own buffer
 * 	control flow:
 * 		-- if we already have some leftover, use it and figure out how much is still needed
 * 		-- if there is still need to read, figure out whether to use our buffer or the passed-in buffer
 * 		-- select so that this operation is timed
 * 		-- if select returns positive, recv, otherwise, return timeout
 * 		-- if recv gets nothing, return lost-of-connection
 * 		-- if the device buffer is used, move it over to the return buffer and update the device buffer pointer
 */
#include "mdef.h"
#include <sys/types.h>
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_inet.h"
#include "gtm_string.h"
#ifdef UNIX
#include "gtm_fcntl.h"
#include "eintr_wrappers.h"
static int fcntl_res;
#ifdef DEBUG
#include <sys/time.h>		/* for gettimeofday */
#endif
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
#include <sys/poll.h>
#endif
#endif
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "stringpool.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "gtm_utf8.h"
#include "outofband.h"

/* MAX_SNR_IO is for read loop in iosocket_snr_utf_prebuffer(). It is possible for a series of interrupts (one
 * from each active region) to interfere with this read so be generous here.
 */
#define MAX_SNR_IO		50

#ifdef DEBUG
/* Hold gettimeofday before and after select to debug AIX spin */
static	struct timeval tvbefore, tvafter;
#endif

GBLREF	io_pair 		io_curr_device;
GBLREF	bool			out_of_time;
GBLREF	spdesc 			stringpool;
GBLREF	tcp_library_struct	tcp_routines;
GBLREF	int4			outofband;

/* Local routine we aren't making static due to increased debugging difficult static routines make */
ssize_t iosocket_snr_io(socket_struct *socketptr, void *buffer, size_t maxlength, int flags, ABS_TIME *time_for_read);

/* Select aNd Receive */
ssize_t iosocket_snr(socket_struct *socketptr, void *buffer, size_t maxlength, int flags, ABS_TIME *time_for_read)
{
	int		status;
	ssize_t		bytesread, recvsize;
	void		*recvbuff;

	DBGSOCK((stdout, "socsnr: socketptr: 0x"lvaddr"  buffer: 0x"lvaddr"  maxlength: %d,  flags: %d\n",
		 socketptr, buffer, maxlength, flags));
	/* Use leftover from the previous read, if there is any */
	assert(0 < maxlength);
	if (0 < socketptr->buffered_length)
	{
		DBGSOCK2((stdout, "socsnr: read from buffer - buffered_length: %d\n", socketptr->buffered_length));
		bytesread = MIN(socketptr->buffered_length, maxlength);
		memcpy(buffer, (void *)(socketptr->buffer + socketptr->buffered_offset), bytesread);
		socketptr->buffered_offset += bytesread;
		socketptr->buffered_length -= bytesread;
		DBGSOCK2((stdout, "socsnr: after buffer read - buffered_offset: %d  buffered_length: %d\n",
			  socketptr->buffered_offset, socketptr->buffered_length));
		return bytesread;
	}
	/* Decide on which buffer to use and the size of the recv */
	if (socketptr->buffer_size > maxlength)
	{
		recvbuff = socketptr->buffer;
		recvsize = socketptr->buffer_size;
	} else
	{
		recvbuff = buffer;
		recvsize = maxlength;
	}
	VMS_ONLY(recvsize = MIN(recvsize, VMS_MAX_TCP_IO_SIZE));
	DBGSOCK2((stdout, "socsnr: recvsize set to %d\n", recvsize));

	/* Select and recv */
	assert(0 == socketptr->buffered_length);
	socketptr->buffered_length = 0;
	bytesread = (int)iosocket_snr_io(socketptr, recvbuff, recvsize, flags, time_for_read);
	DBGSOCK2((stdout, "socsnr: bytes read from recv: %d  timeout: %d\n", bytesread, out_of_time));
	if (0 < bytesread)
	{	/* Got something this time */
		if (recvbuff == socketptr->buffer)
		{
			if (bytesread <= maxlength)
				memcpy(buffer, socketptr->buffer, bytesread);
			else
			{	/* Got some stuff for future recv */
				memcpy(buffer, socketptr->buffer, maxlength);
				socketptr->buffered_length = bytesread - maxlength;
				bytesread = socketptr->buffered_offset = maxlength;
				DBGSOCK2((stdout, "socsnr: Buffer updated post read - buffered_offset: %d  "
					  "buffered_length: %d\n", socketptr->buffered_offset, socketptr->buffered_length));
			}
		}
	}
	return bytesread;
}

/* Do the IO dirty work. Note the return value can be from either select() or recv().
 * This would be a static routine but that just makes it harder to debug.
 */
ssize_t iosocket_snr_io(socket_struct *socketptr, void *buffer, size_t maxlength, int flags, ABS_TIME *time_for_read)
{
	int		status, bytesread, real_errno;
	fd_set		tcp_fd;
	ABS_TIME	lcl_time_for_read;
#	ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
	long		poll_timeout;
	unsigned long	poll_nfds;
	struct pollfd	poll_fdlist[1];
#	endif

	DBGSOCK2((stdout, "socsnrio: Socket read request - socketptr: 0x"lvaddr"  buffer: 0x"lvaddr"  maxlength: %d  flags: %d  ",
		  socketptr, buffer, maxlength, flags));
	DBGSOCK2((stdout, "time_for_read->at_sec: %d  usec: %d\n", time_for_read->at_sec, time_for_read->at_usec));
	DEBUG_ONLY(gettimeofday(&tvbefore, NULL);)
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
        FD_ZERO(&tcp_fd);
        FD_SET(socketptr->sd, &tcp_fd);
        assert(0 != FD_ISSET(socketptr->sd, &tcp_fd));
        lcl_time_for_read = *time_for_read;
        status = tcp_routines.aa_select(socketptr->sd + 1, (void *)(&tcp_fd), (void *)0, (void *)0, &lcl_time_for_read);
#else
	poll_fdlist[0].fd = socketptr->sd;
	poll_fdlist[0].events = POLLIN;
	poll_nfds = 1;
	poll_timeout = time_for_read->at_usec / 1000;	/* convert to millisecs */
	status = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
#endif
	real_errno = errno;
	DEBUG_ONLY(gettimeofday(&tvafter, NULL);)
	DBGSOCK2((stdout, "socsnrio: Select return code: %d :: errno: %d\n", status, real_errno));
        if (0 < status)
	{
                bytesread = tcp_routines.aa_recv(socketptr->sd, buffer, maxlength, flags);
		real_errno = errno;
		socketptr->last_recv_errno = (-1 != status) ? 0 : real_errno;	/* Save status of last recv for dbging purposes */
		DBGSOCK2((stdout, "socsnrio: aa_recv return code: %d :: errno: %d\n", bytesread, errno));
		if ((0 == bytesread) ||
		    ((-1 == bytesread) && ((ECONNRESET == real_errno) || (EPIPE == real_errno) || (EINVAL == real_errno))))
                {       /* Lost connection */
                        if (0 == bytesread)
                                errno = ECONNRESET;
                        return (ssize_t)(-2);
                }
		DBGSOCK_ONLY2(errno = real_errno);
                return bytesread;
	}
	DBGSOCK_ONLY2(errno = real_errno);
	return (ssize_t)status;
}

/* When scanning for delimiters, we have to make sure that the next read can pull in at least one full utf char.
 * Failure to do this means that if a partial utf8 char is read, it will be rebuffered, reread, rebuffered, forever.
 * A return code of zero indicates a timeout error occured. A negative return code indicates an IO error of some sort.
 * A positive return code is the length in bytes of the next unicode char in the buffer.
 */
ssize_t iosocket_snr_utf_prebuffer(io_desc *iod, socket_struct *socketptr, int flags, ABS_TIME *time_for_read,
				   boolean_t wait_for_input)
{
	int	mblen, bytesread, real_errno;
	ssize_t  readlen;
	char	*readptr;

	assert(CHSET_M != iod->ichset);
	DBGSOCK((stdout, "socsnrupb: Enter prebuffer: buffered_length: %d  wait_for_input: %d\n",
		 socketptr->buffered_length, wait_for_input));
	/* See if there is *anything* in the buffer */
	if (0 == socketptr->buffered_length)
	{	/* Buffer is empty, read at least one char into it so we can check how many we need */
		do
		{
			bytesread = (int)iosocket_snr_io(socketptr, socketptr->buffer, socketptr->buffer_size, flags,
							 time_for_read);
			DBGSOCK_ONLY2(real_errno = errno);
			DBGSOCK2((stdout, "socsnrupb: Buffer empty - bytes read: %d  errno: %d\n", bytesread, real_errno));
			DBGSOCK_ONLY2(errno = real_errno);
		} while ((((-1 == bytesread) && (EINTR == errno)) || (0 == bytesread && wait_for_input))
			 && !out_of_time && (0 == outofband));
		if (out_of_time || (0 != outofband))
		{
			DBGSOCK_ONLY(if (out_of_time)
				      {
					      DBGSOCK((stdout, "socsnrupb: Returning due to timeout\n"));
				      } else
				      {
					      DBGSOCK((stdout, "socsnrupb: Returning due to outofband\n"));
				      }
			);
			if (0 < bytesread)
			{	/* If we read anything, be sure to consider it buffered */
				socketptr->buffered_length = bytesread;
				socketptr->buffered_offset = 0;
			}
			return 0;
		}
		if (0 >= bytesread)
		{
			DBGSOCK_ONLY2(real_errno = errno);
			DBGSOCK2((stdout, "socsnrupb: Returning due to error code %d  errno: %d\n", bytesread, real_errno));
			DBGSOCK_ONLY2(errno = real_errno);
			return bytesread;
		}
		socketptr->buffered_length = bytesread;
		socketptr->buffered_offset = 0;
	}
	/* Compute number of bytes we need for the first char in the buffer */
	readptr = socketptr->buffer + socketptr->buffered_offset;
	switch(iod->ichset)
	{
		case CHSET_UTF8:
			mblen = UTF8_MBFOLLOW(readptr);
			if (0 > mblen)
				mblen = 0;	/* Invalid char, just assume one char needed */
			break;
		case CHSET_UTF16BE:
			mblen = UTF16BE_MBFOLLOW(readptr, readptr + socketptr->buffered_length);
			if (0 > mblen)
				mblen = 1;	/* If buffer is too small we will get -1 here. Assume need 2 chars */
			break;
		case CHSET_UTF16LE:
			mblen = UTF16LE_MBFOLLOW(readptr, readptr + socketptr->buffered_length);
                        if (0 > mblen)
                                mblen = 1;      /* If buffer is too small we will get -1 here. Assume need 2 chars */
			break;
		case CHSET_UTF16:
			/* Special case as we don't know which mode we are in. This should only be used when
			 * checking for BOMs. Check if first char is 0xFF or 0xFE. If it is, return 1 as our
			 * (follow) length. If neither, assume UTF16BE (default UTF16 codeset) and return the
			 * length it gives.
			 */
			if ((0xFF == (unsigned char)*readptr) || (0xFE == (unsigned char)*readptr))
				mblen = 1;
			else
			{
				mblen = UTF16BE_MBFOLLOW(readptr, readptr + socketptr->buffered_length);
				if (0 > mblen)
					mblen = 1;      /* If buffer is too small we will get -1 here. Assume need 2 chars */
			}
			break;
		default:
			GTMASSERT;
	}
	mblen++;	/* Include first char we were looking at in the required byte length */
	DBGSOCK2((stdout, "socsnrupb: Length of char: %d\n", mblen));
	if (socketptr->buffered_length < mblen)
	{	/* Still insufficient chars in the buffer for our utf character. Read some more in. */
		if ((socketptr->buffered_offset + mblen) > socketptr->buffer_size)
		{	/* Our char won't fit in the buffer. This can only occur if the read point is
			 * right at the end of the buffer since the minimum buffer size is 32K. Solution
			 * is to slide the part of the char that we have down to the beginning of the
			 * buffer so we have plenty of room. Since this is at most 3 bytes, this is not
			 * a major performance concern.
			 */
			DBGSOCK2((stdout, "socsnrupb: Char won't fit in buffer, slide it down\n"));
			assert(SIZEOF(int) > socketptr->buffered_length);
			assert(socketptr->buffered_offset > socketptr->buffered_length); /* Assert no overlap */
			memcpy(socketptr->buffer, (socketptr->buffer + socketptr->buffered_offset), socketptr->buffered_length);
			socketptr->buffered_offset = 0;
		}
		while (socketptr->buffered_length < mblen)
		{
			DBGSOCK2((stdout, "socsnrupb: Top of read loop for char - buffered_length: %d\n",
				  socketptr->buffered_length));
			readptr = socketptr->buffer + socketptr->buffered_offset + socketptr->buffered_length;
			readlen = socketptr->buffer_size - socketptr->buffered_offset - socketptr->buffered_length;
			assert(0 < readlen);
			bytesread = (int)iosocket_snr_io(socketptr, readptr, readlen, flags, time_for_read);
			DBGSOCK2((stdout, "socsnrupb: Read %d chars\n", bytesread));
			if (0 > bytesread)
			{       /* Some error occurred. Check for restartable condition. */
				if (EINTR == errno)
					if (!out_of_time)
						continue;
					else
						return 0;	/* timeout indicator */
				return bytesread;
			}
			if (out_of_time)
				return 0;
			socketptr->buffered_length += bytesread;
		}
	}
	DBGSOCK((stdout, "socsnrupb: Returning char length %d -- buffered_length: %d\n", mblen, socketptr->buffered_length));
	return mblen;
}

/* Place len bytes pointed by buffer back into socketptr's internal buffer
 *
 * Side effect: suppose the last snr was with a length > internal buffer size, we would not have used the internal buffer. For
 * that case, unsnr might move data not in the internal buffer into the internal buffer and also might result in buffer
 * expansion
 */
void iosocket_unsnr(socket_struct *socketptr, unsigned char *buffer, size_t len)
{
	char	*new_buff;

	DBGSOCK((stdout, "iosunsnr: ** Requeueing %d bytes\n", len));
	if ((socketptr->buffered_length + len) <= socketptr->buffer_size)
	{
		if (0 < socketptr->buffered_length)
		{
			if (socketptr->buffered_offset < len)
			{
				memmove(socketptr->buffer + len, socketptr->buffer + socketptr->buffered_offset,
						socketptr->buffered_length);
				memmove(socketptr->buffer, buffer, len);
			} else
			{
				memmove(socketptr->buffer, buffer, len);
				memmove(socketptr->buffer + len, socketptr->buffer + socketptr->buffered_offset,
						socketptr->buffered_length);
			}
		} else
			memmove(socketptr->buffer, buffer, len);
	} else
	{
		new_buff = malloc(socketptr->buffered_length + len);
		memcpy(new_buff, buffer, len);
		if (0 < socketptr->buffered_length)
			memcpy(new_buff + len, socketptr->buffer + socketptr->buffered_offset, socketptr->buffered_length);
		free(socketptr->buffer);
		socketptr->buffer = new_buff;
	}
	socketptr->buffered_offset = 0;
	socketptr->buffered_length += len;
	return;
}
