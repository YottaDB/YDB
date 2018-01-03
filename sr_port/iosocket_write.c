/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_write.c */

#include "mdef.h"

#include <errno.h>
#ifdef USE_POLL
#include "gtm_poll.h"
#else
#include "gtm_select.h"
#endif
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "eintr_wrappers.h"

#include "io.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "dollarx.h"
#include "gtm_conv.h"
#include "gtm_utf8.h"
#include "min_max.h"
#include "stringpool.h"
#include "send_msg.h"
#include "error.h"
#include "rel_quant.h"
#ifdef GTM_TLS
#include "gtm_tls.h"
#endif

GBLREF io_pair			io_curr_device;
#ifdef UNIX
GBLREF io_pair			io_std_device;
GBLREF bool			prin_out_dev_failure;
#endif

GBLREF mstr			chset_names[];
GBLREF UConverter		*chset_desc[];
GBLREF spdesc			stringpool;
#ifdef GTM_TLS
GBLREF	gtm_tls_ctx_t		*tls_ctx;
#endif

error_def(ERR_CURRSOCKOFR);
error_def(ERR_DELIMSIZNA);
UNIX_ONLY(error_def(ERR_NOPRINCIO);)
error_def(ERR_NOSOCKETINDEV);
error_def(ERR_SOCKPASSDATAMIX);
error_def(ERR_SOCKWRITE);
error_def(ERR_TEXT);
UNIX_ONLY(error_def(ERR_TLSIOERROR);)
error_def(ERR_ZFF2MANY);
error_def(ERR_ZINTRECURSEIO);

#define DOTCPSEND_REAL(SOCKETPTR, SBUFF, SBUFF_LEN, SFLAGS, RC)								\
{															\
	ssize_t		gtmioStatus;											\
	size_t		gtmioBuffLen;											\
	size_t		gtmioChunk;											\
	sm_uc_ptr_t	gtmioBuff;											\
															\
	gtmioBuffLen = SBUFF_LEN;											\
	gtmioBuff = (sm_uc_ptr_t)(SBUFF);										\
	for (;;)													\
        {														\
		gtmioChunk = gtmioBuffLen VMS_ONLY(> VMS_MAX_TCP_IO_SIZE ? VMS_MAX_TCP_IO_SIZE : gtmioBuffLen);		\
		SEND((SOCKETPTR)->sd, gtmioBuff, gtmioChunk, SFLAGS, gtmioStatus);					\
		if ((ssize_t)-1 != gtmioStatus)										\
	        {													\
			gtmioBuffLen -= gtmioStatus;									\
			if (0 == gtmioBuffLen)										\
				break;											\
			gtmioBuff += gtmioStatus;									\
	        }													\
		else													\
			break;												\
        }														\
	if ((ssize_t)-1 == gtmioStatus)    	/* Had legitimate error - return it */					\
		RC = errno;												\
	else if (0 == gtmioBuffLen)											\
	        RC = 0;													\
	else														\
		RC = -1;		/* Something kept us from sending what we wanted */				\
}

#define DOTCPSEND(SOCKETPTR, SBUFF, SBUFF_LEN, SFLAGS, RC)								\
{															\
	ssize_t	localstatus;													\
	if (0 == (SOCKETPTR)->obuffer_size)										\
		DOTCPSEND_REAL(SOCKETPTR, SBUFF, SBUFF_LEN, SFLAGS, RC)							\
	else														\
	{														\
		localstatus = iosocket_write_buffered(SOCKETPTR, SBUFF, SBUFF_LEN);						\
		if (SBUFF_LEN == localstatus)										\
			RC = 0;												\
		else													\
			RC = -1;											\
	}														\
}

void	iosocket_write(mstr *v)
{
	iosocket_write_real(v, TRUE);
}

void iosocket_buffer_error(socket_struct *socketptr)
{	/* output error from obuffer_errno */
	int		errlen, devlen, save_obuffer_errno;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;
	const char	*errptr;

	if (0 == socketptr->obuffer_errno)
		return;			/* no error */
	dsocketptr = socketptr->dev;
	iod = dsocketptr->iod;
#	ifdef GTM_TLS
	if (socketptr->tlsenabled)
	{
		iod->dollar.za = 9;
		if (-1 == socketptr->obuffer_errno)
			errptr = gtm_tls_get_error();
		else
			errptr = (char *)STRERROR(socketptr->obuffer_errno);
		SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errptr);
		socketptr->obuffer_errno = 0;
		if (socketptr->ioerror)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TLSIOERROR, 2, LEN_AND_LIT("send"),
				ERR_TEXT, 2, STRLEN(errptr), errptr);
	} else
