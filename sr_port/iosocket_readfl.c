/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "gtm_conv.h"
#include "gtm_utf8.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "send_msg.h"
#include "error.h"

GBLREF	stack_frame      	*frame_pointer;
GBLREF	unsigned char    	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF	mv_stent         	*mv_chain;
GBLREF	io_pair 		io_curr_device;
#ifdef UNIX
GBLREF	io_pair			io_std_device;
GBLREF	bool			prin_in_dev_failure;
#endif
GBLREF	bool			out_of_time;
GBLREF	spdesc 			stringpool;
GBLREF	volatile int4		outofband;
GBLREF	mstr			chset_names[];
GBLREF	UConverter		*chset_desc[];
GBLREF  boolean_t       	dollar_zininterrupt;
GBLREF	int			socketus_interruptus;
GBLREF	boolean_t		gtm_utf8_mode;

#ifdef UNICODE_SUPPORTED
/* Maintenance of $KEY, $DEVICE and $ZB on a badchar error */
void iosocket_readfl_badchar(mval *vmvalptr, int datalen, int delimlen, unsigned char *delimptr,
			     unsigned char *strend)
{
	int		tmplen, len;
	unsigned char	*delimend;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;

	iod = io_curr_device.in;
	dsocketptr = (d_socket_struct *)(iod->dev_sp);
	vmvalptr->str.len = datalen;
	vmvalptr->str.addr = (char *)stringpool.free;
	if (0 < datalen)
	{	/* Return how much input we got */
		if (CHSET_M != iod->ichset && CHSET_UTF8 != iod->ichset)
		{
			SOCKET_DEBUG(PRINTF("socrflbc: Converting UTF16xx data back to UTF8 for internal use\n"); DEBUGSOCKFLUSH);
			vmvalptr->str.len = gtm_conv(chset_desc[iod->ichset], chset_desc[CHSET_UTF8], &vmvalptr->str, NULL, NULL);
			vmvalptr->str.addr = (char *)stringpool.free;
		}
		stringpool.free += vmvalptr->str.len;
	}
	if (NULL != strend && NULL != delimptr)
	{	/* First find the end of the delimiter (max of 4 bytes) */
		if (0 == delimlen)
		{
			for (delimend = delimptr; GTM_MB_LEN_MAX >= delimlen && delimend < strend; ++delimend, ++delimlen)
			{
				if (UTF8_VALID(delimend, strend, tmplen))
					break;
			}
		}
		if (0 < delimlen)
		{	/* Set $KEY and $ZB with the failing badchar */
			memcpy(iod->dollar.zb, delimptr, MIN(delimlen, ESC_LEN - 1));
			iod->dollar.zb[MIN(delimlen, ESC_LEN - 1)] = '\0';
			memcpy(dsocketptr->dollar_key, delimptr, MIN(delimlen, DD_BUFLEN - 1));
			dsocketptr->dollar_key[MIN(delimlen, DD_BUFLEN - 1)] = '\0';
		}
	}
	len = SIZEOF(ONE_COMMA) - 1;
	memcpy(dsocketptr->dollar_device, ONE_COMMA, len);
	memcpy(&dsocketptr->dollar_device[len], BADCHAR_DEVICE_MSG, SIZEOF(BADCHAR_DEVICE_MSG));
}
#endif

