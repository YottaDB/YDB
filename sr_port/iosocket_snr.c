/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
 * 		-- time_for_read	pointer to the timeout structure used by poll()
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
static int fcntl_res;
#ifdef DEBUG
#include <sys/time.h>		/* for gettimeofday */
#endif
<<<<<<< HEAD
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
#include "gtm_poll.h"
#endif
#endif
#include "gtm_select.h"

=======
#endif
#include "gtm_poll.h"
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
#include "eintr_wrappers.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "stringpool.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "gtm_utf8.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#ifdef GTM_TLS
#include "gtm_tls.h"
#endif

/* MAX_SNR_IO is for read loop in iosocket_snr_utf_prebuffer(). It is possible for a series of interrupts (one
 * from each active region) to interfere with this read so be generous here.
 */
#define MAX_SNR_IO		50

#ifdef DEBUG
/* Hold gettimeofday before and after select to debug AIX spin */
static	struct timespec tsbefore, tsafter;
#endif

GBLREF	bool			out_of_time;
GBLREF	io_pair 		io_curr_device;
GBLREF	spdesc 			stringpool;
GBLREF	volatile int4		outofband;
#ifdef GTM_TLS
GBLREF	gtm_tls_ctx_t		*tls_ctx;
#endif

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
	int		status, real_errno;
	ssize_t		bytesread;
	boolean_t	pollread;
	int		poll_timeout;
	nfds_t		poll_nfds;
	struct pollfd	poll_fdlist[1];
#	ifdef GTM_TLS
	int		tlspolldirection = 0;
#	endif

	DBGSOCK2((stdout, "socsnrio: Socket read request - socketptr: 0x"lvaddr"  buffer: 0x"lvaddr"  maxlength: %d  flags: %d  ",
		  socketptr, buffer, maxlength, flags));
	DBGSOCK2((stdout, "time_for_read->tv_sec: %d  usec: %d\n", time_for_read->tv_sec, time_for_read->tv_nsec / NANOSECS_IN_USEC));
	DEBUG_ONLY(clock_gettime(CLOCK_REALTIME, &tsbefore);)
	while (TRUE)
	{
		status = 0;
#		ifdef GTM_TLS
		if (socketptr->tlsenabled)
		{
			pollread = (tlspolldirection == GTMTLS_WANT_WRITE) ? FALSE : TRUE;
			status = gtm_tls_cachedbytes((gtm_tls_socket_t *)socketptr->tlssocket);
		} else
#		endif
			pollread = TRUE;
		if (0 == status)
		{	/* if TLS cachedbytes available no need to poll */
<<<<<<< HEAD
#			ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
			FD_SET(socketptr->sd, &tcp_fd);
			assert(0 != FD_ISSET(socketptr->sd, &tcp_fd));
			lcl_time_for_read.tv_sec = time_for_read->tv_sec;
			lcl_time_for_read.tv_usec = (gtm_tv_usec_t)(time_for_read->tv_nsec / NANOSECS_IN_USEC);
			if (pollread)
			{
				readfds = &tcp_fd;
				writefds = NULL;
			} else
			{
				writefds = &tcp_fd;
				readfds = NULL;
			}
			status = select(socketptr->sd + 1, readfds, writefds, NULL, &lcl_time_for_read);
#			else
			poll_fdlist[0].fd = socketptr->sd;
			poll_fdlist[0].events = pollread ? POLLIN : POLLOUT;
			poll_nfds = 1;
			poll_timeout = DIVIDE_ROUND_UP(time_for_read->tv_nsec, NANOSECS_IN_MSEC);	/* convert to millisecs */
=======
			poll_fdlist[0].fd = socketptr->sd;
			poll_fdlist[0].events = pollread ? POLLIN : POLLOUT;
			poll_nfds = 1;
			assert(time_for_read->at_sec == 0);
			poll_timeout = DIVIDE_ROUND_UP(time_for_read->at_usec, MICROSECS_IN_MSEC);	/* convert to millisecs */
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
			status = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
			real_errno = errno;
			DEBUG_ONLY(clock_gettime(CLOCK_REALTIME, &tsafter);)
			DBGSOCK2((stdout, "socsnrio: Select return code: %d :: errno: %d\n", status, real_errno));
		}
		if (0 < status)
		{
#			ifdef GTM_TLS
			if (socketptr->tlsenabled)
			{
				bytesread = gtm_tls_recv((gtm_tls_socket_t *)socketptr->tlssocket, buffer, maxlength);
				DBGSOCK2((stdout, "socsnrio: gtm_tls_recv: %d :: errno: %d\n", bytesread, errno));
				if (0 < bytesread)
					return bytesread;
				/* if want read or write, need to loop */
				/* after setting tlspolldirection */
				DBGSOCK2((stdout, "socsnrio: TLS errno %d - %s\n", gtm_tls_errno(),
							gtm_tls_get_error((gtm_tls_socket_t *)socketptr->tlssocket)));
				switch (bytesread)
				{
					case GTMTLS_WANT_READ:
						tlspolldirection = GTMTLS_WANT_READ;
						break;
					case GTMTLS_WANT_WRITE:
						tlspolldirection = GTMTLS_WANT_WRITE;
						break;
					default:
						socketptr->last_recv_errno = errno = gtm_tls_errno();
						if (ECONNRESET == errno)
						{
							return (ssize_t)(-2);
						} else
							return (ssize_t)(-1);
				}
				continue;
			} else
#			endif
			{
				RECV(socketptr->sd, buffer, maxlength, flags, bytesread);
				real_errno = errno;
				socketptr->last_recv_errno = (-1 != status) ? 0 : real_errno;	/* Save status for dbg purposes */
				DBGSOCK2((stdout, "socsnrio: aa_recv return code: %d :: errno: %d\n", bytesread, errno));
				if ((0 == bytesread) || ((-1 == bytesread) && ((ECONNRESET == real_errno) || (EPIPE == real_errno)
					|| (EINVAL == real_errno))))
				{	/* Lost connection */
					if (0 == bytesread)
						errno = ECONNRESET;
					return (ssize_t)(-2);
				}
				DBGSOCK_ONLY2(errno = real_errno);
				return bytesread;
			}
		} else
			break;		/* nothing ready */
	}
	DBGSOCK_ONLY2(errno = real_errno);
	return (ssize_t)status;
}

