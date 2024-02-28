/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gtm_poll.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "eintr_wrappers.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
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
#include "svnames.h"
#include "op.h"
#include "gtmio.h"
#include "wbox_test_init.h"
#include "util.h"
#ifdef GTM_TLS
#include "gtm_tls.h"
#endif

GBLREF boolean_t		prin_in_dev_failure, prin_out_dev_failure;
GBLREF io_pair			io_curr_device, io_std_device;
GBLREF mstr			chset_names[];
GBLREF mval			dollar_zstatus;
GBLREF spdesc			stringpool;
GBLREF UConverter		*chset_desc[];
GBLREF volatile int4		outofband;
#ifdef GTM_TLS
GBLREF	gtm_tls_ctx_t		*tls_ctx;
#endif

error_def(ERR_CURRSOCKOFR);
error_def(ERR_NOPRINCIO);
error_def(ERR_NOSOCKETINDEV);
error_def(ERR_SOCKPASSDATAMIX);
error_def(ERR_SOCKWRITE);
error_def(ERR_TEXT);
error_def(ERR_TLSIOERROR);
error_def(ERR_ZFF2MANY);
error_def(ERR_ZINTRECURSEIO);
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_SETSOCKOPTERR);

#define SOCKBLOCK_UTF_BOM	"Non blocking WRITE error sending UTF-16 BOM"
#define SOCKBLOCK_DELIM		"Non blocking WRITE error sending DELIMITER or ZFF"
#define SOCKBLOCK_WRAP		"Non blocking WRITE error sending DELIMITER while wrapping"
#define SOCKBLOCK_OUTOFBAND	"Non blocking WRITE interrupted"
#define SOCKBLOCK_BLOCKED	"Non blocking WRITE blocked - no further WRITEs allowed"

/* if non blocking writes, stop if non restartable outofband event */
#define SEND_OOB(SOCKETPTR, BUF, LEN, FLAGS, RC)							\
{													\
	do												\
	{												\
		RC = send((SOCKETPTR)->sd, BUF, LEN, FLAGS);	/* BYPASSOK handle EINTR here */	\
		if ((-1 != RC) || (EINTR != errno))							\
			break;										\
		eintr_handling_check();									\
	} while (!outofband || ((SOCKETPTR)->nonblocked_output ? (jobinterrupt == outofband) : TRUE));	\
}

#define DOTCPSEND_REAL(SOCKETPTR, SBUFF, SBUFF_LEN, SFLAGS, WRITTEN, RC)						\
{															\
	ssize_t		gtmioStatus;											\
	size_t		gtmioBuffLen;											\
	size_t		gtmioChunk;											\
	size_t		gtmioWritten;											\
	uint4		gtmioRetries;											\
	sm_uc_ptr_t	gtmioBuff;											\
															\
	gtmioBuffLen = SBUFF_LEN;											\
	gtmioBuff = (sm_uc_ptr_t)(SBUFF);										\
	gtmioWritten = gtmioRetries = 0;										\
	for (;;)													\
        {														\
		gtmioChunk = gtmioBuffLen;										\
		SEND_OOB(SOCKETPTR, gtmioBuff, gtmioChunk, SFLAGS, gtmioStatus);					\
		if ((ssize_t)-1 != gtmioStatus)										\
	        {													\
			gtmioBuffLen -= gtmioStatus;									\
			gtmioWritten += gtmioStatus;									\
			if (0 == gtmioBuffLen)										\
				break;											\
			gtmioBuff += gtmioStatus;									\
	        } else if (!(SOCKETPTR)->nonblocking)									\
			break;												\
		else if ((EAGAIN == errno) || (EWOULDBLOCK == errno))							\
		{ 							 						\
			if (++gtmioRetries > (SOCKETPTR)->max_output_retries)						\
			{												\
				(SOCKETPTR)->output_failures = gtmioRetries;						\
				break;											\
			}												\
			SHORT_SLEEP(WAIT_FOR_BLOCK_TIME);								\
		} else													\
			break;												\
        }														\
	WRITTEN = gtmioWritten;												\
	if ((ssize_t)-1 == gtmioStatus)    	/* Had legitimate error - return it */					\
		RC = errno;												\
	else if (0 == gtmioBuffLen)											\
	        RC = 0;													\
	else														\
		RC = -1;		/* Something kept us from sending what we wanted */				\
}