/* VMS uses the UCX interface; should support others that emulate it */
int	iosocket_readfl(mval *v, int4 width, int4 timeout)
	/* width == 0 is a flag for non-fixed length read */
					/* timeout in seconds */
{
	int		ret, byteperchar;
	boolean_t 	timed, vari, more_data, terminator, has_delimiter, requeue_done;
	boolean_t	zint_restart, outofband_terminate, one_read_done, utf8_active;
	int		flags, len, real_errno, save_errno, fcntl_res, errlen, charlen, stp_need;
	int		bytes_read, orig_bytes_read, ii, max_bufflen, bufflen, chars_read, mb_len, match_delim;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	int4		msec_timeout; /* timeout in milliseconds */
	TID		timer_id;
	ABS_TIME	cur_time, end_time, time_for_read, zero;
	char		*errptr;
	unsigned char	*buffptr, *c_ptr, *c_top, *inv_beg, *buffer_start;
	ssize_t		status;
	gtm_chset_t	ichset;
	mv_stent	*mv_zintdev;
	socket_interrupt *sockintr;

	error_def(ERR_IOEOF);
	error_def(ERR_TEXT);
	error_def(ERR_CURRSOCKOFR);
	error_def(ERR_NOSOCKETINDEV);
	error_def(ERR_GETSOCKOPTERR);
	error_def(ERR_SETSOCKOPTERR);
	error_def(ERR_MAXSTRLEN);
	error_def(ERR_BOMMISMATCH);
	error_def(ERR_ZINTRECURSEIO);
	error_def(ERR_STACKCRIT);
	error_def(ERR_STACKOFLOW);
	UNIX_ONLY(error_def(ERR_NOPRINCIO);)

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	iod = io_curr_device.in;
	ichset = iod->ichset;
	assert(dev_open == iod->state);
	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)(iod->dev_sp);
	if (0 >= dsocketptr->n_socket)
	{
		iod->dollar.za = 9;
		rts_error(VARLSTCNT(1) ERR_NOSOCKETINDEV);
		return 0;
	}
	if (dsocketptr->n_socket <= dsocketptr->current_socket)
	{
		iod->dollar.za = 9;
		rts_error(VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return 0;
	}
	utf8_active = NON_UNICODE_ONLY(FALSE) UNICODE_ONLY(gtm_utf8_mode ? IS_UTF_CHSET(ichset) : FALSE);
	byteperchar = UNICODE_ONLY(utf8_active ? GTM_MB_LEN_MAX :) 1;
	if (0 == width)
	{	/* op_readfl won't do this; must be a call from iosocket_read */
		vari = TRUE;
		width = MAX_STRLEN;
	} else
	{
		vari = FALSE;
		width = (width < MAX_STRLEN) ? width : MAX_STRLEN;
	}
	/* if width is set to MAX_STRLEN, we might be overly generous (assuming every char is just one byte) we must check if byte
	 * length crosses the MAX_STRLEN boundary */
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	assert(socketptr);
	sockintr = &dsocketptr->sock_save_state;
	assert(sockintr);
	outofband_terminate = FALSE;
	one_read_done = FALSE;
	zint_restart = FALSE;
	/* Check if new or resumed read */
	if (dsocketptr->mupintr)
	{	/* We have a pending read restart of some sort */
		if (sockwhich_invalid == sockintr->who_saved)
			GTMASSERT;      /* Interrupt should never have an invalid save state */
		/* check we aren't recursing on this device */
		if (dollar_zininterrupt)
			rts_error(VARLSTCNT(1) ERR_ZINTRECURSEIO);
                if (sockwhich_readfl != sockintr->who_saved)
                        GTMASSERT;      /* ZINTRECURSEIO should have caught */
		SOCKET_DEBUG(PRINTF("socrfl: *#*#*#*#*#*#*#  Restarted interrupted read\n"); DEBUGSOCKFLUSH);
		dsocketptr->mupintr = FALSE;
		sockintr->who_saved = sockwhich_invalid;
		mv_zintdev = io_find_mvstent(iod, FALSE);
		assert(mv_zintdev);
		if (mv_zintdev)
		{
			if (mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid)
			{
				bytes_read = mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.len;
				assert(bytes_read == sockintr->bytes_read);
				if (bytes_read)
				{
					buffer_start = (unsigned char *)mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr;
					zint_restart = TRUE;
				}
			} else
			{
				assert(FALSE);
				SOCKET_DEBUG(PRINTF("Evidence of an interrupt, but it was invalid\n"); DEBUGSOCKFLUSH);
			}
			/* Done with this mv_stent. Pop it off if we can, else mark it inactive. */
			if (mv_chain == mv_zintdev)
				POP_MV_STENT();         /* pop if top of stack */
			else
			{	/* else mark it unused */
				mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
				mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = 0;
				mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = NULL;
			}
		} else
		{
			sockintr->end_time_valid = FALSE;
			SOCKET_DEBUG(PRINTF("Evidence of an interrupt, but no MV_STENT\n"); DEBUGSOCKFLUSH);
		}
	} else
		sockintr->end_time_valid = FALSE;
	if (!zint_restart)
	{	/* Simple path (not restart or nothing read,) no worries*/
		/* compute the worst case byte requirement knowing that width is in chars */
		max_bufflen = (vari) ? MAX_STRBUFF_INIT
			: ((MAX_STRLEN > (width * byteperchar)) ? (width * byteperchar) : MAX_STRLEN);
		ENSURE_STP_FREE_SPACE(max_bufflen);
		bytes_read = 0;
		chars_read = 0;
	} else
	{
		max_bufflen = sockintr->max_bufflen;
		chars_read = sockintr->chars_read;
		assert(chars_read <= bytes_read);
		SOCKET_DEBUG2(PRINTF("socrfl: .. mv_stent found - bytes_read: %d  chars_read: %d  max_bufflen: %d"
			"  interrupts: %d\n", bytes_read, chars_read, max_bufflen, socketus_interruptus);
			DEBUGSOCKFLUSH);
		SOCKET_DEBUG(PRINTF("socrfl: .. timeout: %d\n", timeout); DEBUGSOCKFLUSH);
		SOCKET_DEBUG(if (sockintr->end_time_valid) PRINTF("socrfl: .. endtime: %d/%d\n", end_time.at_sec,
			end_time.at_usec); DEBUGSOCKFLUSH);
		SOCKET_DEBUG2(PRINTF("socrfl: .. buffer address: 0x%08lx  stringpool: 0x%08lx\n",
			buffer_start, stringpool.free); DEBUGSOCKFLUSH);
		if (stringpool.free != (buffer_start + bytes_read))	/* BYPASSOK */
		{	/* Not @ stringpool.free - must move what we have, so we need room for the whole anticipated message */
			SOCKET_DEBUG2(PRINTF("socrfl: .. Stuff put on string pool after our buffer\n"); DEBUGSOCKFLUSH);
			stp_need = max_bufflen;
		} else
		{	/* Previously read buffer piece is still last thing in stringpool, so we need room for the rest */
			SOCKET_DEBUG2(PRINTF("socrfl: .. Our buffer did not move in the stringpool\n"); DEBUGSOCKFLUSH);
			stp_need = max_bufflen - bytes_read;
			assert(stp_need < max_bufflen);
		}
		if (!IS_STP_SPACE_AVAILABLE(stp_need))
		{	/* need more room */
			SOCKET_DEBUG2(PRINTF("socrfl: .. garbage collection done in starting after interrupt\n"); DEBUGSOCKFLUSH);
			v->str.addr = (char *)buffer_start;	/* Protect buffer from reclaim */
			v->str.len = bytes_read;
			INVOKE_STP_GCOL(max_bufflen);
			buffer_start = (unsigned char *)v->str.addr;
		}
		if ((buffer_start + bytes_read) < stringpool.free)	/* BYPASSOK */
		{	/* now need to move it to the top */
			assert(stp_need == max_bufflen);
			memcpy(stringpool.free, buffer_start, bytes_read);
		} else
		{	/* it should still be just under the used space */
			assert((buffer_start + bytes_read) == stringpool.free);	/* BYPASSOK */
			stringpool.free = buffer_start;		/* backup the free pointer */
		}
		v->str.len = 0;		/* Clear incase interrupt or error -- don't want to hold this buffer */
		/* At this point, our previous buffer is sitting at stringpool.free and can be
		   expanded if necessary. This is what the rest of the code is expecting.
		*/
	}
	if (iod->dollar.x  &&  (TCP_WRITE == socketptr->lastop))
	{	/* switching from write to read */
		assert(!zint_restart);
		if (!iod->dollar.za)
			iosocket_flush(iod);
		iod->dollar.x = 0;
	}
	socketptr->lastop = TCP_READ;
	ret = TRUE;
	has_delimiter = (0 < socketptr->n_delimiter);
	time_for_read.at_sec  = 0;
	if (0 == timeout)
		time_for_read.at_usec = 0;
	else if (socketptr->def_moreread_timeout)
		time_for_read.at_usec = socketptr->moreread_timeout * 1000;
	else
		time_for_read.at_usec = INITIAL_MOREREAD_TIMEOUT * 1000;
	SOCKET_DEBUG(PRINTF("socrfl: moreread_timeout = %d def_moreread_timeout= %d time = %d \n",
			    socketptr->moreread_timeout,socketptr->def_moreread_timeout,time_for_read.at_usec); DEBUGSOCKFLUSH);
	timer_id = (TID)iosocket_readfl;
	out_of_time = FALSE;
	if (NO_M_TIMEOUT == timeout)
	{
		timed = FALSE;
		msec_timeout = NO_M_TIMEOUT;
		assert(!sockintr->end_time_valid);
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
				iod->dollar.za = 9;
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error(VARLSTCNT(7) ERR_GETSOCKOPTERR, 5, LEN_AND_LIT("F_GETFL FOR NON BLOCKING I/O"),
					  save_errno, LEN_AND_STR(errptr));
			}
			FCNTL3(socketptr->sd, F_SETFL, flags & (~(O_NDELAY | O_NONBLOCK)), fcntl_res);
			if (fcntl_res < 0)
			{
				iod->dollar.za = 9;
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("F_SETFL FOR NON BLOCKING I/O"),
					  save_errno, LEN_AND_STR(errptr));
			}