#	endif
	{
		save_obuffer_errno = socketptr->obuffer_errno;
		socketptr->obuffer_errno = 0;
		SOCKERROR(iod, socketptr, ERR_SOCKWRITE, save_obuffer_errno);
	}
}

ssize_t iosocket_output(socket_struct *socketptr, char *buffer, size_t length, boolean_t resetbuffer, boolean_t timed);

ssize_t iosocket_output(socket_struct *socketptr, char *buffer, size_t length, boolean_t resetbuffer, boolean_t timed)
{
	boolean_t	pollwrite;
	ssize_t		status;
	size_t		llen;
	int		bytessent, istatus, timeout, save_errno;
	char		*lbuffer;
#	ifdef GTM_TLS
	int		tlspolldirection = 0;
#	endif
#	ifdef USE_POLL
	struct pollfd	fds;
#	else
	fd_set		fds, *readfds, *writefds;
	struct timeval	timeout_spec;
#	endif

	if (!socketptr->obuffer_output_active)
		return 0;		/* how did we get here */
	if (timed)
	{
		if (0 != socketptr->obuffer_errno)
			return -1;	/* unprocessed error */
		timeout = 0;		/* no waiting in poll */
	} else
		timeout = socketptr->obuffer_wait_time;
#	ifndef USE_POLL
	FD_ZERO(&fds);
	FD_SET(socketptr->sd, &fds);
	timeout = timeout * 1000;	/* convert milli to micro seconds */
	timeout_spec.tv_sec = 0;
	timeout_spec.tv_usec = timeout;
#	endif
	llen = length;
	status = 0;
	lbuffer = buffer;
	while (0 < llen)
	{	/* poll/select tlspolldirection - needed if noblocking */
#		ifdef GTM_TLS
		if (socketptr->tlsenabled)
			pollwrite = (tlspolldirection == GTMTLS_WANT_READ) ? FALSE : TRUE;
		else
#		endif
			pollwrite = TRUE;
#		ifdef USE_POLL
		fds.fd = socketptr->sd;
		fds.events = pollwrite ? POLLOUT : POLLIN;
		istatus = poll(&fds, 1, timeout);
#		else
		if (pollwrite)
		{
			writefds = &fds;
			readfds = NULL;
		} else
		{
			readfds = &fds;
			writefds = NULL;
		}
		istatus = select(socketptr->sd + 1, readfds, writefds, NULL, &timeout_spec);
#		endif
		if (-1 == istatus)
		{
			save_errno = errno;
			if (timed)
			{	/* called from timer so only try once */
				socketptr->obuffer_errno = save_errno;
				status = -1;
				break;
			}
			if (EAGAIN == save_errno)
				rel_quant();	/* seems like a legitimate rel_quant */
			else if (EINTR != save_errno)
			{
				status = -1;
				break;
			}
#			ifndef USE_POLL
			timeout_spec.tv_usec = timeout;
			FD_SET(socketptr->sd, &fds);
#			endif
			continue;
		}
#		ifdef GTM_TLS
		if (socketptr->tlsenabled)
		{
			bytessent = gtm_tls_send((gtm_tls_socket_t *)socketptr->tlssocket, lbuffer, llen);
			if (0 < bytessent)
			{	/* unless partial writes enabled either none or all should have been written */
				llen -= bytessent;
				lbuffer += bytessent;
				tlspolldirection = 0;
			} else
			{
				switch (bytessent)
				{
					case GTMTLS_WANT_READ:
						tlspolldirection = GTMTLS_WANT_READ;
						break;
					case GTMTLS_WANT_WRITE:
						tlspolldirection = GTMTLS_WANT_WRITE;
						break;
					default:
						assert(-1 == bytessent);
						socketptr->obuffer_errno = gtm_tls_errno();
						if (ECONNRESET == socketptr->obuffer_errno)
						{
							return (ssize_t)(-2);
						} else
							return (ssize_t)(-1);
				}
			}
			if (0 == llen)
				status = length;
			else
				status = 0;
		} else
#		endif
		{
			DOTCPSEND_REAL(socketptr, buffer, length, 0, status);
			if (0 != status)
			{	/* current callers do this check and return */
				/* if timed, just return - maybe set flag in struct */
				socketptr->obuffer_errno = status;
				status = -1;
			} else
				status = length;
			break;
		}
	}
	if (status == length)
	{
		socketptr->obuffer_errno = 0;
		if (resetbuffer)
			socketptr->obuffer_length = socketptr->obuffer_offset = 0;
	}
	return status;
}