#define DOTCPSEND(SOCKETPTR, SBUFF, SBUFF_LEN, SFLAGS, WRITTEN, RC)							\
{															\
	ssize_t	localstatus, lwritten;											\
	if (!(SOCKETPTR)->obuffer_in_use)										\
		DOTCPSEND_REAL(SOCKETPTR, SBUFF, SBUFF_LEN, SFLAGS, WRITTEN, RC)					\
	else														\
	{	/* need to return and handle  partial write WRITTEN */							\
		localstatus = iosocket_write_buffered(SOCKETPTR, SBUFF, SBUFF_LEN, &lwritten);				\
		if (SBUFF_LEN == lwritten)										\
		{													\
			assert(0 == localstatus);									\
			RC = 0;												\
		} else													\
			RC = localstatus;										\
		WRITTEN = lwritten;											\
	}														\
	if ((0 != RC) && socketptr->nonblocked_output)									\
		socketptr->output_blocked = TRUE;									\
}

void	iosocket_write(mstr *v)
{
	iosocket_write_real(v, TRUE);
}

int iosocket_buffer_error(socket_struct *socketptr)
{	/* output error from obuffer_errno */
	int		errlen, devlen, save_obuffer_errno, msgid;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;
	const char	*errptr;

	if (0 == socketptr->obuffer_errno)
		return 0;			/* no error */
	dsocketptr = socketptr->dev;
	iod = dsocketptr->iod;
#	ifdef GTM_TLS
	if (socketptr->tlsenabled)
	{
		if (-1 == socketptr->obuffer_errno)
		{
			errptr = gtm_tls_get_error();
		} else
		{
			errptr = (char *)STRERROR(socketptr->obuffer_errno);
		}
		save_obuffer_errno = 0;
		msgid = ERR_TLSIOERROR;
	} else
#	endif
	{
		save_obuffer_errno = socketptr->obuffer_errno;
		errptr = NULL;
		msgid = ERR_SOCKWRITE;
	}
	socketptr->obuffer_errno = 0;
	SOCKWRTERROR(iod, socketptr, msgid, save_obuffer_errno, errptr, "send");
	return -1;	/* error issued */
}

ssize_t iosocket_output(socket_struct *socketptr, char *buffer, size_t length, boolean_t resetbuffer,
				boolean_t timed, ssize_t *written);