#endif
			sys_get_curr_time(&cur_time);
			if (!sockintr->end_time_valid)
				add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			else
			{	/* end_time taken from restart data. Compute what msec_timeout should be so timeout timer
				   gets set correctly below.
				*/
			 	end_time = sockintr->end_time;	 /* Restore end_time for timeout */
				cur_time = sub_abs_time(&end_time, &cur_time);
				if (0 > cur_time.at_sec)
				{
					msec_timeout = -1;
					out_of_time = TRUE;
				} else
					msec_timeout = (int4)(cur_time.at_sec * 1000 + cur_time.at_usec / 1000);
				SOCKET_DEBUG(PRINTF("socrfl: Taking timeout end time from read restart data - "
						    "computed msec_timeout: %d\n", msec_timeout); DEBUGSOCKFLUSH);
			}
			if (0 < msec_timeout)
				start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
		} else
		{
			out_of_time = TRUE;
			SOCKET_DEBUG(PRINTF("socrfl: No timeout specified, assuming out_of_time\n"));
		}
	}
	sockintr->end_time_valid = FALSE;
	dsocketptr->dollar_key[0] = '\0';
	iod->dollar.zb[0] = '\0';
	more_data = TRUE;
	real_errno = 0;
	requeue_done = FALSE;
	SOCKET_DEBUG(PRINTF("socrfl: ##################### Entering read loop ######################\n");
		      DEBUGSOCKFLUSH);
	for (status = 0;  status >= 0;)
	{
		SOCKET_DEBUG2(PRINTF("socrfl: ********* Top of read loop - bytes_read: %d  chars_read: %d  "
				     "buffered_length: %d\n", bytes_read, chars_read, socketptr->buffered_length); DEBUGSOCKFLUSH);
		SOCKET_DEBUG2(PRINTF("socrfl: ********* read-width: %d  vari: %d  status: %d\n", width, vari, status);
			      DEBUGSOCKFLUSH);
		if (bytes_read >= max_bufflen)
		{	/* more buffer needed. Extend the stringpool buffer by doubling the size as much as we
			   extended previously */
			SOCKET_DEBUG2(PRINTF("socrfl: Buffer expand1 bytes_read(%d) max_bufflen(%d)\n",
					     bytes_read, max_bufflen); DEBUGSOCKFLUSH);
			assert(vari);
			max_bufflen += max_bufflen;
			if (MAX_STRLEN < max_bufflen)
				max_bufflen = MAX_STRLEN;
			if (!IS_STP_SPACE_AVAILABLE(bytes_read + max_bufflen))
			{
				v->str.len = bytes_read; /* to keep the data read so far from being garbage collected */
				v->str.addr = (char *)stringpool.free;
				stringpool.free += v->str.len;
				assert(stringpool.free <= stringpool.top);
				INVOKE_STP_GCOL(max_bufflen);
				memcpy(stringpool.free, v->str.addr, v->str.len);
				v->str.len = 0; /* If interrupted, don't hold onto old space */
			}
		}
		if (has_delimiter || requeue_done || (socketptr->first_read && CHSET_M != ichset))
		{	/* Delimiter scanning needs one char at a time. Question is how big is a char?
			   For the UTF character sets, we have a similar issue (with the same solution) in that
			   we need to make sure the entire BOM we may have is in the buffer. If the last read
			   caused chars to be requeued, we have to make sure that we read in one full character
			   (buffer likely contains only a partial char) in order to make forward progess. If we
			   didn't do this, we would just pull the partial char out of the buffer, notice its incompleteness
			   and requeue it again and again. Forcing a full char read lets us continue past this point.
			*/
			requeue_done = FALSE;	/* housekeeping -- We don't want to come back here for this reason
						   until it happens again */
			SOCKET_DEBUG2(if (socketptr->first_read && CHSET_M != ichset)
				              PRINTF("socrfl: Prebuffering because ichset = UTF16\n");
				      else PRINTF("socrfl: Prebuffering because we have delimiter or requeue\n"); DEBUGSOCKFLUSH);
			if (CHSET_M == iod->ichset)
				bufflen = 1;		/* M mode, 1 char == 1 byte */
			else
			{	/* We need to make sure the read buffer contains a complete UTF character and to make sure
				   we know how long that character is so we can read it completely. The issue is that if we
				   read a partial utf character, the utf checking code below will notice this and rebuffer it
				   so that it gets re-read on the next iteration. However, this will then re-read the same
				   partial character and we have a loop. We make a call here which will take a peek at the
				   first byte of the next character (and read it in if necessary), determine how long the
				   character is and verify that many characters are available in the buffer and return the
				   character length to us to use for bufflen.
				*/
				charlen = (int)iosocket_snr_utf_prebuffer(iod, socketptr, 0, &time_for_read,
								     (!vari || has_delimiter || 0 == chars_read));
				SOCKET_DEBUG2(PRINTF("socrfl: charlen from iosocket_snr_utf_prebuffer = %d\n", charlen);
					     DEBUGSOCKFLUSH);
				if (0 < charlen)
				{	/* We know how long the next char is. If it will fit in our buffer, then it is
					   the correct bufflen. If it won't, our buffer is full and we need to expand.
					*/
					if ((max_bufflen - bytes_read) < charlen)
					{
						SOCKET_DEBUG2(PRINTF("socrfl: Buffer expand2 - max_bufflen(%d) "
								    "bytes_read(%d)\n",
								    max_bufflen, bytes_read); DEBUGSOCKFLUSH);
						assert(vari);
						max_bufflen += max_bufflen;
						if (MAX_STRLEN < max_bufflen)
							max_bufflen = MAX_STRLEN;
						if (!IS_STP_SPACE_AVAILABLE(bytes_read + max_bufflen))
						{
							v->str.len = bytes_read; /* to keep the data read so far from
										    being garbage collected */
							v->str.addr = (char *)stringpool.free;
							stringpool.free += bytes_read;
							assert(stringpool.free <= stringpool.top);
							INVOKE_STP_GCOL(max_bufflen);
							memcpy(stringpool.free, v->str.addr, v->str.len);
							v->str.len = 0; /* If interrupted, don't hold onto old space */
						}
					}
					bufflen = charlen;
				} else if (0 == charlen)
				{	/* No data was available or there was a timeout. We set status to -3 here
					   so that we end up bypassing the call to iosocket_snr below which was to
					   do the actual IO. No need to repeat our lack of IO issue.
					*/
					SOCKET_DEBUG(PRINTF("socrfl: Timeout or end of data stream\n"); DEBUGSOCKFLUSH);
					status = -3;		/* To differentiate from status=0 if prebuffer bypassed */
					DEBUG_ONLY(bufflen = 0);  /* Triggers assert in iosocket_snr if we end up there anyway */
				} else
				{	/* Something bad happened. Feed the error to the lower levels for proper handling */
					SOCKET_DEBUG2(PRINTF("socrfl: Read error: status: %d  errno: %d\n", charlen, errno);
						     DEBUGSOCKFLUSH);
					status = charlen;
                                        DEBUG_ONLY(bufflen = 0);  /* Triggers assert in iosocket_snr if we end up there anyway */
				}
			}
		} else
		{
			bufflen = max_bufflen - bytes_read;
			SOCKET_DEBUG2(PRINTF("socrfl: Regular read .. bufflen = %d\n", bufflen); DEBUGSOCKFLUSH);
			status = 0;
		}
		buffptr = (stringpool.free + bytes_read);
		terminator = FALSE;
		if (0 <= status)
			/* An IO above in prebuffering may have failed in which case we do not want to reset status */
			status = iosocket_snr(socketptr, buffptr, bufflen, 0, &time_for_read);
		else
		{
			SOCKET_DEBUG2(PRINTF("socrfl: Data read bypassed - status: %d\n", status); DEBUGSOCKFLUSH);
		}
		if (0 == status || -3 == status)	/* -3 status can happen on EOB from prebuffering */
		{
			SOCKET_DEBUG2(PRINTF("socrfl: No more data available\n"); DEBUGSOCKFLUSH);
			more_data = FALSE;
			status = 0;			/* Consistent treatment of no more data */
		} else if (0 < status)
		{
			if (timeout && !socketptr->def_moreread_timeout && !one_read_done)
			{
				one_read_done = TRUE;
				SOCKET_DEBUG(PRINTF("socrfl: before moreread_timeout = %d timeout = %d \n",
						    socketptr->moreread_timeout,time_for_read.at_usec); DEBUGSOCKFLUSH);
				time_for_read.at_usec = DEFAULT_MOREREAD_TIMEOUT * 1000;
				SOCKET_DEBUG(PRINTF("socrfl: after timeout = %d \n",time_for_read.at_usec); DEBUGSOCKFLUSH);
			}
			SOCKET_DEBUG2(PRINTF("socrfl: Bytes read: %d\n", status); DEBUGSOCKFLUSH);
			bytes_read += (int)status;
			UNIX_ONLY(if (iod == io_std_device.out)
				prin_in_dev_failure = FALSE;)
			if (socketptr->first_read && CHSET_M != ichset) /* May have a BOM to defuse */
			{
				if (CHSET_UTF8 != ichset)
				{	/* When the type is UTF16xx, we need to check for a BOM at the beginning of the file. If
					   found it will tell us which of UTF-16BE (default if no BOM) or UTF-16LE mode the data
					   is being written with. The call to iosocket_snr_utf_prebuffer() above should have made
					   sure that there were at least two chars available in the buffer if the char is a BOM.
					*/
					if (UTF16BE_BOM_LEN <= bytes_read)	/* All UTF16xx BOM lengths are the same */
					{
						if (0 == memcmp(buffptr, UTF16BE_BOM, UTF16BE_BOM_LEN))
						{
							if (CHSET_UTF16LE == ichset)
							{
								iod->dollar.za = 9;
								rts_error(VARLSTCNT(6) ERR_BOMMISMATCH, 4,
									  chset_names[CHSET_UTF16BE].len,
									  chset_names[CHSET_UTF16BE].addr,
									  chset_names[CHSET_UTF16LE].len,
									  chset_names[CHSET_UTF16LE].addr);
							} else
							{
								iod->ichset = ichset = CHSET_UTF16BE;
								bytes_read -= UTF16BE_BOM_LEN;	/* Throw way BOM */
								SOCKET_DEBUG2(PRINTF("socrfl: UTF16BE BOM detected\n");
									     DEBUGSOCKFLUSH);
							}
						} else if (0 == memcmp(buffptr, UTF16LE_BOM, UTF16LE_BOM_LEN))
                                                {
                                                        if (CHSET_UTF16BE == ichset)
							{
								iod->dollar.za = 9;
                                                                rts_error(VARLSTCNT(6) ERR_BOMMISMATCH, 4,
                                                                          chset_names[CHSET_UTF16LE].len,
                                                                          chset_names[CHSET_UTF16LE].addr,
                                                                          chset_names[CHSET_UTF16BE].len,
                                                                          chset_names[CHSET_UTF16BE].addr);
							} else
							{
                                                                iod->ichset = ichset = CHSET_UTF16LE;
								bytes_read -= UTF16BE_BOM_LEN;	/* Throw away BOM */
                                                                SOCKET_DEBUG2(PRINTF("socrfl: UTF16LE BOM detected\n");
                                                                             DEBUGSOCKFLUSH);
                                                        }
						} else
						{	/* No BOM specified. If UTF16, default BOM to UTF16BE per Unicode
							   standard
							*/
							if (CHSET_UTF16 == ichset)
							{
								SOCKET_DEBUG2(PRINTF("socrfl: UTF16BE BOM assumed\n");
									     DEBUGSOCKFLUSH);
								iod->ichset = ichset = CHSET_UTF16BE;
							}
						}
					} else
					{	/* Insufficient characters to form a BOM so no BOM present. Like above, if in
						   UTF16 mode, default to UTF16BE per the Unicode standard.
						*/
						if (CHSET_UTF16 == ichset)
						{
							SOCKET_DEBUG2(PRINTF("socrfl: UTF16BE BOM assumed\n");
								     DEBUGSOCKFLUSH);
							iod->ichset = ichset = CHSET_UTF16BE;
						}
					}
				} else
				{	/* Check for UTF8 BOM. If found, just eliminate it. */
                                        if (UTF8_BOM_LEN <= bytes_read && (0 == memcmp(buffptr, UTF8_BOM, UTF8_BOM_LEN)))
					{
						bytes_read -= UTF8_BOM_LEN;        /* Throw way BOM */
						SOCKET_DEBUG2(PRINTF("socrfl: UTF8 BOM detected/ignored\n");
							     DEBUGSOCKFLUSH);
					}
				}
			}
			if (socketptr->first_read)
			{
				if  (CHSET_UTF16BE == ichset || CHSET_UTF16LE == ichset)
				{
					get_chset_desc(&chset_names[ichset]);
					if (has_delimiter)
						iosocket_delim_conv(socketptr, ichset);
				}
				socketptr->first_read = FALSE;
			}
			if (bytes_read && has_delimiter)
			{ /* ------- check to see if it is a delimiter -------- */
				SOCKET_DEBUG2(PRINTF("socrfl: Searching for delimiter\n"); DEBUGSOCKFLUSH);
				for (ii = 0; ii < socketptr->n_delimiter; ii++)
				{
					if (bytes_read < socketptr->idelimiter[ii].len)
						continue;
					if (0 == memcmp(socketptr->idelimiter[ii].addr,
							stringpool.free + bytes_read - socketptr->idelimiter[ii].len,
							socketptr->idelimiter[ii].len))
					{
						terminator = TRUE;
						match_delim = ii;
						memcpy(iod->dollar.zb, socketptr->idelimiter[ii].addr,
						       MIN(socketptr->idelimiter[ii].len, ESC_LEN - 1));
						iod->dollar.zb[MIN(socketptr->idelimiter[ii].len, ESC_LEN - 1)] = '\0';
						memcpy(dsocketptr->dollar_key, socketptr->idelimiter[ii].addr,
						       MIN(socketptr->idelimiter[ii].len, DD_BUFLEN - 1));
						dsocketptr->dollar_key[MIN(socketptr->idelimiter[ii].len, DD_BUFLEN - 1)] = '\0';
						break;
					}
				}
				SOCKET_DEBUG2(
				if (terminator)
					PRINTF("socrfl: Delimiter found - match_delim: %d\n", match_delim);
				else
				        PRINTF("socrfl: Delimiter not found\n");
				DEBUGSOCKFLUSH;
				);
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
		if (bytes_read > MAX_STRLEN)
		{
			iod->dollar.za = 9;
			rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
		}
		orig_bytes_read = bytes_read;
		if (0 != bytes_read)
		{	/* find n chars read from [buffptr, buffptr + bytes_read) */
			SOCKET_DEBUG2(PRINTF("socrfl: Start char scan - c_ptr: 0x%08lx  c_top: 0x%08lx\n",
					     buffptr, (buffptr + status)); DEBUGSOCKFLUSH);
			for (c_ptr = buffptr, c_top = buffptr + status;
			     c_ptr < c_top && chars_read < width;
			     c_ptr += mb_len, chars_read++)
			{
				mb_len = 1;	/* In case of CHSET_M */
				if (!((CHSET_M == ichset) ? 1 :
				      (CHSET_UTF8 == ichset) ? UTF8_VALID(c_ptr, c_top, mb_len) :
				      (CHSET_UTF16BE == ichset) ? UTF16BE_VALID(c_ptr, c_top, mb_len) :
				      UTF16LE_VALID(c_ptr, c_top, mb_len)))
				{	/* This char is not valid unicode but this is only an error if entire char is
					   in the buffer. Else we will ignore it and it will be rebuffered further down.
					   First, we need to find its (real) length as xx_VALID set it to one when it
					   was determined to be invalid.
					*/
					mb_len = (CHSET_M == ichset) ? 0 :
						(CHSET_UTF8 == ichset) ? UTF8_MBFOLLOW(c_ptr) :
						(CHSET_UTF16BE == ichset) ? UTF16BE_MBFOLLOW(c_ptr, c_top) :
						UTF16LE_MBFOLLOW(c_ptr, c_top);
					mb_len++;	/* Account for first byte of char */
					if (0 == mb_len || c_ptr + mb_len <= c_top)
					{	/* The entire char is in the buffer.. badchar */
#ifdef UNICODE_SUPPORTED
						if (CHSET_UTF8 == ichset)
						{
							iosocket_readfl_badchar(v, (int)((unsigned char *)c_ptr - stringpool.free),
										0, c_ptr, c_top);
							UTF8_BADCHAR(0, c_ptr, c_top, 0, NULL);
						} else /* UTF16LE or UTF16BE */
						{
							inv_beg = c_ptr;
							if ((c_ptr += 2) >= c_top)
								c_ptr = c_top;
							iosocket_readfl_badchar(v,
										(int)((unsigned char *)inv_beg - stringpool.free),
										(int)(c_ptr - inv_beg), inv_beg, c_top);
							UTF8_BADCHAR((int)(c_ptr - inv_beg), inv_beg, c_top,
								     chset_names[ichset].len, chset_names[ichset].addr);
						}
#endif
					}
				}
				if (c_ptr + mb_len > c_top)	/* Verify entire char is in buffer */
					break;
			}
                        SOCKET_DEBUG2(PRINTF("socrfl: End char scan - c_ptr: 0x%08lx  c_top: 0x%08lx\n",
                                            c_ptr, c_top); DEBUGSOCKFLUSH);
			if (c_ptr < c_top) /* width size READ completed OR partial last char, push back bytes into input buffer */
			{
				iosocket_unsnr(socketptr, c_ptr, c_top - c_ptr);
				bytes_read -= (int)(c_top - c_ptr);	/* We will be re-reading these bytes */
				requeue_done = TRUE;		/* Force single (full) char read next time through */
				SOCKET_DEBUG2(PRINTF("socrfl: Requeue of %d bytes done - adjusted bytes_read: %d\n",
						    (c_top - c_ptr), bytes_read); DEBUGSOCKFLUSH);
			}
		}
		if (terminator)
		{
			assert(0 != bytes_read);
			bytes_read -= socketptr->idelimiter[match_delim].len;
			c_ptr -= socketptr->idelimiter[match_delim].len;
			UNICODE_ONLY(chars_read -= socketptr->idelimiter[match_delim].char_len);
			NON_UNICODE_ONLY(chars_read = bytes_read);
			SOCKET_DEBUG2(PRINTF("socrfl: Terminator found - bytes_read reduced by %d bytes to %d\n",
					     socketptr->idelimiter[match_delim].len, bytes_read); DEBUGSOCKFLUSH);
			SOCKET_DEBUG2(PRINTF("socrfl: .. c_ptr also reduced to 0x%08lx\n", c_ptr); DEBUGSOCKFLUSH);
		}
		/* If we read as much as we needed or if the buffer was totally full (last char or 3 might be part of an
		   incomplete character than can never be completed in this buffer) or if variable length, no delim with
		   chars available and no more data or outofband or have data including a terminator, we are then done. Note
		   that we are explicitly not handling jobinterrupt outofband here because it is handled above where it needs
		   to be done to be able to cleanly requeue any input (before delimiter procesing).
		*/
		if ((chars_read >= width) ||
		    (MAX_STRLEN <= orig_bytes_read) ||
		    (vari && !has_delimiter && 0 != chars_read && !more_data) ||
		    (status > 0 && terminator))
			break;
		if (0 != outofband)
		{
			outofband_terminate = TRUE;
			break;
		}
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
					SOCKET_DEBUG(PRINTF("socrfl: Out of time detected and set\n"));
					break;
				}
			} else if (!more_data)
				break;
		}
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
				iod->dollar.za = 9;
				save_errno = errno;
				errptr = (char *)STRERROR(errno);
				rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("F_SETFL FOR RESTORING SOCKET OPTIONS"),
					  save_errno, LEN_AND_STR(errptr));
			}