/*	prototype in iosocketdef.h since called by iosocket_flush and iosocket_close */
ssize_t iosocket_output_buffer(socket_struct *socketptr)
{
	ssize_t	status;
	status = iosocket_output(socketptr, socketptr->obuffer, socketptr->obuffer_length, TRUE, FALSE);
	return status;
}

void iosocket_output_timed(socket_struct *socketptr);

void iosocket_output_timed(socket_struct *socketptr)
{
	ssize_t	status;
	size_t	length;

	socketptr->obuffer_timer_set = FALSE;
	if (!socketptr->obuffer_output_active && (0 < socketptr->obuffer_length))
	{	/* no current writer so output the buffer */
		socketptr->obuffer_output_active = TRUE;
		length = socketptr->obuffer_length;
		status = iosocket_output(socketptr, socketptr->obuffer, length, TRUE, TRUE);
		socketptr->obuffer_output_active = FALSE;
	}
	/* reschedule timer if needed */
	if ((0 < socketptr->obuffer_length) && (0 == socketptr->obuffer_errno))
	{
		socketptr->obuffer_timer_set = TRUE;
		start_timer((TID)socketptr, socketptr->obuffer_flush_time, iosocket_output_timed,
			SIZEOF(socketptr), (char *)&socketptr);
	}
}

ssize_t	iosocket_write_buffered(socket_struct *socketptr, char *buffer, size_t length);
ssize_t	iosocket_write_buffered(socket_struct *socketptr, char *buffer, size_t length)
{
	ssize_t		status, obuffered_len;
	int		errlen, devlen;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;
	const char	*errptr;

	socketptr->obuffer_output_active = TRUE;	/* lock out timed writes */
	status = (0 != socketptr->obuffer_errno) ? -1 : 0;
	if ((0 == status ) && (0 < socketptr->obuffer_length) && ((socketptr->obuffer_size - socketptr->obuffer_length) <= length))
	{	/* more output than space left in buffer */
		obuffered_len = socketptr->obuffer_length;
		status = iosocket_output_buffer(socketptr);
		if (obuffered_len == status)
			status = 0;			/* success */
	}
	if ((0 == status ) && (length > socketptr->obuffer_size))
	{	/* more output than can fit in buffer so just output it now */
		status = iosocket_output(socketptr, buffer, length, FALSE, FALSE);
		if (status != length)
			status = -1;			/* failure */
	} else if (0 == status)
	{	/* put in buffer since room is available */
		memcpy((void *)(socketptr->obuffer + socketptr->obuffer_offset), buffer, length);
		socketptr->obuffer_offset += length;
		socketptr->obuffer_length += length;
		status = length;
		/* start timer if not active */
		if (!socketptr->obuffer_timer_set)
		{
			socketptr->obuffer_timer_set = TRUE;
			start_timer((TID)socketptr, socketptr->obuffer_flush_time, iosocket_output_timed,
				SIZEOF(socketptr), (char *)&socketptr);
		}
	}
	socketptr->obuffer_output_active = FALSE;
	if (0 > status)
	{	/* report error */
		iosocket_buffer_error(socketptr);
	}
	return status;
}

void	iosocket_write_real(mstr *v, boolean_t convert_output)
{
	io_desc		*iod;
	mstr		tempv;
	char		*out, *c_ptr, *c_top;
	int		in_b_len, b_len, status, new_len, c_len, mb_len;
	int		flags;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr;
	boolean_t	ch_set;

	DBGSOCK2((stdout, "socwrite: ************************** Top of iosocket_write\n"));
	iod = io_curr_device.out;
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	if (0 >= dsocketptr->n_socket)
	{
#		ifndef VMS
		if (iod == io_std_device.out)
			ionl_write(v);
		else
#		endif
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSOCKETINDEV);
		REVERT_GTMIO_CH(&iod->pair, ch_set);
		return;
	}
	if (dsocketptr->n_socket <= dsocketptr->current_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return;
	}
	if (dsocketptr->mupintr)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	ENSURE_DATA_SOCKET(socketptr);
#ifdef MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;		/* return EPIPE instead of SIGPIPE */
#else
	flags = 0;