/* When scanning for delimiters, we have to make sure that the next read can pull in at least one full utf char.
 * Failure to do this means that if a partial utf8 char is read, it will be rebuffered, reread, rebuffered, forever.
 * A return code of zero indicates a timeout error occured. A negative return code indicates an IO error of some sort.
 * A positive return code is the length in bytes of the next utf char in the buffer.
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
			/* Note: Both "eintr_handling_check()" and "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" ensure "errno"
			 * is untouched so it is safe to use "errno" after the call to examine the pre-call value.
			 */
			if ((-1 == bytesread) && (EINTR == errno))
				eintr_handling_check();
			else
				HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
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
				mblen = 1;	/* If buffer is too small we will get -1 here. Assume need 2 chars */
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
					mblen = 1;	/* If buffer is too small we will get -1 here. Assume need 2 chars */
			}
			break;
		default:
			assertpro(iod->ichset != iod->ichset);
			mblen = 0;	/* needed to silence [-Wsometimes-uninitialized] warning from CLang/LLVM */
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
			{	/* Some error occurred. Check for restartable condition. */
				if (EINTR == errno)
				{
					eintr_handling_check();
					if (!out_of_time)
						continue;
					else
						return 0;	/* timeout indicator */
				} else
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
				return bytesread;
			}
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
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
				assert((len + socketptr->buffered_length) < socketptr->buffer_size);
				memmove(socketptr->buffer + len, socketptr->buffer + socketptr->buffered_offset,
						socketptr->buffered_length);
				memmove(socketptr->buffer, buffer, len);
			} else
			{
				memmove(socketptr->buffer, buffer, len);
				assert((len + socketptr->buffered_length) < socketptr->buffer_size);
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