#endif
			if (out_of_time)
			{
				ret = FALSE;
				SOCKET_DEBUG(PRINTF("socrfl: Out of time to be returned (1)\n"));
			} else
				cancel_timer(timer_id);
		} else if ((chars_read < width) && !(has_delimiter && terminator))
		{
			ret = FALSE;
			SOCKET_DEBUG(PRINTF("socrfl: Out of time to be returned (2)\n"));
		}
	}
	/* If we terminated due to outofband, set up restart info. We may or may not restart (any outofband is capable of
	   restart) but set it up for at least the more common reasons (^C and job interrupt).

	   Some restart info is kept in our iodesc block, but the buffer address information is kept in an mv_stent so if
	   the stack is garbage collected during the interrupt we don't lose track of where our stuff is saved away.
	*/
	if (outofband_terminate)
	{
		SOCKET_DEBUG(PRINTF("socrfl: outofband interrupt received (%d) -- queueing mv_stent for read intr\n", outofband);
			     DEBUGSOCKFLUSH);
		PUSH_MV_STENT(MVST_ZINTDEV);
		mv_chain->mv_st_cont.mvs_zintdev.io_ptr = iod;
		mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
		mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = (char *)stringpool.free;
		mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = bytes_read;
		sockintr->who_saved = sockwhich_readfl;
		if (0 < msec_timeout && NO_M_TIMEOUT != msec_timeout)
		{
			sockintr->end_time = end_time;
			sockintr->end_time_valid = TRUE;
			cancel_timer(timer_id);		/* Worry about timer if/when we come back */
		}
		sockintr->max_bufflen = max_bufflen;
		sockintr->bytes_read = bytes_read;
		sockintr->chars_read = chars_read;
		dsocketptr->mupintr = TRUE;
		stringpool.free += bytes_read;	/* Don't step on our parade in the interrupt */
		socketus_interruptus++;
		SOCKET_DEBUG2(PRINTF("socrfl: .. mv_stent: bytes_read: %d  chars_read: %d  max_bufflen: %d  "
				    "interrupts: %d  buffer_start: 0x%08lx\n",
				    bytes_read, chars_read, max_bufflen, socketus_interruptus, stringpool.free); DEBUGSOCKFLUSH);
		SOCKET_DEBUG(if (sockintr->end_time_valid) PRINTF("socrfl: .. endtime: %d/%d  timeout: %d  msec_timeout: %d\n",
								  end_time.at_sec, end_time.at_usec, timeout, msec_timeout);
			     DEBUGSOCKFLUSH);
		outofband_action(FALSE);
		GTMASSERT;	/* Should *never* return from outofband_action */
		return FALSE;	/* For the compiler.. */
	}
	if (chars_read > 0)
	{	/* there's something to return */
		v->str.len = INTCAST(c_ptr - stringpool.free);
		v->str.addr = (char *)stringpool.free;
		UNICODE_ONLY(v->str.char_len = chars_read);
		assert(v->str.len == bytes_read);
		SOCKET_DEBUG(PRINTF("socrfl: String to return bytelen: %d  charlen: %d  iod-width: %d  wrap: %d\n",
				    v->str.len, chars_read, iod->width, iod->wrap); DEBUGSOCKFLUSH);
		SOCKET_DEBUG2(PRINTF("socrfl:   x: %d  y: %d\n", iod->dollar.x, iod->dollar.y); DEBUGSOCKFLUSH);
		if (((iod->dollar.x += chars_read) >= iod->width) && iod->wrap)
		{
			iod->dollar.y += (iod->dollar.x / iod->width);
			if (0 != iod->length)
				iod->dollar.y %= iod->length;
			iod->dollar.x %= iod->width;
		}
		if (CHSET_M != ichset && CHSET_UTF8 != ichset)
		{
			SOCKET_DEBUG2(PRINTF("socrfl: Converting UTF16xx data back to UTF8 for internal use\n"); DEBUGSOCKFLUSH);
			v->str.len = gtm_conv(chset_desc[ichset], chset_desc[CHSET_UTF8], &v->str, NULL, NULL);
			v->str.addr = (char *)stringpool.free;
			stringpool.free += v->str.len;
		}

	} else
	{
		v->str.len = 0;
		v->str.addr = dsocketptr->dollar_key;
	}
	if (status >= 0)
	{	/* no real problems */
		iod->dollar.zeof = FALSE;
		iod->dollar.za = 0;
		memcpy(dsocketptr->dollar_device, "0", SIZEOF("0"));
	} else
	{	/* there's a significant problem */
		SOCKET_DEBUG(PRINTF("socrfl: Error handling triggered - status: %d\n", status); DEBUGSOCKFLUSH);
		if (0 == chars_read)
			iod->dollar.x = 0;
		iod->dollar.za = 9;
		len = SIZEOF(ONE_COMMA) - 1;
		memcpy(dsocketptr->dollar_device, ONE_COMMA, len);
		errptr = (char *)STRERROR(real_errno);
		errlen = STRLEN(errptr);
		memcpy(&dsocketptr->dollar_device[len], errptr, errlen + 1);	/* + 1 for null */
#ifdef UNIX
		if (io_curr_device.in == io_std_device.in)
		{
			if (!prin_in_dev_failure)
				prin_in_dev_failure = TRUE;
			else
			{
				send_msg(VARLSTCNT(1) ERR_NOPRINCIO);
					stop_image_no_core();
			}
		}
#endif
		if (iod->dollar.zeof || -1 == status || 0 < iod->error_handler.len)
		{
			iod->dollar.zeof = TRUE;
			if (socketptr->ioerror)
				rts_error(VARLSTCNT(6) ERR_IOEOF, 0, ERR_TEXT, 2, errlen, errptr);
		} else
			iod->dollar.zeof = TRUE;
	}
	SOCKET_DEBUG(if (!ret && out_of_time) PRINTF("socrfl: Returning from read due to timeout\n");
		     else PRINTF("socrfl: Returning from read with success indicator set to %d\n", ret));
	SOCKET_DEBUG(fflush(stdout));
	return (ret);
}