#endif
	tempv = *v;
	socketptr->lastop = TCP_WRITE;
	/* In case the CHSET changes from non-UTF-16 to UTF-16 and a read has already been done,
	 * there's no way to read the BOM bytes & to determine the variant. So default to UTF-16BE.
	 */
	if (!socketptr->first_write && (!IS_UTF16_CHSET(dsocketptr->ochset_utf16_variant) && (CHSET_UTF16 == iod->ochset)))
	{
		iod->ochset = dsocketptr->ochset_utf16_variant = CHSET_UTF16BE;
		get_chset_desc(&chset_names[iod->ochset]);
	}
	if (socketptr->first_write)
	{ /* First WRITE, do following
	     Transition to UTF16BE if ochset is UTF16 and WRITE a BOM
	   */
		if (CHSET_UTF16 == iod->ochset)
		{
			DBGSOCK2((stdout, "socwrite: First write UTF16 -- writing BOM\n"));
			iod->ochset = CHSET_UTF16BE; /* per Unicode standard, assume big endian when endian
							format is unspecified */
			dsocketptr->ochset_utf16_variant = iod->ochset;
			get_chset_desc(&chset_names[iod->ochset]);
			DOTCPSEND(socketptr, UTF16BE_BOM, UTF16BE_BOM_LEN, flags, status);
			DBGSOCK2((stdout, "socwrite: TCP send of BOM-BE with rc %d\n", status));
			if (0 != status)
			{
				SOCKERROR(iod, socketptr, ERR_SOCKWRITE, status);
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			}
#ifdef UNIX
			else if (iod == io_std_device.out)
				prin_out_dev_failure = FALSE;
#endif
		}
		socketptr->first_write = FALSE;

	}
	if (CHSET_UTF16BE == iod->ochset || CHSET_UTF16LE == iod->ochset)
	{
		if ((0 < socketptr->zff.len) && (socketptr->zff.addr == socketptr->ozff.addr))
		{ /* Convert ZFF into ochset format so we don't need to convert every time ZFF is output */
			new_len = gtm_conv(chset_desc[CHSET_UTF8], chset_desc[iod->ochset], &socketptr->zff, NULL,
						NULL);
			if (MAX_ZFF_LEN < new_len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZFF2MANY, 2, new_len, MAX_ZFF_LEN);
			socketptr->ozff.addr = (char *)malloc(MAX_ZFF_LEN);	/* should not need */
			socketptr->ozff.len = new_len;
			UNICODE_ONLY(socketptr->ozff.char_len = 0); /* don't care */
			memcpy(socketptr->ozff.addr, stringpool.free, new_len);
			if (tempv.addr == socketptr->zff.addr)
				tempv = socketptr->ozff;	/* from iosocket_wtff so use converted form */
		}
	}
	/* Convert DELIMITER 0 to OCHSET format to avoid repeated conversions of DELIM0 on output.
	 * CHSET can be switched b/w M/UTF-8/UTF-16* in UTF-8 mode. Convert the odelimitor0 accordingly
	 * 1. odelimiter0 == idelimiter[0] (i.e. it's not been converted) && IS_UTF16_CHSET
	 * 2. odelimiter0 != idelimiter[0] (i.e. it's been converted to UTF-16) && CHSET is NOT UTF-16
	 */
	if (gtm_utf8_mode && (0 < socketptr->n_delimiter))
	{
		if (((socketptr->delimiter[0].addr == socketptr->odelimiter0.addr) && IS_UTF16_CHSET(iod->ochset))
			|| ((socketptr->delimiter[0].addr != socketptr->odelimiter0.addr) && !IS_UTF16_CHSET(iod->ochset)))
			iosocket_odelim_conv(socketptr, iod->ochset);

		if (tempv.addr == socketptr->delimiter[0].addr)
			tempv = socketptr->odelimiter0;	/* from iosocket_wteol so use converted form */
	}
	memcpy(iod->dollar.device, "0", SIZEOF("0"));
	if (CHSET_M != iod->ochset)
	{ /* For ochset == UTF-8, validate the output,
	   * For ochset == UTF-16[B|L]E, convert the output (and validate during conversion)
	   */
		if (CHSET_UTF8 == iod->ochset)
		{
			UTF8_LEN_STRICT(v->addr, v->len); /* triggers badchar error for invalid sequence */
			tempv = *v;
		} else
		{
			assert(CHSET_UTF16BE == iod->ochset || CHSET_UTF16LE == iod->ochset);
			/* Certain types of writes (calls from iosocket_wteol or _wtff) already have their output
			   converted. Converting again just wrecks it so avoid that when necessary.
			*/
			if (convert_output)
			{
				new_len = gtm_conv(chset_desc[CHSET_UTF8], chset_desc[iod->ochset], v, NULL, NULL);
				tempv.addr = (char *)stringpool.free;
				tempv.len = new_len;
				/* Since there is no dependence on string pool between now and when we send the data,
				   we won't bother "protecting" the stringpool value. This space can be used again
				   by whomever needs it without us forcing a garbage collection due to IO reformat.
				*/
				/* stringpool.free += new_len; */
			}
		}
	}
	if (0 != (in_b_len = tempv.len))
	{
		DBGSOCK2((stdout, "socwrite: starting output loop (%d bytes) - iodwidth: %d  wrap: %d\n",
			  in_b_len, iod->width, iod->wrap));
		for (out = tempv.addr;  ; out += b_len)
		{
			DBGSOCK2((stdout, "socwrite: ---------> Top of write loop $x: %d  $y: %d  in_b_len: %d\n",
				  iod->dollar.x, iod->dollar.y, in_b_len));
			if (!iod->wrap)
				b_len = in_b_len;
			else
			{
				if ((iod->dollar.x >= iod->width) && (START == iod->esc_state))
				{
					/* Should this really be iosocket_Wteol() (for FILTER)? IF we call iosocket_wteol(),
					 * there will be recursion iosocket_Write -> iosocket_Wteol ->iosocket_Write */
					if (0 < socketptr->n_delimiter)
					{
						DOTCPSEND(socketptr, socketptr->odelimiter0.addr, socketptr->odelimiter0.len,
								(socketptr->urgent ? MSG_OOB : 0) | flags, status);
						DBGSOCK2((stdout, "socwrite: TCP send of %d byte delimiter with rc %d\n",
							  socketptr->odelimiter0.len, status));
						if (0 != status)
						{
							SOCKERROR(iod, socketptr, ERR_SOCKWRITE, status);
							REVERT_GTMIO_CH(&iod->pair, ch_set);
							return;
						}
#ifdef UNIX
						else if (iod == io_std_device.out)
							prin_out_dev_failure = FALSE;
#endif
					}
					iod->dollar.y++;
					iod->dollar.x = 0;
					DBGSOCK2((stdout, "socwrite: $x > width - wrote delimiter: %d  $x: %d  $y: %d\n",
						  (0 < socketptr->n_delimiter), iod->dollar.x, iod->dollar.y));
				}
				if ((START != iod->esc_state) || ((int)(iod->dollar.x + in_b_len) <= (int)iod->width))
				{ /* enough room even in the worst case, i.e., if width - dollar.x can accommodate in_b_len chars,
				   * it certainly can accommodate in_b_len bytes */
					b_len = in_b_len;
				} else
				{
					c_len = iod->width - iod->dollar.x;
					for (c_ptr = out, c_top = out + in_b_len, b_len = 0;
					     (c_ptr < c_top) && c_len--;
					     b_len += mb_len, c_ptr += mb_len)
					{
						mb_len = (CHSET_M       == iod->ochset) ? 0 :
						         (CHSET_UTF8    == iod->ochset) ? UTF8_MBFOLLOW(c_ptr) :
							 (CHSET_UTF16BE == iod->ochset) ? UTF16BE_MBFOLLOW(c_ptr, c_top) :
							 UTF16LE_MBFOLLOW(c_ptr, c_top);
						assert(-1 != mb_len);
						mb_len++;
					}
					DBGSOCK2((stdout, "socwrite: computing string length in chars: in_b_len: %d  mb_len: %d\n",
						  in_b_len, mb_len));
				}
			}
			assert(0 != b_len);
			DOTCPSEND(socketptr, out, b_len, (socketptr->urgent ? MSG_OOB : 0) | flags, status);
			DBGSOCK2((stdout, "socwrite: TCP data send of %d bytes with rc %d\n", b_len, status));
			if (0 != status)
			{
				SOCKERROR(iod, socketptr, ERR_SOCKWRITE, status);
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			}
#ifdef UNIX
			else if (iod == io_std_device.out)
				prin_out_dev_failure = FALSE;
#endif
			dollarx(iod, (uchar_ptr_t)out, (uchar_ptr_t)out + b_len);
			DBGSOCK2((stdout, "socwrite: $x/$y updated by dollarx():  $x: %d  $y: %d  filter: %d  escape:  %d\n",
				  iod->dollar.x, iod->dollar.y, iod->write_filter, iod->esc_state));
			in_b_len -= b_len;
			if (0 >= in_b_len)
				break;
		}
		iod->dollar.za = 0;
	}
	DBGSOCK2((stdout, "socwrite: <--------- Leaving iosocket_write\n"));
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