ssize_t iosocket_output(socket_struct *socketptr, char *buffer, size_t length, boolean_t resetbuffer,
				boolean_t timed, ssize_t *written)
{	/* note: buffer may not be socketptr->obuffer */
	boolean_t	pollwrite;
	ssize_t		status, local_written;
	size_t		llen;
	int		bytessent, istatus, timeout, save_errno, output_retries;
	char		*lbuffer;
#	ifdef GTM_TLS
	int		tlspolldirection = 0, short_sends = 0, save_errno2 = 0;
#	endif
	struct pollfd	fds;

	*written = local_written = 0;
	if (!socketptr->obuffer_output_active)
		return 0;		/* how did we get here */
	if (timed)
	{
		if (0 != socketptr->obuffer_errno)
			return -1;	/* unprocessed error */
		timeout = 0;		/* no waiting in poll */
	} else
	{
		assert(0 != socketptr->obuffer_wait_time);
		timeout = socketptr->obuffer_wait_time;
	}
	llen = length;
	output_retries = status = 0;
	lbuffer = buffer;
	while ((0 == status) && (0 < llen))
	{	/* poll/select tlspolldirection - needed if noblocking */
#		ifdef GTM_TLS
		if (socketptr->tlsenabled)
			pollwrite = (tlspolldirection == GTMTLS_WANT_READ) ? FALSE : TRUE;
		else
#		endif
			pollwrite = TRUE;
		fds.fd = socketptr->sd;
		fds.events = pollwrite ? POLLOUT : POLLIN;
		istatus = poll(&fds, 1, timeout);
		if (-1 == istatus)
		{
			save_errno = errno;
			if (timed)
			{	/* called from timer so only try once */
				socketptr->obuffer_errno = save_errno;
				status = -1;
				break;
			}
			if ((EAGAIN == save_errno) || (EWOULDBLOCK == save_errno))
			{
				if (socketptr->nonblocking && (++output_retries > socketptr->max_output_retries))
				{
					socketptr->output_failures = output_retries;
					socketptr->obuffer_errno = save_errno;
					status = -1;
					break;
				}
				/* No need of "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" call as "rel_quant()" checks for
				 * deferred signals already (invokes "DEFERRED_SIGNAL_HANDLING_CHECK").
				 */
				rel_quant();	/* seems like a legitimate rel_quant */
			} else if (EINTR != save_errno)
			{
				socketptr->obuffer_errno = save_errno;
				status = -1;
				break;
			} else if (socketptr->nonblocked_output && (no_event != outofband) && (jobinterrupt != outofband))
			{
				socketptr->obuffer_errno = save_errno;
				status = -1;
				break;
			} else
				eintr_handling_check();
			continue;
		} else if (0 == istatus)
		{	/* poll/select timedout */
			if (socketptr->nonblocking && (++output_retries > socketptr->max_output_retries))
			{
				socketptr->obuffer_errno = EAGAIN;
				status = -1;
				break;
			}
			continue;
		}
		HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
#		ifdef GTM_TLS
		if (socketptr->tlsenabled)
		{
			bytessent = gtm_tls_send((gtm_tls_socket_t *)socketptr->tlssocket, lbuffer, llen);
			if (0 < bytessent)
			{	/* unless partial writes enabled either none or all should have been written */
				save_errno2 = gtm_tls_errno();	/* get for debug */
				UNUSED(save_errno2);	/* to avoid [clang-analyzer-deadcode.DeadStores] warning */
				if (llen > bytessent)
				{
					assert(socketptr->nonblocked_output);
					++short_sends;
				}
				llen -= bytessent;
				lbuffer += bytessent;
				tlspolldirection = 0;
				local_written += bytessent;
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
						socketptr->obuffer_errno = save_errno = gtm_tls_errno();
						if (-1 == save_errno)
						{	/* non errno error use gtm_tls_get_error when reporting */
							status = -1;
							break;
						}
						if (!socketptr->nonblocked_output && (0 == bytessent) && (0 == save_errno))
						{	/* should not get here */
							assert(socketptr->nonblocked_output || (0 != bytessent) ||
								(0 != save_errno));
						} else if (socketptr->nonblocked_output
							&& ((EAGAIN == save_errno) || (EWOULDBLOCK == save_errno)))
						{
							if (socketptr->nonblocking &&
								(++output_retries > socketptr->max_output_retries))
							{
								socketptr->output_failures = output_retries;
								status = -1;
								break;
							} else
								assert(!socketptr->nonblocking ||
									(output_retries <= socketptr->max_output_retries));
							rel_quant();
							break;
						} else if (socketptr->nonblocked_output &&
							((EINTR == save_errno) &&
							(0 != outofband) && (jobinterrupt != outofband)))
						{	/* not JOBEXAM signal */
							status = -1;
							break;
						} else
						{
							assert(-1 == bytessent);
							if (ECONNRESET == socketptr->obuffer_errno)
							{
								return (ssize_t)(-2);
							} else
								return (ssize_t)(-1);
						}
						break;
				}
				if (-1 == status)
					break;
			}
		} else
#		endif
		{	/* as of 2020/8/18 buffered output is TLS only so revisit this section if non TLS buffered output added by
			   GTM-3162 buffering on socket device */
			DOTCPSEND_REAL(socketptr, buffer, length, 0, local_written, status);
			if (0 != status)	/* 0=allsent -1=partial else=errno */
			{	/* current callers do this check and return */
				socketptr->obuffer_errno = save_errno = status;
				status = -1;
				/* next 13 lines need rework if non TLS buffered output */
				if (timed)
				{	/* called from timer so only try once */
					break;
				}
				if (!socketptr->nonblocked_output && ((EAGAIN == save_errno) || (EWOULDBLOCK == save_errno)))
				{
					rel_quant();	/* seems like a legitimate rel_quant */
					status = 0;
					continue;
				} else if (EINTR != save_errno)
				{
					status = -1;
					break;
				}
			} else
				status = 0;
			break;		/* DOTCPSEND_REAL does retries so no need here */
		}
	}
	if (local_written == length)
	{	/* all output written */
		socketptr->obuffer_errno = 0;
		if (resetbuffer)
			socketptr->obuffer_length = socketptr->obuffer_offset = 0;
	} else
		assert(0 != status);
	*written = local_written;
	return status;
}

