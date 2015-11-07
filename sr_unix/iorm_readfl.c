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
#include "gtm_fcntl.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iotimer.h"
#include "iormdef.h"
#include "stringpool.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "have_crit.h"
#include "eintr_wrappers.h"
#include "wake_alarm.h"
#include "min_max.h"
#include "outofband.h"
#include "mv_stent.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_conv.h"
#include "gtm_utf8.h"
#endif

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	volatile bool	out_of_time;
GBLREF  boolean_t       gtm_utf8_mode;
GBLREF	volatile int4		outofband;
GBLREF	mv_stent         	*mv_chain;
GBLREF  boolean_t       	dollar_zininterrupt;
#ifdef UNICODE_SUPPORTED
LITREF	UChar32		u32_line_term[];
LITREF	mstr		chset_names[];
GBLREF	UConverter	*chset_desc[];
#endif
error_def(ERR_IOEOF);
error_def(ERR_SYSCALL);
error_def(ERR_ZINTRECURSEIO);
error_def(ERR_DEVICEWRITEONLY);

#define fl_copy(a, b) (a > b ? b : a)

#define SETZACANCELTIMER					\
		io_ptr->dollar.za = 9;				\
		v->str.len = 0;					\
		if (timed && !out_of_time)			\
			cancel_timer(timer_id);

#ifdef UNICODE_SUPPORTED
/* Maintenance of $ZB on a badchar error and returning partial data (if any) */
void iorm_readfl_badchar(mval *vmvalptr, int datalen, int delimlen, unsigned char *delimptr, unsigned char *strend)
{
	int             tmplen, len;
	unsigned char   *delimend;
	io_desc         *iod;
	d_rm_struct	*rm_ptr;

	assert(0 <= datalen);
	iod = io_curr_device.in;
	rm_ptr = (d_rm_struct *)(iod->dev_sp);
	assert(NULL != rm_ptr);
	vmvalptr->str.len = datalen;
	vmvalptr->str.addr = (char *)stringpool.free;
        if (0 < datalen)
		/* Return how much input we got */
		stringpool.free += vmvalptr->str.len;

        if (NULL != strend && NULL != delimptr)
        {       /* First find the end of the delimiter (max of 4 bytes) */
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
			memcpy(iod->dollar.key, delimptr, MIN(delimlen, DD_BUFLEN - 1));
			iod->dollar.key[MIN(delimlen, DD_BUFLEN - 1)] = '\0';
                }
        }
	/* set dollar.device in the output device */
	len = SIZEOF(ONE_COMMA) - 1;
	memcpy(iod->dollar.device, ONE_COMMA, len);
	memcpy(&iod->dollar.device[len], BADCHAR_DEVICE_MSG, SIZEOF(BADCHAR_DEVICE_MSG));
}
#endif

int	iorm_readfl (mval *v, int4 width, int4 timeout) /* timeout in seconds */
{
	boolean_t	ret, timed, utf_active, line_term_seen = FALSE, rdone = FALSE, zint_restart;
	char		inchar, *temp, *temp_start;
	unsigned char	*nextmb, *char_ptr, *char_start, *buffer_start;
	int		flags = 0;
	int		len;
	int		errlen, real_errno;
	int		fcntl_res, stp_need;
	int4		msec_timeout;	/* timeout in milliseconds */
	int4		bytes2read, bytes_read, char_bytes_read, add_bytes, reclen;
	int4		buff_len, mblen, char_count, bytes_count, tot_bytes_read, utf_tot_bytes_read;
	int4		status, max_width, ltind, exp_width, from_bom;
	wint_t		utf_code;
	char		*errptr;
	io_desc		*io_ptr;
	d_rm_struct	*rm_ptr;
	gtm_chset_t	chset;
	TID		timer_id;
	int		fildes;
	FILE		*filstr;
	boolean_t	pipe_zero_timeout = FALSE;
	boolean_t	pipe_or_fifo = FALSE;
	boolean_t	follow_timeout = FALSE;
	boolean_t	bom_timeout = FALSE;
	int		blocked_in = TRUE;
	int		do_clearerr = FALSE;
	int		saved_lastop;
	int             min_bytes_to_copy;
	ABS_TIME	cur_time, end_time, time_for_read;
	pipe_interrupt	*pipeintr;
	mv_stent	*mv_zintdev;
	unsigned int	*dollarx_ptr;
	unsigned int	*dollary_ptr;
	struct timeval	poll_interval;
	int		poll_status;
	fd_set		input_fds;
	int4 sleep_left;
	int4 sleep_time;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

#ifdef DEBUG_PIPE
	pid=getpid();
#endif
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);

	io_ptr = io_curr_device.in;
	/* don't allow a read from a writeonly fifo */
	if (((d_rm_struct *)io_ptr->dev_sp)->write_only)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVICEWRITEONLY);
#ifdef __MVS__
	/* on zos if it is a fifo device then point to the pair.out for $X and $Y */
	if (((d_rm_struct *)io_ptr->dev_sp)->fifo)
	{
		dollarx_ptr = &(io_ptr->pair.out->dollar.x);
		dollary_ptr = &(io_ptr->pair.out->dollar.y);
	} else