/*	prototype in iosocketdef.h since called by iosocket_flush and iosocket_close */
ssize_t iosocket_output_buffer(socket_struct *socketptr)
{
	ssize_t	status, written;
	status = iosocket_output(socketptr, socketptr->obuffer, socketptr->obuffer_length, TRUE, FALSE, &written);
	return status;
}

void iosocket_output_timed(socket_struct *socketptr);

void iosocket_output_timed(socket_struct *socketptr)
{
	ssize_t	status, written;
	size_t	length;

	socketptr->obuffer_timer_set = FALSE;
	if (!socketptr->obuffer_output_active && (0 < socketptr->obuffer_length))
	{	/* no current writer so output the buffer */
		socketptr->obuffer_output_active = TRUE;
		length = socketptr->obuffer_length;
		status = iosocket_output(socketptr, socketptr->obuffer, length, TRUE, TRUE, &written);
		UNUSED(status);
		socketptr->obuffer_output_active = FALSE;
		assert((length == written) || (0 != socketptr->obuffer_errno));	/* short write */
	}
	/* reschedule timer if needed for blocking socket */
	if ((0 < socketptr->obuffer_length) && (0 == socketptr->obuffer_errno) && !socketptr->nonblocked_output)
	{
		assert(0 != socketptr->obuffer_flush_time);
		socketptr->obuffer_timer_set = TRUE;
		start_timer((TID)socketptr, socketptr->obuffer_flush_time * (uint8)NANOSECS_IN_MSEC, iosocket_output_timed,
			SIZEOF(socketptr), (char *)&socketptr);
	}
}

ssize_t	iosocket_write_buffered(socket_struct *socketptr, char *buffer, size_t length, ssize_t *written);
ssize_t	iosocket_write_buffered(socket_struct *socketptr, char *buffer, size_t length, ssize_t *written)
{
	ssize_t		status, obuffered_len;
	int		errlen, devlen;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;
	const char	*errptr;

	socketptr->obuffer_output_active = TRUE;	/* lock out timed writes */
	*written = 0;
	status = socketptr->obuffer_errno;
	if ((0 == status ) && (0 < socketptr->obuffer_length) && ((socketptr->obuffer_size - socketptr->obuffer_length) <= length))
	{	/* more output than space left in buffer */
		status = iosocket_output_buffer(socketptr);
	}
	if ((0 == status ) && (length > (socketptr->obuffer_size - socketptr->obuffer_offset)))
	{	/* more output than can fit in buffer so just output it now */
		assert(0 == socketptr->obuffer_length);
		status = iosocket_output(socketptr, buffer, length, FALSE, FALSE, written);
	} else if (0 == status)
	{	/* put in buffer since room is available */
		memcpy((void *)(socketptr->obuffer + socketptr->obuffer_offset), buffer, length);
		socketptr->obuffer_offset += length;
		socketptr->obuffer_length += length;
		/* start timer if not active */
		if (!socketptr->obuffer_timer_set && (0 != socketptr->obuffer_flush_time))
		{
			status = 0;
			*written = length;
			socketptr->obuffer_timer_set = TRUE;
			start_timer((TID)socketptr, socketptr->obuffer_flush_time * (uint8)NANOSECS_IN_MSEC, iosocket_output_timed,
				SIZEOF(socketptr), (char *)&socketptr);
		} else if (0 == socketptr->obuffer_flush_time)
		{
			obuffered_len = socketptr->obuffer_length;
			status = iosocket_output_buffer(socketptr);
			*written = (obuffered_len - socketptr->obuffer_length);
			assert((length == *written) || (0 != status));
		}
	}
	socketptr->obuffer_output_active = FALSE;
	if ((0 > status) || (0 != socketptr->obuffer_errno))
	{	/* report error */
		status = iosocket_buffer_error(socketptr);
	}
	return status;
}