#endif
	{
		dollarx_ptr = &(io_ptr->dollar.x);
		dollary_ptr = &(io_ptr->dollar.y);
	}
	assert (io_ptr->state == dev_open);
	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	assert(NULL != rm_ptr);
	pipeintr = &rm_ptr->pipe_save_state;
	if (rm_ptr->pipe || rm_ptr->fifo)
		pipe_or_fifo = TRUE;

	PIPE_DEBUG(PRINTF(" %d enter iorm_readfl\n",pid); DEBUGPIPEFLUSH);
	/* if it is a pipe and it's the stdout returned then we need to get the read file descriptor
	   from rm_ptr->read_fildes and the stream pointer from rm_ptr->read_filstr */
	if ((rm_ptr->pipe ZOS_ONLY(|| rm_ptr->fifo)) && (0 < rm_ptr->read_fildes))
	{
		assert(rm_ptr->read_filstr);
		fildes = rm_ptr->read_fildes;
		filstr = rm_ptr->read_filstr;
	} else
	{
		fildes = rm_ptr->fildes;
		filstr = rm_ptr->filstr;
	}

	utf_active = gtm_utf8_mode ? (IS_UTF_CHSET(io_ptr->ichset)) : FALSE;
	/* If the last operation was a write and $X is non-zero we may have to call iorm_wteol() */
	if (*dollarx_ptr && rm_ptr->lastop == RM_WRITE)
	{
		/* don't need to flush the pipe device for a streaming read */
		/* Fixed mode read may output pad characters in iorm_wteol() for all device types */
		if (!io_ptr->dollar.za && (!rm_ptr->pipe || rm_ptr->fixed))
			iorm_wteol(1, io_ptr);
		*dollarx_ptr = 0;
	}

	/* if it's a fifo and not system input and the last operation was a write and O_NONBLOCK is set then
	   turn if off.  A write will turn it on.  The default is RM_NOOP. */
	if (rm_ptr->fifo && (0 != rm_ptr->fildes) && (RM_WRITE == rm_ptr->lastop))
	{
		flags = 0;
		FCNTL2(rm_ptr->fildes, F_GETFL, flags);
		if (0 > flags)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM, errno);
		if (flags & O_NONBLOCK)
		{
			FCNTL3(rm_ptr->fildes, F_SETFL, (flags & ~O_NONBLOCK), fcntl_res);
			if (0 > fcntl_res)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM, errno);
		}
	}

	zint_restart = FALSE;
	/* Check if new or resumed read */
	if (rm_ptr->mupintr)
	{	/* We have a pending read restart of some sort */
		if (pipewhich_invalid == pipeintr->who_saved)
			GTMASSERT;      /* Interrupt should never have an invalid save state */
		/* check we aren't recursing on this device */
		if (dollar_zininterrupt)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
                if (pipewhich_readfl != pipeintr->who_saved)
                        GTMASSERT;      /* ZINTRECURSEIO should have caught */
		PIPE_DEBUG(PRINTF("piperfl: *#*#*#*#*#*#*#  Restarted interrupted read\n"); DEBUGPIPEFLUSH);
		mv_zintdev = io_find_mvstent(io_ptr, FALSE);
		if (mv_zintdev)
		{
			if (mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid)
			{
				assert(mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.len == pipeintr->bytes_read);
				buffer_start = (unsigned char *)mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr;
				zint_restart = TRUE;
			} else
			{
				PIPE_DEBUG(PRINTF("Evidence of an interrupt, but it was invalid\n"); DEBUGPIPEFLUSH);
				assert(FALSE);
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
			pipeintr->end_time_valid = FALSE;
                        PIPE_DEBUG(PRINTF("Evidence of an interrupt, but no MV_STENT\n"); DEBUGPIPEFLUSH);
		}
		rm_ptr->mupintr = FALSE;
		pipeintr->who_saved = pipewhich_invalid;
	} else
		pipeintr->end_time_valid = FALSE;

	/* save the lastop for zeof test later */
	saved_lastop = rm_ptr->lastop;
	rm_ptr->lastop = RM_READ;
	timer_id = (TID)iorm_readfl;
	max_width = io_ptr->width - *dollarx_ptr;
	if (0 == width)
	{
		width = io_ptr->width;		/* called from iorm_read */
		if (!utf_active || !rm_ptr->fixed)
			max_width = width;	/* preserve prior functionality */
	} else if (-1 == width)
	{
		rdone = TRUE;			/* called from iorm_rdone */
		width = 1;
	}
	width = (width < max_width) ? width : max_width;
	tot_bytes_read = char_bytes_read = char_count = bytes_count = 0;
	ret = TRUE;
	if (!zint_restart)
	{	/* Simple path (not restart or nothing read,) no worries*/
		/* compute the worst case byte requirement knowing that width is in chars */
		/* if utf_active, need room for multi byte characters */
		exp_width = utf_active ? (GTM_MB_LEN_MAX * width) : width;
		ENSURE_STP_FREE_SPACE(exp_width);
		temp = (char *)stringpool.free;
	} else
	{
		exp_width = pipeintr->max_bufflen;
		bytes_read = pipeintr->bytes_read;
		/* some locals needed by unicode streaming mode */
		if (utf_active && !rm_ptr->fixed)
		{
			bytes2read = pipeintr->bytes2read;
			char_count = pipeintr->char_count;
			bytes_count = pipeintr->bytes_count;
			add_bytes = pipeintr->add_bytes;
		}

		PIPE_DEBUG(PRINTF("piperfl: .. mv_stent found - bytes_read: %d max_bufflen: %d"
				  "  interrupts: %d\n", bytes_read, exp_width, TREF(pipefifo_interrupt)); DEBUGPIPEFLUSH);
		PIPE_DEBUG(PRINTF("piperfl: .. timeout: %d\n", timeout); DEBUGPIPEFLUSH);
		PIPE_DEBUG(if (pipeintr->end_time_valid) PRINTF("piperfl: .. endtime: %d/%d\n", end_time.at_sec,
								end_time.at_usec); DEBUGPIPEFLUSH);
		PIPE_DEBUG(PRINTF("piperfl: .. buffer address: 0x%08lx  stringpool: 0x%08lx\n",
				  buffer_start, stringpool.free); DEBUGPIPEFLUSH);
		PIPE_DEBUG(PRINTF("buffer_start =%s\n",buffer_start); DEBUGPIPEFLUSH);
		/* If it is fixed and utf mode then we are not doing any mods affecting stringpool during the read and
		   don't use temp, so skip the following stringpool checks */
		if (!utf_active || !rm_ptr->fixed)
		{
			if (stringpool.free != (buffer_start + bytes_read)) /* BYPASSOK */
			{	/* Not @ stringpool.free - must move what we have, so we need room for
				   the whole anticipated message */
				PIPE_DEBUG(PRINTF("socrfl: .. Stuff put on string pool after our buffer\n"); DEBUGPIPEFLUSH);
				stp_need = exp_width;
			} else
			{	/* Previously read buffer piece is still last thing in stringpool, so we need room for the rest */
				PIPE_DEBUG(PRINTF("piperfl: .. Our buffer did not move in the stringpool\n"); DEBUGPIPEFLUSH);
				stp_need = exp_width - bytes_read;
				assert(stp_need <= exp_width);
			}
			if (!IS_STP_SPACE_AVAILABLE(stp_need))
			{	/* need more room */
				PIPE_DEBUG(PRINTF("piperfl: .. garbage collection done in starting after interrrupt\n");
					   DEBUGPIPEFLUSH);
				v->str.addr = (char *)buffer_start;	/* Protect buffer from reclaim */
				v->str.len = bytes_read;
				INVOKE_STP_GCOL(exp_width);
				/* if v->str.len is 0 then v->str.add is ignored by garbage collection so reset it to
				   stringpool.free */
				if (v->str.len == 0)
					v->str.addr =  (char *)stringpool.free;
				buffer_start = (unsigned char *)v->str.addr;
			}
			if ((buffer_start + bytes_read) < stringpool.free)	/* BYPASSOK */
			{	/* now need to move it to the top */
				assert(stp_need == exp_width);
				memcpy(stringpool.free, buffer_start, bytes_read);
			} else
			{	/* it should still be just under the used space */
				assert(!bytes_read || ((buffer_start + bytes_read) == stringpool.free));	/* BYPASSOK */
				stringpool.free = buffer_start;
			}
			v->str.len = 0;		/* Clear in case interrupt or error -- don't want to hold this buffer */

			temp = (char *)(stringpool.free + bytes_read);
			tot_bytes_read = bytes_count = bytes_read;
			if (!(rm_ptr->fixed && rm_ptr->follow))
				width -= bytes_read;
		}
	}

        if (utf_active)
		bytes_read = 0;

	out_of_time = FALSE;
	if (timeout == NO_M_TIMEOUT)
	{
		timed = FALSE;
		msec_timeout = NO_M_TIMEOUT;
		assert(!pipeintr->end_time_valid);
	} else
	{
		/* For timed input, only one timer will be set in this routine.  The first case is for a read x:n
		   and the second case is potentially for the pipe device doing a read x:0.  If a timer is set, the
		   out_of_time variable will start as FALSE.  The out_of_time variable will change from FALSE
		   to TRUE if the timer exires prior to a read completing. For the read x:0 case for a pipe, an attempt
		   is made to read one character in non-blocking mode.  If it succeeds then the pipe is set to
		   blocking mode and a timer is set.  In addition, the blocked_in variable is set to TRUE to prevent
		   doing this a second time.  If a timer is set, it is checked at the end of this routine
		   under the "if (timed)" clause.  The timed variable is set to true for both read x:n and x:0, but
		   msec_timeout will be 0 for read x:0 case unless it's a pipe and has read one character and started the 1 sec
		   timer. */
		timed = TRUE;
		msec_timeout = timeout2msec(timeout);
		if (msec_timeout > 0)
		{
			/* for the read x:n case a timer started here.  It is checked in the (timed) clause
			 at the end of this routine and canceled if it has not expired. */
			sys_get_curr_time(&cur_time);
			if (!zint_restart || !pipeintr->end_time_valid)
				add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			else
			{	/* end_time taken from restart data. Compute what msec_timeout should be so timeout timer
				   gets set correctly below.
				*/
				end_time = pipeintr->end_time;	/* Restore end_time for timeout */
                                cur_time = sub_abs_time(&end_time, &cur_time);
                                if (0 > cur_time.at_sec)
                                {
					msec_timeout = -1;
                                        out_of_time = TRUE;
                                } else
					msec_timeout = (int4)(cur_time.at_sec * 1000 + cur_time.at_usec / 1000);
				PIPE_DEBUG(PRINTF("piperfl: Taking timeout end time from read restart data - "
						  "computed msec_timeout: %d\n", msec_timeout); DEBUGPIPEFLUSH);
			}
			PIPE_DEBUG(PRINTF("msec_timeout: %d\n", msec_timeout); DEBUGPIPEFLUSH);
			/* if it is a disk read with follow don't start timer, as we use a sleep loop instead */
			if ((0 < msec_timeout) && !rm_ptr->follow)
				start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
		}
		else
		{
			/* out_of_time is set to TRUE because no timer is set for read x:0 for any device type at
			 this point.  It will be set to FALSE for a pipe device if it has read one character as
			 described above and set a timer. */
			out_of_time = TRUE;
			FCNTL2(fildes, F_GETFL, flags);
			if (0 > flags)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM, errno);
			FCNTL3(fildes, F_SETFL, (flags | O_NONBLOCK), fcntl_res);
			if (0 > fcntl_res)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM, errno);
			blocked_in = FALSE;
			if (rm_ptr->pipe)
				pipe_zero_timeout = TRUE;
		}
	}
	pipeintr->end_time_valid = FALSE;
	errno = status = 0;
	chset = io_ptr->ichset;
        if (!utf_active)
	{
		if (rm_ptr->fixed)
                {       /* This is M mode - one character is one byte.
                         * Note the check for EINTR below is valid and should not be converted to an EINTR
                         * wrapper macro, since action is taken on EINTR, not a retry.
                         */
			if (rm_ptr->follow)
			{
				PIPE_DEBUG(PRINTF(" %d fixed\n", pid); DEBUGPIPEFLUSH);
				if (timed)
				{
					if (0 < msec_timeout)
					{
						sleep_left = msec_timeout;
					} else
						sleep_left = 0;
				}
				/* if zeof is set in follow mode then ignore any previous zeof */
				if (TRUE == io_ptr->dollar.zeof)
					io_ptr->dollar.zeof = FALSE;
				do
				{
					status = read(fildes, temp, width - bytes_count);
					if (0 < status) /* we read some chars */
					{
						tot_bytes_read += status;
						rm_ptr->file_pos += status;
						bytes_count += status;
						temp = temp + status;
					} else if (0 == status) /* end of file */
					{
						if ((TRUE == timed) && (0 >= sleep_left))
						{
							follow_timeout = TRUE;
							break;
						}
						/* if a timed read, sleep the minimum of 100 ms and sleep_left.
						   If not a timed read then just sleep 100 ms */
						if (TRUE == timed)
							sleep_time = MIN(100,sleep_left);
						else
							sleep_time = 100;
						SHORT_SLEEP(sleep_time);
						if (TRUE == timed)
							sleep_left -= sleep_time;
						if (outofband)
						{
							PIPE_DEBUG(PRINTF(" %d fixed outofband\n", pid); DEBUGPIPEFLUSH);
							PUSH_MV_STENT(MVST_ZINTDEV);
							mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
							mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr =
								(char *)stringpool.free;
							mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = tot_bytes_read;
							mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
							pipeintr->who_saved = pipewhich_readfl;
							if (0 < msec_timeout && NO_M_TIMEOUT != msec_timeout)
							{
								pipeintr->end_time = end_time;
								pipeintr->end_time_valid = TRUE;
							}
							pipeintr->max_bufflen = exp_width;
							pipeintr->bytes_read = tot_bytes_read;
							rm_ptr->mupintr = TRUE;
							stringpool.free += tot_bytes_read; /* Don't step on our parade in
											      the interrupt */
							(TREF(pipefifo_interrupt))++;
							outofband_action(FALSE);
							GTMASSERT;	/* Should *never* return from outofband_action */
							return FALSE;	/* For the compiler.. */
						}
						continue; /* for now try and read again if eof or no input ready */
					} else		  /* error returned */
					{
						PIPE_DEBUG(PRINTF("errno= %d fixed\n", errno); DEBUGPIPEFLUSH);
						if (errno != EINTR)
						{
							bytes_count = 0;
							break;
						}
					}
				} while (bytes_count < width);
			} else
			{
				/* If it is a pipe and at least one character is read, a timer with timer_id
				   will be started.  It is canceled later in this routine if not expired
				   prior to return */
				DOREADRLTO2(fildes, temp, width, out_of_time, &blocked_in, rm_ptr->pipe, flags, status,
					    &tot_bytes_read, timer_id, &msec_timeout, pipe_zero_timeout, FALSE, pipe_or_fifo);

				PIPE_DEBUG(PRINTF(" %d fixed\n", pid); DEBUGPIPEFLUSH);

				if (0 > status)
				{
					if (pipe_or_fifo)
						bytes_count = tot_bytes_read;
					else
						bytes_count = 0;
					if (errno == EINTR  &&  out_of_time)
						status = -2;
				} else
				{
					tot_bytes_read = bytes_count = status;
					rm_ptr->file_pos += status;
				}
				if (zint_restart)
				{
					tot_bytes_read += bytes_read;
					bytes_count = tot_bytes_read;
					PIPE_DEBUG(PRINTF(" %d temp= %s tot_bytes_read = %d\n", pid, temp, tot_bytes_read);
						   DEBUGPIPEFLUSH);
				}

				if (pipe_or_fifo && outofband)
				{
					PIPE_DEBUG(PRINTF(" %d fixed outofband\n", pid); DEBUGPIPEFLUSH);
					PUSH_MV_STENT(MVST_ZINTDEV);
					mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
					mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = (char *)stringpool.free;
					mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = tot_bytes_read;
					mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
					pipeintr->who_saved = pipewhich_readfl;
					if (0 < msec_timeout && NO_M_TIMEOUT != msec_timeout)
					{
						pipeintr->end_time = end_time;
						pipeintr->end_time_valid = TRUE;
						cancel_timer(timer_id);		/* Worry about timer if/when we come back */
					}
					pipeintr->max_bufflen = exp_width;
					pipeintr->bytes_read = tot_bytes_read;
					rm_ptr->mupintr = TRUE;
					stringpool.free += tot_bytes_read;	/* Don't step on our parade in the interrupt */
					(TREF(pipefifo_interrupt))++;
					outofband_action(FALSE);
					GTMASSERT;	/* Should *never* return from outofband_action */
					return FALSE;	/* For the compiler.. */
				}
			}

		} else if (!rm_ptr->pipe && !rm_ptr->fifo)
		{	/* rms-file device */
			if ((rm_ptr->follow) && timed)
			{
				if (0 < msec_timeout)
				{
					sleep_left = msec_timeout;
				} else
					sleep_left = 0;
			}

			/* if zeof is set and follow is TRUE then ignore any previous zeof */
			if (rm_ptr->follow && (TRUE == io_ptr->dollar.zeof))
				io_ptr->dollar.zeof = FALSE;
			do
			{
				if (EOF != (status = getc(filstr)))
				{
					inchar = (unsigned char)status;
					tot_bytes_read++;
					rm_ptr->file_pos++;
					if (inchar == NATIVE_NL)
					{
						line_term_seen = TRUE;
						if (!rdone)
							break;
					}
					*temp++ = inchar;
					bytes_count++;
				} else
				{
					inchar = 0;
					if (feof(filstr))
					{
						status = 0;
						clearerr(filstr);

						if (rm_ptr->follow)
						{
							if ((TRUE == timed) && (0 >= sleep_left))
							{
								follow_timeout = TRUE;
								break;
							}
							/* if a timed read, sleep the minimum of 100 ms and sleep_left.
							   If not a timed read then just sleep 100 ms */
							if (TRUE == timed)
								sleep_time = MIN(100,sleep_left);
							else
								sleep_time = 100;
							SHORT_SLEEP(sleep_time);
							if (TRUE == timed)
								sleep_left -= sleep_time;
							if (outofband)
							{
								PIPE_DEBUG(PRINTF(" %d outofband\n", pid); DEBUGPIPEFLUSH);
								PUSH_MV_STENT(MVST_ZINTDEV);
								mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
								mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr =
									(char *)stringpool.free;
								mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len =
									tot_bytes_read;
								mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
								pipeintr->who_saved = pipewhich_readfl;
								if (0 < msec_timeout && NO_M_TIMEOUT != msec_timeout)
								{
									pipeintr->end_time = end_time;
									pipeintr->end_time_valid = TRUE;
								}
								pipeintr->max_bufflen = exp_width;
								pipeintr->bytes_read = tot_bytes_read;
								rm_ptr->mupintr = TRUE;
								stringpool.free += tot_bytes_read;	/* Don't step on our parade
													   in the interrupt */
								(TREF(pipefifo_interrupt))++;
								outofband_action(FALSE);
								GTMASSERT;	/* Should *never* return from outofband_action */
								return FALSE;	/* For the compiler.. */
							}
							continue; /* for now try and read again if eof or no input ready */
						}
					}
					else
						do_clearerr = TRUE;
					break;
				}
			} while (bytes_count < width);
		} else
		{	/* fifo or pipe device */
			do
			{
				unsigned char tchar;
				int tfcntl_res;
				if (EOF != (status = getc(filstr)))
				{
					tchar = (unsigned char)status;
					/* force it to process below in case character read is a 0 */
					if (!status)
						status = 1;
				}
				else if (feof(filstr))
				{
					status = 0;
					clearerr(filstr);
				}

				if (0 > status)
				{
					do_clearerr = TRUE;
					if (!rm_ptr->pipe || !timed || 0 != msec_timeout)
					{
						/* process for a non-pipe or r x or r x:1 or r x:0 after timer started*/
						inchar = 0;
						/* When a sigusr1 is sent it sets errno to EINTR */
						/* if it is outofband don't ignore it - don't take the "continue" */
						/* Don't ignore the out_of_time, however */
						if (EINTR == errno)
						{
							if (out_of_time)
								status = -2;
							else if (0 == outofband || !pipe_or_fifo)
								continue; /* Ignore interrupt if not our wakeup and not outofband */
						}
					}
					/* if it is not an outofband signal for a pipe/fifo then break out of the loop */
					if (0 == outofband || !pipe_or_fifo)
						break;
				} else if (status)
				{
					status = tchar;
					inchar = (unsigned char)status;
					if (pipe_zero_timeout && blocked_in == FALSE)
					{
						FCNTL3(fildes, F_SETFL, flags, tfcntl_res);
						if (0 > tfcntl_res)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
								  LEN_AND_LIT("fcntl"), CALLFROM, errno);
						blocked_in = TRUE;
						out_of_time = FALSE;
						/* Set a timer for 1 sec so atomic read x:0 will still work
						   on loaded systems but timeout on incomplete reads.  Any
						   characters read prior a timeout will be returned and
						   $device will be set to "0".*/
						msec_timeout = timeout2msec(1);
						start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
					}
					tot_bytes_read++;
					if (NATIVE_NL == inchar)
					{
						line_term_seen = TRUE;
						if (!rdone)
							break;
					}
					*temp++ = inchar;
					bytes_count++;
				} else
					break; /* it's an EOF */

				/* process outofband if set and we didn't see a line terminator or an EOF */
				if (pipe_or_fifo && outofband)
				{
					PIPE_DEBUG(PRINTF(" %d outofband\n", pid); DEBUGPIPEFLUSH);
					PUSH_MV_STENT(MVST_ZINTDEV);
					mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
					mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = (char *)stringpool.free;
					mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = tot_bytes_read;
					mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
					pipeintr->who_saved = pipewhich_readfl;
					if (0 < msec_timeout && NO_M_TIMEOUT != msec_timeout)
					{
						pipeintr->end_time = end_time;
						pipeintr->end_time_valid = TRUE;
						cancel_timer(timer_id);		/* Worry about timer if/when we come back */
					}
					pipeintr->max_bufflen = exp_width;
					pipeintr->bytes_read = tot_bytes_read;
					rm_ptr->mupintr = TRUE;
					stringpool.free += tot_bytes_read;	/* Don't step on our parade in the interrupt */
					(TREF(pipefifo_interrupt))++;
					outofband_action(FALSE);
					GTMASSERT;	/* Should *never* return from outofband_action */
					return FALSE;	/* For the compiler.. */
				}
			} while (bytes_count < width);
		}
	} else
	{	/* Unicode mode */
		assert(NULL != rm_ptr->inbuf);
		if (rm_ptr->fixed)
		{
			buff_len = (int)(rm_ptr->inbuf_top - rm_ptr->inbuf_off);
			assert(buff_len < rm_ptr->recordsize);
			/* zint_restart can only be set if we haven't finished filling the buffer.  Let iorm_get() decide
			 what to do.  */
			if ((0 == buff_len) || zint_restart)
			{	/* need to refill the buffer */
				if (rm_ptr->follow)
					buff_len = iorm_get_fol(io_ptr, &tot_bytes_read, &msec_timeout, timed, zint_restart,
								&follow_timeout);
				else
					buff_len = iorm_get(io_ptr, &blocked_in, rm_ptr->pipe, flags, &tot_bytes_read,
						    timer_id, &msec_timeout, pipe_zero_timeout, zint_restart);
				if (0 > buff_len)
				{
					bytes_count = 0;
					if (errno == EINTR  &&  out_of_time)
						buff_len = -2;
				} else if (outofband && (buff_len < rm_ptr->recordsize))
				{
					PIPE_DEBUG(PRINTF(" %d utf fixed outofband, buff_len: %d done_1st_read: %d\n", pid,
							  buff_len, rm_ptr->done_1st_read); DEBUGPIPEFLUSH);
					PUSH_MV_STENT(MVST_ZINTDEV);
					mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
					mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = (char *)stringpool.free;
					mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = 0;
					mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
					pipeintr->who_saved = pipewhich_readfl;
					if (0 < msec_timeout && NO_M_TIMEOUT != msec_timeout)
					{
						pipeintr->end_time = end_time;
						pipeintr->end_time_valid = TRUE;
						cancel_timer(timer_id);		/* Worry about timer if/when we come back */
					}
					pipeintr->max_bufflen = exp_width;
					/* since nothing read into stringpool.free set pipeintr->bytes_read to zero
					 as no bytes need to be copied in restart. */
					pipeintr->bytes_read = 0;
					rm_ptr->mupintr = TRUE;
					(TREF(pipefifo_interrupt))++;
					outofband_action(FALSE);
					GTMASSERT;	/* Should *never* return from outofband_action */
					return FALSE;	/* For the compiler.. */
				}
				chset = io_ptr->ichset;		/* in case UTF-16 was changed */
			}
			status = tot_bytes_read = buff_len;		/* for EOF checking at the end */
			char_ptr = rm_ptr->inbuf_off;

			if (0 < buff_len)
			{
				for (char_count = 0; char_count < width && char_ptr < rm_ptr->inbuf_top; char_count++)
				{	/* count chars and check for validity */
					switch (chset)
					{
					case CHSET_UTF8:
						if (UTF8_VALID(char_ptr, rm_ptr->inbuf_top, mblen))
						{
							bytes_count += mblen;
							char_ptr += mblen;
						} else
						{
							SETZACANCELTIMER;
							/* nothing in temp so set size to 0 */
							iorm_readfl_badchar(v, 0, mblen, char_ptr, rm_ptr->inbuf_top);
							rm_ptr->inbuf_off = char_ptr + mblen;	/* mark as read */
							UTF8_BADCHAR(mblen, char_ptr, rm_ptr->inbuf_top,
								     chset_names[chset].len, chset_names[chset].addr);
						}
						break;
					case CHSET_UTF16BE:
						if (UTF16BE_VALID(char_ptr, rm_ptr->inbuf_top, mblen))
						{
							bytes_count += mblen;
							char_ptr += mblen;
						} else
						{
							SETZACANCELTIMER;
							/* nothing in temp so set size to 0 */
							iorm_readfl_badchar(v, 0, mblen, char_ptr, rm_ptr->inbuf_top);
							rm_ptr->inbuf_off = char_ptr + mblen;	/* mark as read */
							UTF8_BADCHAR(mblen, char_ptr, rm_ptr->inbuf_top,
								     chset_names[chset].len, chset_names[chset].addr);
						}
						break;
					case CHSET_UTF16LE:
						if (UTF16LE_VALID(char_ptr, rm_ptr->inbuf_top, mblen))
						{
							bytes_count += mblen;
							char_ptr += mblen;
						} else
						{
							SETZACANCELTIMER;
							/* nothing in temp so set size to 0 */
							iorm_readfl_badchar(v, 0, mblen, char_ptr, rm_ptr->inbuf_top);
							rm_ptr->inbuf_off = char_ptr + mblen;	/* mark as read */
							UTF8_BADCHAR(mblen, char_ptr, rm_ptr->inbuf_top,
								     chset_names[chset].len, chset_names[chset].addr);
						}
						break;
					default:
						GTMASSERT;
					}
				}
				if (rm_ptr->follow && (char_count == width) && (TRUE == follow_timeout))
					follow_timeout = FALSE;

				v->str.len = INTCAST(char_ptr - rm_ptr->inbuf_off);
				UNICODE_ONLY(v->str.char_len = char_count;)
				if (0 < v->str.len)
				{
					if (CHSET_UTF8 == chset)
					{
						if (temp != (char *)stringpool.free) /* BYPASSOK */
						{
							/* make sure enough space to store buffer */
							ENSURE_STP_FREE_SPACE(GTM_MB_LEN_MAX * width);
						}
						memcpy(stringpool.free, rm_ptr->inbuf_off, v->str.len);
					}
					else
					{
						v->str.addr = (char *)rm_ptr->inbuf_off;
						v->str.len = gtm_conv(chset_desc[chset], chset_desc[CHSET_UTF8],
								      &v->str, NULL, NULL);
					}
					v->str.addr = (char *)stringpool.free;
					rm_ptr->inbuf_off += char_ptr - rm_ptr->inbuf_off;
				}
			}
                } else
		{	/* VARIABLE or STREAM */
			PIPE_DEBUG(PRINTF("enter utf stream: %d inbuf_pos: %d inbuf_off: %d follow: %d\n", rm_ptr->done_1st_read,
					  rm_ptr->inbuf_pos, rm_ptr->inbuf_off, rm_ptr->follow); DEBUGPIPEFLUSH);
			assert(IS_UTF_CHSET(chset));
			if (rm_ptr->inbuf_pos <= rm_ptr->inbuf_off)
			{	/* reset buffer pointers */
				if (!zint_restart)
				{
					rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;
					bytes2read = (CHSET_UTF8 == chset) ? 1 : 2;
				}
			} else
			{	 /* use bytes from buffer for character left over from last read */
				assert(rm_ptr->done_1st_read);
				if (!zint_restart)
				{
					bytes2read = 0;			/* use bytes from buffer left over from last read */
				}
				bytes_read = char_bytes_read = (int)(rm_ptr->inbuf_pos - rm_ptr->inbuf_off);
				if (!zint_restart)
					add_bytes = bytes_read - 1;	/* to satisfy asserts */
			}
			PIPE_DEBUG(PRINTF("1: status: %d bytes2read: %d rm_ptr->utf_start_pos: %d "
					  "rm_ptr->utf_tot_bytes_in_buffer: %d char_bytes_read: %d add_bytes: %d\n",
					  status, bytes2read,rm_ptr->utf_start_pos,rm_ptr->utf_tot_bytes_in_buffer,
					  char_bytes_read, add_bytes); DEBUGPIPEFLUSH);
			char_start = rm_ptr->inbuf_off;

			if (rm_ptr->follow)
			{
				PIPE_DEBUG(PRINTF(" %d utf streaming with follow\n", pid); DEBUGPIPEFLUSH);

				/* rms-file device in follow mode */
				if (timed)
				{
					if (0 < msec_timeout)
					{
						sleep_left = msec_timeout;
					} else
						sleep_left = 0;
				}

				/* if zeof is set in follow mode then ignore any previous zeof */
				if (TRUE == io_ptr->dollar.zeof)
					io_ptr->dollar.zeof = FALSE;
			}

			do
			{
				if (!rm_ptr->done_1st_read)
				{
					/* need to check BOM */
					if (rm_ptr->follow)
					{
						status = iorm_get_bom_fol(io_ptr, &tot_bytes_read, &msec_timeout, timed,
									  &bom_timeout);
					} else
						status = iorm_get_bom(io_ptr, &blocked_in, rm_ptr->pipe, flags, &tot_bytes_read,
							      timer_id, &msec_timeout, pipe_zero_timeout);

					/* if we got an interrupt then the iorm_get_bom did not complete so not as much state
					   needs to be saved*/

					if (outofband && (pipe_or_fifo || rm_ptr->follow))
					{
						PIPE_DEBUG(PRINTF(" %d utf1 stream outofband\n", pid); DEBUGPIPEFLUSH);
						PUSH_MV_STENT(MVST_ZINTDEV);
						mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
						mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = (char *)stringpool.free;
						mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = 0;
						mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
						pipeintr->who_saved = pipewhich_readfl;
						if (0 < msec_timeout && NO_M_TIMEOUT != msec_timeout)
						{
							pipeintr->end_time = end_time;
							pipeintr->end_time_valid = TRUE;
							cancel_timer(timer_id);	/* Worry about timer if/when we come back */
						}
						pipeintr->max_bufflen = exp_width;
						/* nothing copied to stringpool.free yet */
						pipeintr->bytes_read = 0;
						pipeintr->bytes2read = bytes2read;
						pipeintr->add_bytes = add_bytes;
						pipeintr->bytes_count = bytes_count;
						rm_ptr->mupintr = TRUE;
						(TREF(pipefifo_interrupt))++;
						PIPE_DEBUG(PRINTF(" %d utf1.1 stream outofband\n", pid); DEBUGPIPEFLUSH);
						outofband_action(FALSE);
						GTMASSERT;	/* Should *never* return from outofband_action */
						return FALSE;	/* For the compiler.. */
					}
					chset = io_ptr->ichset;	/* UTF16 will have changed to UTF16BE or UTF16LE */
				}

				if (0 <= status && bytes2read && rm_ptr->bom_buf_cnt > rm_ptr->bom_buf_off)
				{
					PIPE_DEBUG(PRINTF("2: status: %d bytes2read: %d bom_buf_cnt: %d bom_buf_off: %d\n",
							  status, bytes2read, rm_ptr->bom_buf_cnt, rm_ptr->bom_buf_off);
						   DEBUGPIPEFLUSH);
					from_bom = MIN((rm_ptr->bom_buf_cnt - rm_ptr->bom_buf_off), bytes2read);
					memcpy(rm_ptr->inbuf_pos, &rm_ptr->bom_buf[rm_ptr->bom_buf_off], from_bom);
					rm_ptr->bom_buf_off += from_bom;
					rm_ptr->inbuf_pos += from_bom;
					bytes2read -= from_bom;		/* now in buffer */
					bytes_read = from_bom;
					char_bytes_read += from_bom;
					rm_ptr->file_pos += from_bom;   /* If there is no BOM increment file position */
					status = 0;
				}


				if ((0 <= status && 0 < bytes2read))
				{
					PIPE_DEBUG(PRINTF("3: status: %d bytes2read: %d rm_ptr->utf_start_pos: %d "
							  "rm_ptr->utf_tot_bytes_in_buffer: %d\n",
							  status, bytes2read, rm_ptr->utf_start_pos,
							  rm_ptr->utf_tot_bytes_in_buffer); DEBUGPIPEFLUSH);
					/* If it is a pipe and at least one character is read, a timer with timer_id
					   will be started.  It is canceled later in this routine if not expired
					   prior to return */
					if (rm_ptr->utf_start_pos == rm_ptr->utf_tot_bytes_in_buffer)
					{
						DEBUG_ONLY(memset(rm_ptr->utf_tmp_buffer, 0, CHUNK_SIZE));
						rm_ptr->utf_start_pos = rm_ptr->utf_tot_bytes_in_buffer = 0;
						/* Read CHUNK_SIZE bytes from device into the temporary buffer. By doing this
						 * one-byte reads can be avoided when in UTF mode.
						 *
						 */

						if (rm_ptr->follow)
						{
							if (FALSE == bom_timeout)
							{
								status = read(fildes, rm_ptr->utf_tmp_buffer, CHUNK_SIZE);

								if (0 == status) /* end of file */
								{
									if ((TRUE == timed) && (0 >= sleep_left))
									{
										follow_timeout = TRUE;
										break;
									}

									/* if a timed read, sleep the minimum of 100 ms and
									   sleep_left. If not a timed read then just sleep 100 ms */
									if (TRUE == timed)
										sleep_time = MIN(100,sleep_left);
									else
										sleep_time = 100;
									SHORT_SLEEP(sleep_time);
									if (TRUE == timed)
										sleep_left -= sleep_time;

									if (outofband)
									{
										PIPE_DEBUG(PRINTF(" %d utf2 stream outofband\n",
												  pid); DEBUGPIPEFLUSH);
										PUSH_MV_STENT(MVST_ZINTDEV);
										mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
										mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr
											= (char *)stringpool.free;
										mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len
											= bytes_count;
										mv_chain->mv_st_cont.mvs_zintdev.buffer_valid =
											TRUE;
										pipeintr->who_saved = pipewhich_readfl;
										if (0 < msec_timeout && NO_M_TIMEOUT !=
										    msec_timeout)
										{
											pipeintr->end_time = end_time;
											pipeintr->end_time_valid = TRUE;
										}
										pipeintr->max_bufflen = exp_width;
										/* streaming mode uses bytes_count to show how many
										   bytes are in *temp, but the interrupt re-entrant
										   code uses bytes_read */
										pipeintr->bytes_read = bytes_count;
										pipeintr->bytes2read = bytes2read;
										pipeintr->char_count = char_count;
										pipeintr->add_bytes = add_bytes;
										pipeintr->bytes_count = bytes_count;
										PIPE_DEBUG(PRINTF("utf2 stream outofband "
												  "char_bytes_read %d add_bytes "
												  "%d bytes_count %d\n",
												  char_bytes_read,
												  add_bytes, bytes_count);
											   DEBUGPIPEFLUSH);
										rm_ptr->mupintr = TRUE;
										/* Don't step on our parade in the interrupt */
										stringpool.free += bytes_count;
										(TREF(pipefifo_interrupt))++;
										outofband_action(FALSE);
										GTMASSERT;	/* Should *never* return from
												   outofband_action */
										return FALSE;	/* For the compiler.. */
									}
									continue; /* for now try and read again if eof or no input
										     ready */
								} else if (-1 == status && errno != EINTR)  /* error returned */
								{
									bytes_count = 0;
									break;
								}
							}
						} else
						{
							DOREADRLTO2(fildes, rm_ptr->utf_tmp_buffer, CHUNK_SIZE,
								    out_of_time, &blocked_in, rm_ptr->pipe, flags,
								    status, &utf_tot_bytes_read, timer_id,
								    &msec_timeout, pipe_zero_timeout, pipe_or_fifo, pipe_or_fifo);
						}

						PIPE_DEBUG(PRINTF("4: read chunk  status: %d utf_tot_bytes_read: %d\n",
								  status, utf_tot_bytes_read); DEBUGPIPEFLUSH);
						/* if status is -1, then total number of bytes read will be stored
						 * in utf_tot_bytes_read. */
						/* if chunk read returned some bytes then ignore outofband.  We won't try a
						 read again until bytes are processed*/
						if (pipe_or_fifo && outofband && (0 >= status))
						{
							PIPE_DEBUG(PRINTF(" %d utf2 stream outofband\n", pid); DEBUGPIPEFLUSH);
							PUSH_MV_STENT(MVST_ZINTDEV);
							mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
							mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr =
								(char *)stringpool.free;
							mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = bytes_count;
							mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
							pipeintr->who_saved = pipewhich_readfl;
							if (0 < msec_timeout && NO_M_TIMEOUT != msec_timeout)
							{
								pipeintr->end_time = end_time;
								pipeintr->end_time_valid = TRUE;
								/* Worry about timer if/when we come back */
								cancel_timer(timer_id);
							}
							pipeintr->max_bufflen = exp_width;
							/* streaming mode uses bytes_count to show how many bytes are in *temp,
							 but the interrupt re-entrant code uses bytes_read */
							pipeintr->bytes_read = bytes_count;
							pipeintr->bytes2read = bytes2read;
							pipeintr->char_count = char_count;
							pipeintr->add_bytes = add_bytes;
							pipeintr->bytes_count = bytes_count;
							PIPE_DEBUG(PRINTF("utf2 stream outofband "
									  "char_bytes_read %d add_bytes %d bytes_count %d\n",
									  char_bytes_read, add_bytes, bytes_count); DEBUGPIPEFLUSH);
							rm_ptr->mupintr = TRUE;
							/* Don't step on our parade in the interrupt */
							stringpool.free += bytes_count;
							(TREF(pipefifo_interrupt))++;
							outofband_action(FALSE);
							GTMASSERT;	/* Should *never* return from outofband_action */
							return FALSE;	/* For the compiler.. */
						}

						if (-1 == status)
						{
							rm_ptr->utf_tot_bytes_in_buffer = utf_tot_bytes_read;
							tot_bytes_read = utf_tot_bytes_read;
							if (!rm_ptr->pipe && !rm_ptr->fifo)
								status = utf_tot_bytes_read;
						}
						else
							rm_ptr->utf_tot_bytes_in_buffer = status;

					} else if (pipe_zero_timeout)
						out_of_time = FALSE;	/* reset out_of_time for pipe as no actual read is done */


					if (0 <= rm_ptr->utf_tot_bytes_in_buffer)
					{
						min_bytes_to_copy = MIN(bytes2read,
									(rm_ptr->utf_tot_bytes_in_buffer - rm_ptr->utf_start_pos));
						assert(0 <= min_bytes_to_copy);
						assert(CHUNK_SIZE >= min_bytes_to_copy);
						assert(rm_ptr->utf_start_pos <= rm_ptr->utf_tot_bytes_in_buffer);
						/* If we have data in buffer, copy it to inbuf_pos */
						if (0 < min_bytes_to_copy)
						{
							/* If min_bytes_to_copy == 1, avoid memcpy by direct assignment */
							if (1 == min_bytes_to_copy)
								*rm_ptr->inbuf_pos = rm_ptr->utf_tmp_buffer[rm_ptr->utf_start_pos];
							else
							{
								memcpy(rm_ptr->inbuf_pos,
										rm_ptr->utf_tmp_buffer + rm_ptr->utf_start_pos,
										min_bytes_to_copy);
							}
						}
						/* Increment utf_start_pos */
						rm_ptr->utf_start_pos += min_bytes_to_copy;
						/* Set status appropriately so that the following code can continue as if
						 * a one byte read happened. In case of a negative status, preserve the status
						 * as-is.
						 */
						status = (0 <= status) ? min_bytes_to_copy : status;
					}
				}
				if (0 <= status)
				{
					rm_ptr->inbuf_pos += status;
					bytes_read += status;			/* bytes read this pass */
					char_bytes_read += status;		/* bytes for this character */
					tot_bytes_read += bytes_read;		/* total bytes read this command */
					rm_ptr->file_pos += status;
					PIPE_DEBUG(PRINTF("5: status: %d bytes2read: %d bytes_read: %d char_bytes_read: "
							  "%d tot_bytes_read: %d\n", status, bytes2read, bytes_read,
							  char_bytes_read, tot_bytes_read); DEBUGPIPEFLUSH);
					if (0 < bytes2read && 0 == status)
					{	/* EOF  on read */
						if (0 == char_bytes_read)
							break;			/* nothing read for this char so treat as EOF */
						assert(1 < (bytes2read + bytes_read));	/* incomplete character */
						SETZACANCELTIMER;
						iorm_readfl_badchar(v, (int)((unsigned char *)temp - stringpool.free),
								    bytes_read, char_start, rm_ptr->inbuf_pos);
						rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
						UTF8_BADCHAR(bytes_read, char_start, rm_ptr->inbuf_pos,
							     chset_names[chset].len, chset_names[chset].addr);
					} else if (status < bytes2read)
					{
						bytes2read -= status;
						continue;
					}
					if (CHSET_UTF8 == chset)
					{
						PIPE_DEBUG(PRINTF("6: char_bytes_read: %d\n", char_bytes_read); DEBUGPIPEFLUSH);
						if (1 == char_bytes_read)
						{
							add_bytes = UTF8_MBFOLLOW(char_start);
							if (0 < add_bytes)
							{
								bytes2read = add_bytes;
								PIPE_DEBUG(PRINTF("7: bytes2read: %d\n",
										  bytes2read); DEBUGPIPEFLUSH);
								continue;
							} else if (-1 == add_bytes)
							{
								SETZACANCELTIMER;
								iorm_readfl_badchar(v,
										    (int)((unsigned char *)temp - stringpool.free),
										    char_bytes_read,
										    char_start, (char_start + char_bytes_read));
								rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
								UTF8_BADCHAR(char_bytes_read, char_start,
									     char_start + char_bytes_read, 0, NULL);
							}
							if (u32_line_term[U32_LT_LF] == *char_start)
								if (rm_ptr->crlast)
								{	/* ignore LF following CR */
									rm_ptr->crlast = FALSE;
									rm_ptr->inbuf_pos = char_start;
									bytes2read = 1;				/* reset */
									bytes_read = char_bytes_read = 0;	/* start fresh */
									tot_bytes_read--;
									continue;
								} else
								{
									line_term_seen = TRUE;
									rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
									if (!rdone)
										break;
								}
							rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
							if (u32_line_term[U32_LT_CR] == *char_start)
							{
								rm_ptr->crlast = TRUE;
								line_term_seen = TRUE;
								if (!rdone)
									break;
							} else
								rm_ptr->crlast = FALSE;
							if (u32_line_term[U32_LT_FF] == *char_start)
							{
								line_term_seen = TRUE;
								if (!rdone)
									break;
							}
							*temp++ = *char_start;
							PIPE_DEBUG(PRINTF("8: move *char_start to *temp\n"); DEBUGPIPEFLUSH);
						} else
						{
							PIPE_DEBUG(PRINTF("9: char_bytes_read: %d add_bytes: %d\n",
									  char_bytes_read, add_bytes); DEBUGPIPEFLUSH);

							assert(char_bytes_read == (add_bytes + 1));
							nextmb = UTF8_MBTOWC(char_start, rm_ptr->inbuf_pos, utf_code);
							if (WEOF == utf_code)
							{	/* invalid mb char */
								SETZACANCELTIMER;
								iorm_readfl_badchar(v,
										    (int)((unsigned char *)temp - stringpool.free),
										    char_bytes_read,
										    char_start, rm_ptr->inbuf_pos);
								rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
								UTF8_BADCHAR(char_bytes_read, char_start,
									     rm_ptr->inbuf_pos, 0, NULL);
							}
							assert(nextmb == rm_ptr->inbuf_pos);
							rm_ptr->inbuf_off = nextmb;	/* mark as read */
							rm_ptr->crlast = FALSE;
							if (u32_line_term[U32_LT_NL] == utf_code ||
							    u32_line_term[U32_LT_LS] == utf_code ||
							    u32_line_term[U32_LT_PS] == utf_code)
							{
								line_term_seen = TRUE;
								if (!rdone)
									break;
							}
							memcpy(temp, char_start, char_bytes_read);
							PIPE_DEBUG(PRINTF("10: move char_bytes_read: %d to *temp\n",
									  char_bytes_read); DEBUGPIPEFLUSH);
							temp += char_bytes_read;
						}
						bytes_count += char_bytes_read;
						PIPE_DEBUG(PRINTF("11: bytes_count: %d \n", bytes_count); DEBUGPIPEFLUSH);
						if (bytes_count > MAX_STRLEN)
						{	/* need to leave bytes for this character in buffer */
							bytes_count -= char_bytes_read;
							rm_ptr->inbuf_off = char_start;
							rm_ptr->file_pos -= char_bytes_read;
							break;
						}
					} else if (CHSET_UTF16BE == chset || CHSET_UTF16LE == chset)
					{
						if (2 == char_bytes_read)
						{
							if (CHSET_UTF16BE == chset)
								add_bytes = UTF16BE_MBFOLLOW(char_start, rm_ptr->inbuf_pos);
							else
								add_bytes = UTF16LE_MBFOLLOW(char_start, rm_ptr->inbuf_pos);
							if (1 < add_bytes)
							{	/* UTF16xE_MBFOLLOW returns 1 or 3 if valid */
								bytes2read = add_bytes - 1;
								continue;
							} else if (-1 == add_bytes)
							{	/*  not valid */
								SETZACANCELTIMER;
                                                                iorm_readfl_badchar(v,
										    (int)((unsigned char *)temp - stringpool.free),
										    char_bytes_read,
                                                                                    char_start, rm_ptr->inbuf_pos);
								rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
								UTF8_BADCHAR(char_bytes_read, char_start, rm_ptr->inbuf_pos,
									     chset_names[chset].len, chset_names[chset].addr);
							}
						}
						assert(char_bytes_read == (add_bytes + 1));
						if (CHSET_UTF16BE == chset)
							nextmb = UTF16BE_MBTOWC(char_start, rm_ptr->inbuf_pos, utf_code);
						else
							nextmb = UTF16LE_MBTOWC(char_start, rm_ptr->inbuf_pos, utf_code);
						if (WEOF == utf_code)
						{	/* invalid mb char */
							SETZACANCELTIMER;
							iorm_readfl_badchar(v, (int)((unsigned char *)temp - stringpool.free),
									    char_bytes_read, char_start, rm_ptr->inbuf_pos);
							rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
							UTF8_BADCHAR(char_bytes_read, char_start, rm_ptr->inbuf_pos,
								     chset_names[chset].len, chset_names[chset].addr);
						}
						assert(nextmb == rm_ptr->inbuf_pos);
						if (u32_line_term[U32_LT_LF] == utf_code)
						{
							if (rm_ptr->crlast)
							{	/* ignore LF following CR */
								rm_ptr->crlast = FALSE;
								rm_ptr->inbuf_pos = char_start;
								bytes2read = 2;				/* reset */
								bytes_read = char_bytes_read = 0;	/* start fresh */
								tot_bytes_read -= 2;
								continue;
							}
						}
						rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
						if (u32_line_term[U32_LT_CR] == utf_code)
							rm_ptr->crlast = TRUE;
						else
							rm_ptr->crlast = FALSE;
						for (ltind = 0; 0 < u32_line_term[ltind]; ltind++)
							if (u32_line_term[ltind] == utf_code)
							{
								line_term_seen = TRUE;
								break;
							}
						if (line_term_seen && !rdone)
							break;		/* out of do loop */
						temp_start = temp;
						temp = (char *)UTF8_WCTOMB(utf_code, temp_start);
						bytes_count += (int4)(temp - temp_start);
						if (bytes_count > MAX_STRLEN)
						{	/* need to leave bytes for this character in buffer */
							bytes_count -= (int4)(temp - temp_start);
							rm_ptr->inbuf_off = char_start;
							rm_ptr->file_pos -= char_bytes_read;
							break;
						}
					} else
						GTMASSERT;
					char_count++;
					char_start = rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;
					bytes_read = char_bytes_read = 0;
					bytes2read = (CHSET_UTF8 == chset) ? 1 : 2;
				} else
				{
					inchar = 0;
					if (errno == 0)
					{
						tot_bytes_read = 0;
						status = 0;
					} else if (EINTR == errno)
					{
						if (out_of_time)
							status = -2;
						else
							continue;		/* Ignore interrupt if not our wakeup */
					}
					break;
				}
			} while (char_count < width && bytes_count < MAX_STRLEN);
		}
 	}
	PIPE_DEBUG(PRINTF(" %d notoutofband, %d %d\n", pid, status, line_term_seen); DEBUGPIPEFLUSH);
	real_errno = errno;
	if (TRUE == do_clearerr)
		clearerr(filstr);
	memcpy(io_ptr->dollar.device, "0", SIZEOF("0"));
	io_ptr->dollar.za = 0;
	/* On error, getc() returns EOF while read() returns -1. Both code paths converge here. Thankfully EOF is -1 on all
	 * platforms that we know of so it is enough to check for -1 status here. Assert that below.
	 */
	assert(EOF == -1);
	if ((-1 == status) && (EINTR != real_errno))
	{
		v->str.len = 0;
		v->str.addr = (char *)stringpool.free;		/* ensure valid address */
		if (EAGAIN != real_errno)
		{
			/* Need to cancel the timer before taking the error return.  Otherwise, it will be
			   canceled under the (timed) clause below. */
			if (timed && !out_of_time)
				cancel_timer(timer_id);
			io_ptr->dollar.za = 9;
			/* save error in $device */
			DOLLAR_DEVICE_SET(io_ptr, real_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) real_errno);
		}
		ret = FALSE;
	}

	if (timed)
	{
		/* If a timer was started then msec_timeout will be non-zero so the cancel_timer
		 check is done in the else clause. */
		if (msec_timeout == 0)
		{
			if (!rm_ptr->pipe || FALSE == blocked_in)
			{
				FCNTL3(fildes, F_SETFL, flags, fcntl_res);
				if (0 > fcntl_res)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"),
						      CALLFROM, errno);
			}
			if ((rm_ptr->pipe && (0 == status)) || (rm_ptr->fifo && (0 == status || real_errno == EAGAIN)))
				ret = FALSE;
		} else
		{
			if (out_of_time)
				ret = FALSE;
			else if (!rm_ptr->follow) /* if follow then no timer started */
				cancel_timer(timer_id);
		}
	}

	if (status == 0 && tot_bytes_read == 0)
	{
		v->str.len = 0;
		v->str.addr = (char *)stringpool.free;		/* ensure valid address */
		UNICODE_ONLY(v->str.char_len = 0;)

		if (rm_ptr->follow)
		{
			if (TRUE == io_ptr->dollar.zeof)
				io_ptr->dollar.zeof = FALSE; /* no EOF in follow mode */
			return(FALSE);
		}
		/* on end of file set $za to 9 */
		len = SIZEOF(ONE_COMMA_DEV_DET_EOF);
		memcpy(io_ptr->dollar.device, ONE_COMMA_DEV_DET_EOF, len);
		io_ptr->dollar.za = 9;

		if ((TRUE == io_ptr->dollar.zeof) && (RM_READ == saved_lastop))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IOEOF);

		io_ptr->dollar.zeof = TRUE;
		*dollarx_ptr = 0;
		(*dollary_ptr)++;
		if (io_ptr->error_handler.len > 0)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IOEOF);
		if ((pipe_zero_timeout || rm_ptr->fifo) && out_of_time)
		{
			ret = TRUE;
			out_of_time = FALSE;
		}
	} else
	{
		if (rm_ptr->pipe)
		{
			if ((tot_bytes_read > 0) || (out_of_time && blocked_in))
				ret = TRUE;
			else
				ret = FALSE;
		}
		if (rm_ptr->follow && !rm_ptr->fixed && !line_term_seen)
			ret = FALSE;
		if (!utf_active || !rm_ptr->fixed)
		{	/* if Unicode and fixed, already setup the mstr */
			v->str.len = bytes_count;
			v->str.addr = (char *)stringpool.free;
			UNICODE_ONLY(v->str.char_len = char_count;)
			if (!utf_active && !rm_ptr->fixed)
				char_count = bytes_count;
		}
		if (!rm_ptr->fixed && line_term_seen)
		{
		    	*dollarx_ptr = 0;
			(*dollary_ptr)++;
		} else
		{
			*dollarx_ptr += char_count;
			if (*dollarx_ptr >= io_ptr->width && io_ptr->wrap)
			{
				*dollary_ptr += (*dollarx_ptr / io_ptr->width);
				if(io_ptr->length)
					*dollary_ptr %= io_ptr->length;
				*dollarx_ptr %= io_ptr->width;
			}
		}
	}
	if (follow_timeout)
		ret = FALSE;
	assert(FALSE == rm_ptr->mupintr);
	return (rm_ptr->pipe && out_of_time) ? FALSE : ret;
}