void	iosocket_write_real(mstr *v, boolean_t convert_output)
{	/* convert_output is FALSE when called from wteol or wtff */
	io_desc		*iod;
	mstr		tempv;
	char		*out, *c_ptr, *c_top, *errptr, *errortext;
	int		in_b_len, b_len, status, new_len, c_len, mb_len;
	int		flags, fcntl_flags, fcntl_res, save_errno;
	size_t		written, total_written;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGSOCK2((stdout, "socwrite: ************************** Top of iosocket_write\n"));
	iod = io_curr_device.out;
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	if (0 >= dsocketptr->n_socket)
	{
		if (iod == io_std_device.out)
			ionl_write(v);
		else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_NOSOCKETINDEV);
		REVERT_GTMIO_CH(&iod->pair, ch_set);
		return;
	}
	if (dsocketptr->n_socket <= dsocketptr->current_socket)
	{
		assert(FALSE);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
	}
	if (dsocketptr->mupintr)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTRECURSEIO);
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	ENSURE_DATA_SOCKET(socketptr);
#ifdef MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;		/* return EPIPE instead of SIGPIPE */
#else
	flags = 0;
#endif
	tempv = *v;
	socketptr->lastop = TCP_WRITE;
	if (socketptr->nonblocked_output)
	{
		socketptr->lastaction = dsocketptr->waitcycle;
		socketptr->readyforwhat &= ~SOCKREADY_WRITE;
		socketptr->pendingevent &= ~SOCKPEND_WRITE;
		socketptr->lastarg_sent = 0;
		if (socketptr->output_blocked)
		{
			SOCKWRTERROR(iod, socketptr, ERR_SOCKWRITE, 0, SOCKBLOCK_BLOCKED, "");
			REVERT_GTMIO_CH(&iod->pair, ch_set);
			return;
		}
		if (!socketptr->nonblocking)
		{	/* set O_NONBLOCK if needed */
			FCNTL2(socketptr->sd, F_GETFL, fcntl_flags);
			if (fcntl_flags < 0)
			{
				iod->dollar.za = ZA_IO_ERR;
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
						LEN_AND_LIT("F_GETFL FOR NON BLOCKING WRITE"),
						save_errno, LEN_AND_STR(errptr));
			}
			FCNTL3(socketptr->sd, F_SETFL, fcntl_flags | (O_NDELAY | O_NONBLOCK), fcntl_res);
			if (fcntl_res < 0)
			{
				iod->dollar.za = ZA_IO_ERR;
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
						LEN_AND_LIT("F_SETFL FOR NON BLOCKING WRITE"),
						save_errno, LEN_AND_STR(errptr));
			}
			socketptr->nonblocking = TRUE;
		}
	}
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
			iod->ochset = CHSET_UTF16BE; /* per standard, assume big endian when endian
							format is unspecified */
			dsocketptr->ochset_utf16_variant = iod->ochset;
			get_chset_desc(&chset_names[iod->ochset]);
			DOTCPSEND(socketptr, UTF16BE_BOM, UTF16BE_BOM_LEN, flags, written, status);
			DBGSOCK2((stdout, "socwrite: TCP send of BOM-BE with rc %d\n", status));
			if (0 != status)
			{
				if (!socketptr->obuffer_in_use || (-1 != status))
				{	/* if buffered and status == -1 then error was already issued */
					if (socketptr->nonblocked_output &&
						((EAGAIN == status) || (EWOULDBLOCK == status) || (EINTR == status)))
					{
						errortext = SOCKBLOCK_UTF_BOM;
						status = 0;
						socketptr->lastarg_sent = (size_t)-1;	/* to force need to /BLOCK(CLEAR) */
					} else
						errortext = NULL;
					SOCKWRTERROR(iod, socketptr, ERR_SOCKWRITE, status, errortext, "send");
				}
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			} else if (iod == io_std_device.out)
				prin_out_dev_failure = FALSE;
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
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZFF2MANY, 2, new_len, MAX_ZFF_LEN);
			socketptr->ozff.addr = (char *)malloc(MAX_ZFF_LEN);	/* should not need */
			socketptr->ozff.len = new_len;
			UTF8_ONLY(socketptr->ozff.char_len = 0); /* don't care */
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
	total_written = 0;		/* data only this argument */
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
					{	/* delimiters from wrapping are not counted for non blocking output */
						DOTCPSEND(socketptr, socketptr->odelimiter0.addr, socketptr->odelimiter0.len,
							(socketptr->urgent ? MSG_OOB : 0) | flags, written, status);
						DBGSOCK2((stdout, "socwrite: TCP send of %d byte delimiter with rc %d\n",
							  socketptr->odelimiter0.len, status));
						if (0 != status)
						{
							if (!socketptr->obuffer_in_use || (-1 != status))
							{	/* if buffered and status == -1 then error was already issued */
								if (socketptr->nonblocked_output && ((EAGAIN == status) ||
									(EWOULDBLOCK == status) || (EINTR == status) ||
									(-1 == status)))
								{
									errortext = SOCKBLOCK_WRAP;
									status = 0;
									socketptr->lastarg_sent = (size_t)-1;	/* need to CLEAR */
								} else
									errortext = NULL;
								SOCKWRTERROR(iod, socketptr, ERR_SOCKWRITE, status,
									errortext, "send");
							}
							REVERT_GTMIO_CH(&iod->pair, ch_set);
							return;
						} else if (iod == io_std_device.out)
							prin_out_dev_failure = FALSE;
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
			DOTCPSEND(socketptr, out, b_len, (socketptr->urgent ? MSG_OOB : 0) | flags, written, status);
			DBGSOCK2((stdout, "socwrite: TCP data send of %d (of %d) bytes with rc %d\n", written, b_len, status));
			total_written += written;
			if (0 != status)
			{
				if (!socketptr->obuffer_in_use || (-1 != status))
				{	/* if buffered and status == -1 then error was already issued */
					if (socketptr->nonblocked_output &&
						((EAGAIN == status) || (EWOULDBLOCK == status) || (EINTR == status) ||
						(-1 == status)))
					{
						if (!convert_output)
						{	/* called from iosocket_wteol or iosocket_wtff */
							status = 0;
							errortext = SOCKBLOCK_DELIM;
						} else
						{
							if (EINTR == status)
							{
								eintr_handling_check();
								status = 0;
								errortext = SOCKBLOCK_OUTOFBAND;
							} else
								errortext = NULL;
							socketptr->lastarg_sent = (0 != total_written) ? total_written : -1;
							if (0 < written)
								dollarx(iod, (uchar_ptr_t)out, (uchar_ptr_t)out + written);
						}
					} else
						errortext = NULL;
					SOCKWRTERROR(iod, socketptr, ERR_SOCKWRITE, status, errortext, "send");
				}
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			} else
			{	/* no error */
				if (iod == io_std_device.out)
					prin_out_dev_failure = FALSE;
			}
			if (socketptr->nonblocked_output)
				b_len = written;	/* update to amount actually written */
			dollarx(iod, (uchar_ptr_t)out, (uchar_ptr_t)out + b_len);
			DBGSOCK2((stdout, "socwrite: $x/$y updated by dollarx():  $x: %d  $y: %d  filter: %d  escape:  %d\n",
				  iod->dollar.x, iod->dollar.y, iod->write_filter, iod->esc_state));
			in_b_len -= b_len;
			if (0 >= in_b_len)
				break;
		}
		iod->dollar.za = 0;
	}
	if (socketptr->nonblocked_output)
	{
		socketptr->args_written++;
		if (tempv.len != total_written)
			socketptr->lastarg_sent = (0 != total_written) ? total_written : -1;	/* non restartable outofband */
	}
	DBGSOCK2((stdout, "socwrite: <--------- Leaving iosocket_write\n"));
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
