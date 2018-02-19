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
#include "gtmcrypt.h"
#include "error.h"

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	volatile bool	out_of_time;
GBLREF  boolean_t       gtm_utf8_mode;
GBLREF	volatile int4	outofband;
GBLREF	mv_stent      	*mv_chain;
GBLREF  boolean_t    	dollar_zininterrupt;
#ifdef UNICODE_SUPPORTED
LITREF	UChar32		u32_line_term[];
LITREF	mstr		chset_names[];
GBLREF	UConverter	*chset_desc[];
#endif
error_def(ERR_IOEOF);
error_def(ERR_SYSCALL);
error_def(ERR_ZINTRECURSEIO);
error_def(ERR_DEVICEWRITEONLY);
error_def(ERR_IOERROR);

#define fl_copy(a, b) (a > b ? b : a)

#define SETZACANCELTIMER					\
		io_ptr->dollar.za = 9;				\
		v->str.len = 0;					\
		if (!rm_ptr->follow && timed && !out_of_time)	\
			cancel_timer(timer_id);

#ifdef UNICODE_SUPPORTED

#define UTF8CRLEN	1	/* Length of CR in UTF8 mode. */
#define SET_UTF8_DOLLARKEY_DOLLARZB(UTF_CODE, DOLLAR_KEY, DOLLAR_ZB)		\
{										\
		unsigned char *endstr;						\
		endstr = UTF8_WCTOMB(UTF_CODE, DOLLAR_KEY);			\
		*endstr = '\0';							\
		endstr = UTF8_WCTOMB(UTF_CODE, DOLLAR_ZB);			\
		*endstr = '\0';							\
}

/* Maintenance of $ZB on a badchar error and returning partial data (if any) */
void iorm_readfl_badchar(mval *vmvalptr, int datalen, int delimlen, unsigned char *delimptr, unsigned char *strend)
{
	int             tmplen, len;
	unsigned char   *delimend;
	io_desc         *iod;
	d_rm_struct	*rm_ptr;
	boolean_t	ch_set;

	assert(0 <= datalen);
	iod = io_curr_device.in;
	ESTABLISH_GTMIO_CH(&io_curr_device, ch_set);
	rm_ptr = (d_rm_struct *)(iod->dev_sp);
	assert(NULL != rm_ptr);
	vmvalptr->str.len = datalen;
	vmvalptr->str.addr = (char *)stringpool.free;
        if (0 < datalen)
		/* Return how much input we got */
		stringpool.free += vmvalptr->str.len;

        if ((NULL != strend) && (NULL != delimptr))
        {       /* First find the end of the delimiter (max of 4 bytes) */
		if (0 == delimlen)
		{
			for (delimend = delimptr; (GTM_MB_LEN_MAX >= delimlen) && (delimend < strend); ++delimend, ++delimlen)
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
	SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, BADCHAR_DEVICE_MSG);
	REVERT_GTMIO_CH(&io_curr_device, ch_set);
}
#endif

int	iorm_readfl (mval *v, int4 width, int4 msec_timeout) /* timeout in milliseconds */
{
	boolean_t	ret, timed, utf_active, line_term_seen = FALSE, rdone = FALSE, zint_restart;
	char		inchar, *temp, *temp_start;
	unsigned char	*nextmb, *char_ptr, *char_start, *buffer_start;
	int		flags = 0;
	int		len;
	int		save_errno, errlen, real_errno;
	int		fcntl_res, stp_need;
	int4		bytes2read, bytes_read, char_bytes_read, add_bytes, reclen;
	int4		buff_len, mblen, char_count, bytes_count, tot_bytes_read, chunk_bytes_read, utf_tot_bytes_read;
	int4		status, max_width, ltind, exp_width, from_bom, fol_bytes_read, feof_status;
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
	int		follow_width;
	int		blocked_in = TRUE;
	int		do_clearerr = FALSE;
	int		saved_lastop;
	int             min_bytes_to_copy;
	ABS_TIME	cur_time, end_time, current_time, time_left;
	pipe_interrupt	*pipeintr;
	mv_stent	*mv_zintdev;
	unsigned int	*dollarx_ptr;
	unsigned int	*dollary_ptr;
	struct timeval	poll_interval;
	int		poll_status;
	fd_set		input_fds;
	int4 sleep_left;
	int4 sleep_time;
	struct stat	statbuf;
	int		fstat_res;
	off_t		cur_position;
	int		bom_size_toread;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG_PIPE
	pid=getpid();
#	endif
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);

	io_ptr = io_curr_device.in;
	ESTABLISH_RET_GTMIO_CH(&io_curr_device, -1, ch_set);
	/* don't allow a read from a writeonly fifo */
	if (((d_rm_struct *)io_ptr->dev_sp)->write_only)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVICEWRITEONLY);
#	ifdef __MVS__
	/* on zos if it is a fifo device then point to the pair.out for $X and $Y */
	if (((d_rm_struct *)io_ptr->dev_sp)->fifo)
	{
		dollarx_ptr = &(io_ptr->pair.out->dollar.x);
		dollary_ptr = &(io_ptr->pair.out->dollar.y);
	} else
#	endif
	{
		dollarx_ptr = &(io_ptr->dollar.x);
		dollary_ptr = &(io_ptr->dollar.y);
	}
	assert (io_ptr->state == dev_open);
	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	assert(NULL != rm_ptr);
	pipeintr = &rm_ptr->pipe_save_state;
	if (rm_ptr->is_pipe || rm_ptr->fifo)
		pipe_or_fifo = TRUE;
	PIPE_DEBUG(PRINTF(" %d enter iorm_readfl\n", pid); DEBUGPIPEFLUSH);
	/* if it is a pipe and it's the stdout returned then we need to get the read file descriptor
	   from rm_ptr->read_fildes and the stream pointer from rm_ptr->read_filstr */
	if ((rm_ptr->is_pipe ZOS_ONLY(|| rm_ptr->fifo)) && (0 < rm_ptr->read_fildes))
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
		if (!io_ptr->dollar.za && (!rm_ptr->is_pipe || rm_ptr->fixed))
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
		{
			save_errno = errno;
			SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM, save_errno);
		}
		if (flags & O_NONBLOCK)
		{
			FCNTL3(rm_ptr->fildes, F_SETFL, (flags & ~O_NONBLOCK), fcntl_res);
			if (0 > fcntl_res)
			{
				save_errno = errno;
				SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM,
					save_errno);
			}
		}
	}
	/* if the last operation was a write to a disk, we need to initialize it so file_pos is pointing
	   to the current file position */
	if (!rm_ptr->fifo && !rm_ptr->is_pipe && (2 < rm_ptr->fildes) && (RM_WRITE == rm_ptr->lastop))
	{
		/* need to do an lseek to get current location in file */
		cur_position = lseek(rm_ptr->fildes, 0, SEEK_CUR);
		if ((off_t)-1 == cur_position)
		{
			save_errno = errno;
			SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("lseek"),
				      RTS_ERROR_LITERAL("iorm_readfl()"), CALLFROM, save_errno);
		} else
			rm_ptr->file_pos = cur_position;
		*dollary_ptr = 0;
		*dollarx_ptr = 0;
		/* Reset temporary buffer so that the next read starts afresh */
		if (utf_active || !rm_ptr->fixed)
		{
			rm_ptr->out_bytes = rm_ptr->bom_buf_cnt = rm_ptr->bom_buf_off = 0;
			rm_ptr->inbuf_top = rm_ptr->inbuf_off = rm_ptr->inbuf_pos = rm_ptr->inbuf;
			DEBUG_ONLY(MEMSET_IF_DEFINED(rm_ptr->tmp_buffer, 0, CHUNK_SIZE));
			rm_ptr->start_pos = 0;
			rm_ptr->tot_bytes_in_buffer = 0;
		}
		if (utf_active)
		{
			/* If bom not checked yet, not at the beginning of the file and at least UTF16BE_BOM_LEN number of bytes,
			 * then go to the beginning of the file and read the potential BOM. Move back to previous file position
			 * after BOM check. Note that in case of encryption this is the only place where the BOM is read.
			 */
			if ((!rm_ptr->bom_checked) && (0 < rm_ptr->file_pos) && (!rm_ptr->input_encrypted))
			{
				FSTAT_FILE(fildes, &statbuf, fstat_res);
				if (-1 == fstat_res)
				{
					save_errno = errno;
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fstat"),
						      CALLFROM, save_errno);
				}
				assert(UTF16BE_BOM_LEN < UTF8_BOM_LEN);
				if ((CHSET_UTF8 == io_ptr->ichset) && (statbuf.st_size >= UTF8_BOM_LEN))
					bom_size_toread = UTF8_BOM_LEN;
				else if (IS_UTF16_CHSET(io_ptr->ichset) && (statbuf.st_size >= UTF16BE_BOM_LEN))
					bom_size_toread = UTF16BE_BOM_LEN;
				else
					bom_size_toread = 0;
				if (0 < bom_size_toread)
				{
					if ((off_t)-1 == lseek(fildes, 0, SEEK_SET))
					{
						save_errno = errno;
						SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
							      RTS_ERROR_LITERAL("lseek"), RTS_ERROR_LITERAL(
								      "Error setting file pointer to beginning of file"),
							      CALLFROM, save_errno);
					}
					rm_ptr->bom_num_bytes = open_get_bom(io_ptr, bom_size_toread);
					/* move back to previous file position */
					if ((off_t)-1 == lseek(fildes, rm_ptr->file_pos, SEEK_SET))
					{
						save_errno = errno;
						SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
							      RTS_ERROR_LITERAL("lseek"), RTS_ERROR_LITERAL(
								      "Error restoring file pointer"), CALLFROM, save_errno);
					}
				}
				rm_ptr->bom_checked = TRUE;
			}
		}
	}
	zint_restart = FALSE;
	/* Check if new or resumed read */
	if (rm_ptr->mupintr)
	{	/* We have a pending read restart of some sort */
		assertpro(pipewhich_invalid != pipeintr->who_saved);      /* Interrupt should never have an invalid save state */
		/* check we aren't recursing on this device */
		if (dollar_zininterrupt)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
		if (pipewhich_readfl != pipeintr->who_saved)
			assertpro(FALSE);      /* ZINTRECURSEIO should have caught */
		PIPE_DEBUG(PRINTF("piperfl: *#*#*#*#*#*#*#  Restarted interrupted read\n"); DEBUGPIPEFLUSH);
		mv_zintdev = io_find_mvstent(io_ptr, FALSE);
		if (mv_zintdev)
		{
			if (mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid)
			{
				bytes_read = pipeintr->bytes_read;
				assert(mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.len == bytes_read);
				if (bytes_read)
					buffer_start = (unsigned char *)mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr;
				else
					buffer_start = stringpool.free;
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
	io_ptr->dollar.key[0] = '\0';
	io_ptr->dollar.zb[0] = '\0';
	timer_id = (TID)iorm_readfl;
	max_width = io_ptr->width - *dollarx_ptr;
	if (0 == width)
	{
		width = io_ptr->width;		/* called from iorm_read */
		if (!rm_ptr->fixed)
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
		/* some locals needed by unicode & M streaming mode */
		if (!rm_ptr->fixed)
		{
			bytes2read = pipeintr->bytes2read;
			char_count = pipeintr->char_count;
			bytes_count = pipeintr->bytes_count;
			add_bytes = pipeintr->add_bytes;
		}
		PIPE_DEBUG(PRINTF("piperfl: .. mv_stent found - bytes_read: %d max_bufflen: %d"
				  "  interrupts: %d\n", bytes_read, exp_width, TREF(pipefifo_interrupt)); DEBUGPIPEFLUSH);
		PIPE_DEBUG(PRINTF("piperfl: .. timeout: %d\n", msec_timeout); DEBUGPIPEFLUSH);
		PIPE_DEBUG(if (pipeintr->end_time_valid) PRINTF("piperfl: .. endtime: %d/%d\n", end_time.at_sec,
								end_time.at_usec); DEBUGPIPEFLUSH);
		PIPE_DEBUG(PRINTF("piperfl: .. buffer address: 0x%08lx  stringpool: 0x%08lx\n",
				  buffer_start, stringpool.free); DEBUGPIPEFLUSH);
		PIPE_DEBUG(PRINTF("buffer_start =%s\n", buffer_start); DEBUGPIPEFLUSH);
		/* If it is fixed and utf mode then we are not doing any mods affecting stringpool during the read and
		   don't use temp, so skip the following stringpool checks */
		if (!utf_active || !rm_ptr->fixed)
		{
			if (!IS_AT_END_OF_STRINGPOOL(buffer_start, bytes_read))
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
			assert((buffer_start + bytes_read) <= stringpool.free);	/* BYPASSOK */
			if (!IS_AT_END_OF_STRINGPOOL(buffer_start, bytes_read))
			{	/* now need to move it to the top */
				assert(stp_need == exp_width);
				memcpy(stringpool.free, buffer_start, bytes_read);
			} else
				stringpool.free = buffer_start;	/* it should still be just under the used space */
			v->str.len = 0;		/* Clear in case interrupt or error -- don't want to hold this buffer */
			temp = (char *)(stringpool.free + bytes_read);
			tot_bytes_read = bytes_count = bytes_read;
			if (!(rm_ptr->fixed && rm_ptr->follow))
				width -= bytes_read;
		}
	}
	if (utf_active || !rm_ptr->fixed)
		bytes_read = 0;
	out_of_time = FALSE;
	if (NO_M_TIMEOUT == msec_timeout)
	{
		timed = FALSE;
		assert(!pipeintr->end_time_valid);
	} else
	{	/* For timed input, this routine starts only one timer. One case is for a READ x:n; another is potentially for the
		 * PIPE device doing a READ x:0. If a timer is set, out_of_time starts as FALSE. If the timer expires prior to a
		 * read completing, the timer handler changes out_of_time to TRUE. In the READ x:0 case for a PIPE, if an attempt to
		 * read one character in non-blocking mode succeeds, we set blocked_in to TRUE (to prevent a 2nd timer), set the
		 * PIPE to blocking mode and start the timer for one second. We set timed to TRUE for both READ x:n and x:0;
		 * msec_timeout is 0 for a READ x:0 unless it's a PIPE which has read one character and started a 1 second timer. If
		 * timed is TRUE, we manage the timer outcome at the end of this routine.
		 */
		timed = TRUE;
		if (0 < msec_timeout)
		{	/* For the READ x:n case, start a timer and clean it up in the (timed) clause at the end of this routine if
			 * it has not expired.
			 */
			sys_get_curr_time(&cur_time);
			if (!zint_restart || !pipeintr->end_time_valid)
				add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			else
			{	/* Compute appropriate msec_timeout using end_time from restart data. */
				end_time = pipeintr->end_time;	/* Restore end_time for timeout */
				cur_time = sub_abs_time(&end_time, &cur_time);
				if (0 > cur_time.at_sec)
				{
					msec_timeout = -1;
					out_of_time = TRUE;
				} else
					msec_timeout = (int4)(cur_time.at_sec * MILLISECS_IN_SEC +
							      DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
				if (rm_ptr->follow && !out_of_time && !msec_timeout)
					msec_timeout = 1;
				PIPE_DEBUG(PRINTF("piperfl: Taking timeout end time from read restart data - "
						  "computed msec_timeout: %d\n", msec_timeout); DEBUGPIPEFLUSH);
			}
			PIPE_DEBUG(PRINTF("msec_timeout: %d\n", msec_timeout); DEBUGPIPEFLUSH);
			/* if it is a disk read with follow don't start timer, as we use a sleep loop instead */
			if ((0 < msec_timeout) && !rm_ptr->follow)
				start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
		} else
		{	/* Except for the one-character read case with a PIPE device described above, a READ x:0 sets out_of_time to
			 * TRUE.
			 */
			out_of_time = TRUE;
			FCNTL2(fildes, F_GETFL, flags);
			if (0 > flags)
			{
				save_errno = errno;
				SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM,
					save_errno);
			}
			FCNTL3(fildes, F_SETFL, (flags | O_NONBLOCK), fcntl_res);
			if (0 > fcntl_res)
			{
				save_errno = errno;
				SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM,
					save_errno);
			}
			blocked_in = FALSE;
			if (rm_ptr->is_pipe)
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
					/* recalculate msec_timeout and sleep_left as &end_time - &current_time */
					if (0 < msec_timeout)
					{
						/* get the current time */
						sys_get_curr_time(&current_time);
						time_left = sub_abs_time(&end_time, &current_time);
						if (0 > time_left.at_sec)
						{
							msec_timeout = -1;
							out_of_time = TRUE;
						} else
							msec_timeout = (int4)(time_left.at_sec * MILLISECS_IN_SEC +
									DIVIDE_ROUND_UP(time_left.at_usec, MICROSECS_IN_MSEC));
						/* make sure it terminates with out_of_time */
						if (!out_of_time && !msec_timeout)
							msec_timeout = 1;
						sleep_left = msec_timeout;
					} else
						sleep_left = 0;
				}
				/* if zeof is set in follow mode then ignore any previous zeof */
				if (TRUE == io_ptr->dollar.zeof)
					io_ptr->dollar.zeof = FALSE;
				do
				{
					/* in follow mode a read will return an EOF if no more bytes are available. */
					status = read(fildes, temp, width - bytes_count);
					if (0 < status) /* we read some chars */
					{
						if (rm_ptr->input_encrypted)
							READ_ENCRYPTED_DATA(rm_ptr, io_ptr->trans_name, temp, status, NULL);
						rm_ptr->read_occurred = TRUE;
						rm_ptr->done_1st_read = TRUE;
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
						{
							/* recalculate msec_timeout and sleep_left as &end_time - &current_time */
							/* get the current time */
							sys_get_curr_time(&current_time);
							time_left = sub_abs_time(&end_time, &current_time);
							if (0 > time_left.at_sec)
							{
								msec_timeout = -1;
								out_of_time = TRUE;
							} else
								msec_timeout = (int4)(time_left.at_sec * MILLISECS_IN_SEC +
										      DIVIDE_ROUND_UP(time_left.at_usec,
												      MICROSECS_IN_MSEC));

							/* make sure it terminates with out_of_time */
							if (!out_of_time && !msec_timeout)
								msec_timeout = 1;
							sleep_left = msec_timeout;
							sleep_time = MIN(100, sleep_left);
						} else
							sleep_time = 100;
						if (0 < sleep_time)
							SHORT_SLEEP(sleep_time);
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
							if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
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
							REVERT_GTMIO_CH(&io_curr_device, ch_set);
							outofband_action(FALSE);
							assertpro(FALSE);	/* Should *never* return from outofband_action */
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
			{	/* If the device is a PIPE, and we read at least one character, start a timer using timer_id. We
				 * cancel that timer later in this routine if it has not expired before the return.
				 */
				DOREADRLTO2(fildes, temp, width, out_of_time, &blocked_in, rm_ptr->is_pipe, flags, status,
					    &tot_bytes_read, timer_id, &msec_timeout, pipe_zero_timeout, FALSE, pipe_or_fifo);
				PIPE_DEBUG(PRINTF(" %d fixed\n", pid); DEBUGPIPEFLUSH);
				if ((0 < status) || ((0 > status) && (0 < tot_bytes_read)))
				{
					if (rm_ptr->input_encrypted)
						READ_ENCRYPTED_DATA(rm_ptr, io_ptr->trans_name, temp,
							(status > 0) ? status : tot_bytes_read, NULL);
					rm_ptr->read_occurred = TRUE;
					rm_ptr->done_1st_read = TRUE;
				}
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
					if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
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
					REVERT_GTMIO_CH(&io_curr_device, ch_set);
					outofband_action(FALSE);
					assertpro(FALSE);	/* Should *never* return from outofband_action */
					return FALSE;	/* For the compiler.. */
				}
			}
		} else  /* Handle pipe/fifo with stream.var files */
		{	/* rms-file device, PIPE or FIFO */
		/* read using read() into buffer and manage the buffer. */
			PIPE_DEBUG(PRINTF("M 1: status: %d bytes2read: %d rm_ptr->start_pos: %d "
					  "rm_ptr->tot_bytes_in_buffer: %d add_bytes: %d bytes_read: %d\n",
					  status, bytes2read, rm_ptr->start_pos, rm_ptr->tot_bytes_in_buffer,
					  add_bytes, bytes_read); DEBUGPIPEFLUSH);
			if (rm_ptr->follow)
			{
				PIPE_DEBUG(PRINTF(" %d M streaming with follow\n", pid); DEBUGPIPEFLUSH);
				/* rms-file device in follow mode */
				if (timed)
				{
					/* recalculate msec_timeout and sleep_left as &end_time - &current_time */
					if (0 < msec_timeout)
					{
						/* get the current time */
						sys_get_curr_time(&current_time);
						time_left = sub_abs_time(&end_time, &current_time);
						if (0 > time_left.at_sec)
						{
							msec_timeout = -1;
							out_of_time = TRUE;
						} else
							msec_timeout = (int4)(time_left.at_sec * MILLISECS_IN_SEC +
									DIVIDE_ROUND_UP(time_left.at_usec, MICROSECS_IN_MSEC));
						/* make sure it terminates with out_of_time */
						if (!out_of_time && !msec_timeout)
							msec_timeout = 1;
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
				bytes2read = 1;
				PIPE_DEBUG(PRINTF("M 3: status: %d bytes2read: %d rm_ptr->start_pos: %d "
						  "rm_ptr->tot_bytes_in_buffer: %d\n",
						  status, bytes2read, rm_ptr->start_pos,
						  rm_ptr->tot_bytes_in_buffer); DEBUGPIPEFLUSH);
				/* If it is a pipe and at least one character is read, a timer with timer_id
				   will be started.  It is canceled later in this routine if not expired
				   prior to return */
				if (rm_ptr->start_pos == rm_ptr->tot_bytes_in_buffer)
				{
					DEBUG_ONLY(MEMSET_IF_DEFINED(rm_ptr->tmp_buffer, 0, CHUNK_SIZE));
					rm_ptr->start_pos = rm_ptr->tot_bytes_in_buffer = 0;
					/* Read CHUNK_SIZE bytes from device into the temporary buffer. By doing this
					 * one-byte reads can be avoided when in non fixed format.
					 *
					 */
					if (rm_ptr->follow)
					{
						/* In follow mode a read returns an EOF if no more bytes are available. */
						status = read(fildes, rm_ptr->tmp_buffer, CHUNK_SIZE);
						if (0 < status)
						{
							if (rm_ptr->input_encrypted)
								READ_ENCRYPTED_DATA(rm_ptr, io_ptr->trans_name,
									rm_ptr->tmp_buffer, status, NULL);
							rm_ptr->read_occurred = TRUE;
							rm_ptr->done_1st_read = TRUE;
						}
						if (0 == status) /* end of file */
						{
							if ((TRUE == timed) && (0 >= sleep_left))
							{
								follow_timeout = TRUE;
								break;
							}
							/* If a timed read, sleep the minimum of 100 ms and sleep_left.
							 * If not a timed read then just sleep 100 ms.
							 */
							if (TRUE == timed)
							{
								/* recalculate msec_timeout and sleep_left as
								 * &end_time - &current_time.
								 */
								/* get the current time */
								sys_get_curr_time(&current_time);
								time_left = sub_abs_time(&end_time, &current_time);
								if (0 > time_left.at_sec)
								{
									msec_timeout = -1;
									out_of_time = TRUE;
								} else
									msec_timeout = (int4)(time_left.at_sec *
											MILLISECS_IN_SEC +
											DIVIDE_ROUND_UP(time_left.at_usec,
											MICROSECS_IN_MSEC));
								/* make sure it terminates with out_of_time */
								if (!out_of_time && !msec_timeout)
									msec_timeout = 1;
								sleep_left = msec_timeout;
								sleep_time = MIN(100, sleep_left);
							} else
								sleep_time = 100;
							if (0 < sleep_time)
								SHORT_SLEEP(sleep_time);
							if (outofband)
							{
								PIPE_DEBUG(PRINTF(" %d M 2 stream outofband\n",
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
								if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
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
								pipeintr->char_count = bytes_count;
								pipeintr->add_bytes = add_bytes;
								pipeintr->bytes_count = bytes_count;
								PIPE_DEBUG(PRINTF("M 2 stream outofband "
										  " add_bytes "
										  "%d bytes_count %d\n",
										  add_bytes, bytes_count);
									   DEBUGPIPEFLUSH);
								rm_ptr->mupintr = TRUE;
								/* Don't step on our parade in the interrupt */
								stringpool.free += bytes_count;
								(TREF(pipefifo_interrupt))++;
								REVERT_GTMIO_CH(&io_curr_device, ch_set);
								outofband_action(FALSE);
								assertpro(FALSE);	/* Should *never* return from
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
					} else
					{	/* NO FOLLOW */
						DOREADRLTO2(fildes, rm_ptr->tmp_buffer, CHUNK_SIZE,
							    out_of_time, &blocked_in, rm_ptr->is_pipe, flags,
							    status, &chunk_bytes_read, timer_id,
							    &msec_timeout, pipe_zero_timeout, pipe_or_fifo, pipe_or_fifo);
						if ((0 < status) || ((0 > status) && (0 < chunk_bytes_read)))
						{
							if (rm_ptr->input_encrypted)
								READ_ENCRYPTED_DATA(rm_ptr, io_ptr->trans_name,
									rm_ptr->tmp_buffer,
									(status > 0) ? status : chunk_bytes_read, NULL);
							rm_ptr->read_occurred = TRUE;
							rm_ptr->done_1st_read = TRUE;
						}
					}
					PIPE_DEBUG(PRINTF(" M 4: read chunk  status: %d chunk_bytes_read: %d\n",
							  status, chunk_bytes_read); DEBUGPIPEFLUSH);
					/* If status is -1, then total number of bytes read will be stored in
					 * chunk_bytes_read. If chunk read returned some bytes, then ignore outofband.
					 * We won't try a read again until bytes are processed.
					 */
					if ((pipe_or_fifo || rm_ptr->follow) && outofband && (0 >= status))
					{
						PIPE_DEBUG(PRINTF(" %d utf2 stream outofband\n", pid); DEBUGPIPEFLUSH);
						PUSH_MV_STENT(MVST_ZINTDEV);
						mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
						mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr =
							(char *)stringpool.free;
						mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = bytes_count;
						mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
						pipeintr->who_saved = pipewhich_readfl;
						if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
						{
							pipeintr->end_time = end_time;
							pipeintr->end_time_valid = TRUE;
							/* Worry about timer if/when we come back */
							if (!rm_ptr->follow)
								cancel_timer(timer_id);
						}
						pipeintr->max_bufflen = exp_width;
						/* streaming mode uses bytes_count to show how many bytes are in *temp,
						 * but the interrupt re-entrant code uses bytes_read.
						 */
						pipeintr->bytes_read = bytes_count;
						pipeintr->bytes2read = bytes2read;
						pipeintr->char_count = bytes_count;
						pipeintr->add_bytes = add_bytes;
						pipeintr->bytes_count = bytes_count;
						PIPE_DEBUG(PRINTF("utf2 stream outofband "
							  "add_bytes %d bytes_count %d\n",
							  add_bytes, bytes_count); DEBUGPIPEFLUSH);
						rm_ptr->mupintr = TRUE;
						/* Don't step on our parade in the interrupt */
						stringpool.free += bytes_count;
						(TREF(pipefifo_interrupt))++;
						REVERT_GTMIO_CH(&io_curr_device, ch_set);
						outofband_action(FALSE);
						assertpro(FALSE);	/* Should *never* return from outofband_action */
						return FALSE;	/* For the compiler.. */
					}
					if (-1 == status)
					{
						rm_ptr->tot_bytes_in_buffer = chunk_bytes_read;
						tot_bytes_read = chunk_bytes_read;
						if (!rm_ptr->is_pipe && !rm_ptr->fifo)
							status = chunk_bytes_read;
					}
					else
						rm_ptr->tot_bytes_in_buffer = status;
				} else if (pipe_zero_timeout)
					out_of_time = FALSE;	/* reset out_of_time for pipe as no actual read is done */
				if (0 <= rm_ptr->tot_bytes_in_buffer)
				{
					min_bytes_to_copy = (rm_ptr->tot_bytes_in_buffer - rm_ptr->start_pos) >= 1 ? 1 : 0;
					assert(rm_ptr->start_pos <= rm_ptr->tot_bytes_in_buffer);
					/* Copy the char to inchar */
					inchar = rm_ptr->tmp_buffer[rm_ptr->start_pos];
					/* Increment start_pos. Only 1 byte in M mode */
					rm_ptr->start_pos += min_bytes_to_copy;
					/* Set status appropriately so that the following code can continue as if
					 * a one byte read happened. In case of a negative status, preserve the status
					 * as-is.
					 */
					status = (0 <= status) ? min_bytes_to_copy : status;
				}
				if (0 <= status)
				{
					bytes_read += status;			/* bytes read this pass */
					tot_bytes_read += bytes_read;		/* total bytes read this command */
					rm_ptr->file_pos += status;
					PIPE_DEBUG(PRINTF("M 5: status: %d bytes2read: %d bytes_read: %d "
							  "%d tot_bytes_read: %d\n", status, bytes2read, bytes_read,
							  tot_bytes_read); DEBUGPIPEFLUSH);
					if ((0 < bytes2read) && (0 == status))
					{	/* EOF  on read */
						SETZACANCELTIMER;
						break;			/* nothing read for this char so treat as EOF */
					}
					/* Check for the line terminators : NATIVE_NL */
					if (NATIVE_NL == inchar)
					{
						line_term_seen = TRUE;
						io_ptr->dollar.key[0] = io_ptr->dollar.zb[0] = NATIVE_NL;
						io_ptr->dollar.key[1] = io_ptr->dollar.zb[1] = '\0';
						if (!rdone)
							break;
					}
					*temp++ = inchar;
					PIPE_DEBUG(PRINTF("8: move inchar to *temp\n"); DEBUGPIPEFLUSH);
					bytes_count += status;
					PIPE_DEBUG(PRINTF("11: bytes_count: %d \n", bytes_count); DEBUGPIPEFLUSH);
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
				{
					buff_len = iorm_get_fol(io_ptr, &tot_bytes_read, &msec_timeout, timed, zint_restart,
								&follow_timeout, end_time);
					/* this will include the total bytes read including any stripped pad chars */
					fol_bytes_read = rm_ptr->fol_bytes_read;
				} else
				{
					buff_len = iorm_get(io_ptr, &blocked_in, rm_ptr->is_pipe, flags, &tot_bytes_read,
						    timer_id, &msec_timeout, pipe_zero_timeout, zint_restart);
					/* not using fol_bytes_read for non-follow mode */
					fol_bytes_read = buff_len;
				}
				if (0 > buff_len)
				{
					bytes_count = 0;
					if (errno == EINTR  &&  out_of_time)
						buff_len = -2;
				} else if (outofband && (fol_bytes_read < rm_ptr->recordsize))
				{
					PIPE_DEBUG(PRINTF(" %d utf fixed outofband, buff_len: %d done_1st_read: %d\n", pid,
							  buff_len, rm_ptr->done_1st_read); DEBUGPIPEFLUSH);
					PUSH_MV_STENT(MVST_ZINTDEV);
					mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
					mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = (char *)stringpool.free;
					mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = 0;
					mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
					pipeintr->who_saved = pipewhich_readfl;
					if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
					{
						pipeintr->end_time = end_time;
						pipeintr->end_time_valid = TRUE;
						if (!rm_ptr->follow)
							cancel_timer(timer_id);	/* Worry about timer if/when we come back */
					}
					pipeintr->max_bufflen = exp_width;
					/* since nothing read into stringpool.free set pipeintr->bytes_read to zero
					 as no bytes need to be copied in restart. */
					pipeintr->bytes_read = 0;
					rm_ptr->mupintr = TRUE;
					(TREF(pipefifo_interrupt))++;
					REVERT_GTMIO_CH(&io_curr_device, ch_set);
					outofband_action(FALSE);
					assertpro(FALSE);	/* Should *never* return from outofband_action */
					return FALSE;	/* For the compiler.. */
				}
				chset = io_ptr->ichset;		/* in case UTF-16 was changed */
			}
			status = tot_bytes_read = buff_len;		/* for EOF checking at the end */
			char_ptr = rm_ptr->inbuf_off;
			PIPE_DEBUG(PRINTF("iorm_readfl: inbuf: 0x%08lx, top: 0x%08lx, off: 0x%08lx\n", rm_ptr->inbuf,
					  rm_ptr->inbuf_top, rm_ptr->inbuf_off); DEBUGPIPEFLUSH;);
			PIPE_DEBUG(PRINTF("iorm_readfl: status: %d, width: %d", status, width); DEBUGPIPEFLUSH;);
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
						if ((2 <= (mblen = (rm_ptr->inbuf_top - char_ptr))) &&
						    UTF16BE_VALID(char_ptr, rm_ptr->inbuf_top, mblen))
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
						if ((2 <= (mblen = (rm_ptr->inbuf_top - char_ptr))) &&
						    UTF16LE_VALID(char_ptr, rm_ptr->inbuf_top, mblen))
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
						assertpro(FALSE);
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
						if (!IS_AT_END_OF_STRINGPOOL(temp, 0))
						{	/* make sure enough space to store buffer */
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
			PIPE_DEBUG(PRINTF("1: status: %d bytes2read: %d rm_ptr->start_pos: %d "
					  "rm_ptr->tot_bytes_in_buffer: %d char_bytes_read: %d add_bytes: %d\n",
					  status, bytes2read, rm_ptr->start_pos, rm_ptr->tot_bytes_in_buffer,
					  char_bytes_read, add_bytes); DEBUGPIPEFLUSH);
			char_start = rm_ptr->inbuf_off;
			if (rm_ptr->follow)
			{
				PIPE_DEBUG(PRINTF(" %d utf streaming with follow\n", pid); DEBUGPIPEFLUSH);
				/* rms-file device in follow mode */
				if (timed)
				{
					/* recalculate msec_timeout and sleep_left as &end_time - &current_time */
					if (0 < msec_timeout)
					{
						/* get the current time */
						sys_get_curr_time(&current_time);
						time_left = sub_abs_time(&end_time, &current_time);
						if (0 > time_left.at_sec)
						{
							msec_timeout = -1;
							out_of_time = TRUE;
						} else
							msec_timeout = (int4)(time_left.at_sec * MILLISECS_IN_SEC +
									DIVIDE_ROUND_UP(time_left.at_usec, MICROSECS_IN_MSEC));
						/* make sure it terminates with out_of_time */
						if (!out_of_time && !msec_timeout)
							msec_timeout = 1;
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
				/* In case the CHSET changes from non-UTF-16 to UTF-16 and a read has already been done,
				 * there's no way to read the BOM bytes & to determine the variant. So default to UTF-16BE.
				 */
				if (rm_ptr->done_1st_read &&
					 (!IS_UTF16_CHSET(rm_ptr->ichset_utf16_variant) && (CHSET_UTF16 == io_ptr->ichset)))
				{
					chset = io_ptr->ichset = rm_ptr->ichset_utf16_variant = CHSET_UTF16BE;
				}
				if (!rm_ptr->done_1st_read)
				{
					/* need to check BOM */
					if (rm_ptr->follow)
					{
						status = iorm_get_bom_fol(io_ptr, &tot_bytes_read, &msec_timeout, timed,
									  &bom_timeout, end_time);
					} else
						status = iorm_get_bom(io_ptr, &blocked_in, rm_ptr->is_pipe, flags, &tot_bytes_read,
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
						if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
						{
							pipeintr->end_time = end_time;
							pipeintr->end_time_valid = TRUE;
							if (!rm_ptr->follow)
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
						REVERT_GTMIO_CH(&io_curr_device, ch_set);
						outofband_action(FALSE);
						assertpro(FALSE);	/* Should *never* return from outofband_action */
						return FALSE;	/* For the compiler.. */
					}
					/* Set the UTF-16 variant if not already set, and has been determined right now */
					if (!IS_UTF16_CHSET(rm_ptr->ichset_utf16_variant) && (chset != io_ptr->ichset))
						rm_ptr->ichset_utf16_variant = io_ptr->ichset;
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
				if ((0 <= status) && (0 < bytes2read))
				{
					PIPE_DEBUG(PRINTF("3: status: %d bytes2read: %d rm_ptr->start_pos: %d "
							  "rm_ptr->tot_bytes_in_buffer: %d\n",
							  status, bytes2read, rm_ptr->start_pos,
							  rm_ptr->tot_bytes_in_buffer); DEBUGPIPEFLUSH);
					/* If it is a pipe and at least one character is read, a timer with timer_id
					   will be started.  It is canceled later in this routine if not expired
					   prior to return */
					if (rm_ptr->start_pos == rm_ptr->tot_bytes_in_buffer)
					{
						DEBUG_ONLY(MEMSET_IF_DEFINED(rm_ptr->tmp_buffer, 0, CHUNK_SIZE));
						rm_ptr->start_pos = rm_ptr->tot_bytes_in_buffer = 0;
						/* Read CHUNK_SIZE bytes from device into the temporary buffer. By doing this
						 * one-byte reads can be avoided when in UTF mode.
						 *
						 */
						if (rm_ptr->follow && (FALSE == bom_timeout))
						{
							/* In follow mode a read returns an EOF if no more bytes are available. */
							status = read(fildes, rm_ptr->tmp_buffer, CHUNK_SIZE);
							if (0 < status)
							{
								if (rm_ptr->input_encrypted)
									READ_ENCRYPTED_DATA(rm_ptr, io_ptr->trans_name,
										rm_ptr->tmp_buffer, status, NULL);
								rm_ptr->read_occurred = TRUE;
								rm_ptr->done_1st_read = TRUE;
							}
							if (0 == status) /* end of file */
							{
								if ((TRUE == timed) && (0 >= sleep_left))
								{
									follow_timeout = TRUE;
									break;
								}
								/* If a timed read, sleep the minimum of 100 ms and sleep_left.
								 * If not a timed read then just sleep 100 ms.
								 */
								if (TRUE == timed)
								{
									/* recalculate msec_timeout and sleep_left as
									 * &end_time - &current_time.
									 */
									/* get the current time */
									sys_get_curr_time(&current_time);
									time_left = sub_abs_time(&end_time, &current_time);
									if (0 > time_left.at_sec)
									{
										msec_timeout = -1;
										out_of_time = TRUE;
									} else
										msec_timeout = (int4)(time_left.at_sec *
												MILLISECS_IN_SEC +
												DIVIDE_ROUND_UP(time_left.at_usec,
												MICROSECS_IN_MSEC));
									/* make sure it terminates with out_of_time */
									if (!out_of_time && !msec_timeout)
										msec_timeout = 1;
									sleep_left = msec_timeout;
									sleep_time = MIN(100, sleep_left);
								} else
									sleep_time = 100;
								if (0 < sleep_time)
									SHORT_SLEEP(sleep_time);
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
									if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
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
									REVERT_GTMIO_CH(&io_curr_device, ch_set);
									outofband_action(FALSE);
									assertpro(FALSE);	/* Should *never* return from
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
						} else
						{
							DOREADRLTO2(fildes, rm_ptr->tmp_buffer, CHUNK_SIZE,
								    out_of_time, &blocked_in, rm_ptr->is_pipe, flags,
								    status, &utf_tot_bytes_read, timer_id,
								    &msec_timeout, pipe_zero_timeout, pipe_or_fifo, pipe_or_fifo);
							if ((0 < status) || ((0 > status) && (0 < utf_tot_bytes_read)))
							{
								if (rm_ptr->input_encrypted)
									READ_ENCRYPTED_DATA(rm_ptr, io_ptr->trans_name,
										rm_ptr->tmp_buffer,
										(status > 0) ? status : utf_tot_bytes_read, NULL);
								rm_ptr->read_occurred = TRUE;
								rm_ptr->done_1st_read = TRUE;
							}
						}
						PIPE_DEBUG(PRINTF("4: read chunk  status: %d utf_tot_bytes_read: %d\n",
								  status, utf_tot_bytes_read); DEBUGPIPEFLUSH);
						/* If status is -1, then total number of bytes read will be stored in
						 * utf_tot_bytes_read. If chunk read returned some bytes, then ignore outofband.
						 * We won't try a read again until bytes are processed.
						 */
						if ((pipe_or_fifo || rm_ptr->follow) && outofband && (0 >= status))
						{
							PIPE_DEBUG(PRINTF(" %d utf2 stream outofband\n", pid); DEBUGPIPEFLUSH);
							PUSH_MV_STENT(MVST_ZINTDEV);
							mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
							mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr =
								(char *)stringpool.free;
							mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = bytes_count;
							mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
							pipeintr->who_saved = pipewhich_readfl;
							if ((0 < msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
							{
								pipeintr->end_time = end_time;
								pipeintr->end_time_valid = TRUE;
								/* Worry about timer if/when we come back */
								if (!rm_ptr->follow)
									cancel_timer(timer_id);
							}
							pipeintr->max_bufflen = exp_width;
							/* streaming mode uses bytes_count to show how many bytes are in *temp,
							 * but the interrupt re-entrant code uses bytes_read.
							 */
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
							REVERT_GTMIO_CH(&io_curr_device, ch_set);
							outofband_action(FALSE);
							assertpro(FALSE);	/* Should *never* return from outofband_action */
							return FALSE;	/* For the compiler.. */
						}
						if (-1 == status)
						{
							rm_ptr->tot_bytes_in_buffer = utf_tot_bytes_read;
							tot_bytes_read = utf_tot_bytes_read;
							if (!rm_ptr->is_pipe && !rm_ptr->fifo)
								status = utf_tot_bytes_read;
						}
						else
							rm_ptr->tot_bytes_in_buffer = status;
					} else if (pipe_zero_timeout)
						out_of_time = FALSE;	/* reset out_of_time for pipe as no actual read is done */
					if (0 <= rm_ptr->tot_bytes_in_buffer)
					{
						min_bytes_to_copy = MIN(bytes2read,
									(rm_ptr->tot_bytes_in_buffer - rm_ptr->start_pos));
						assert(0 <= min_bytes_to_copy);
						assert(CHUNK_SIZE >= min_bytes_to_copy);
						assert(rm_ptr->start_pos <= rm_ptr->tot_bytes_in_buffer);
						/* If we have data in buffer, copy it to inbuf_pos */
						if (0 < min_bytes_to_copy)
						{
							/* If min_bytes_to_copy == 1, avoid memcpy by direct assignment */
							if (1 == min_bytes_to_copy)
								*rm_ptr->inbuf_pos = rm_ptr->tmp_buffer[rm_ptr->start_pos];
							else
							{
								memcpy(rm_ptr->inbuf_pos,
										rm_ptr->tmp_buffer + rm_ptr->start_pos,
										min_bytes_to_copy);
							}
						}
						/* Increment start_pos */
						rm_ptr->start_pos += min_bytes_to_copy;
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
					if ((0 < bytes2read) && (0 == status))
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
							if (ASCII_LF == *char_start)
							{
								if (rm_ptr->crlastbuff)
									assert(rm_ptr->crlast);
								if (rm_ptr->crlast && !rdone)
								{	/* ignore LF following CR */
									rm_ptr->crlast = FALSE;
									rm_ptr->inbuf_pos = char_start;
									bytes2read = 1;				/* reset */
									bytes_read = char_bytes_read = 0;	/* start fresh */
									tot_bytes_read--;
									if (rm_ptr->crlastbuff)
									{
										/* $KEY contains CR at this point. append LF.
										 * Also, CR was the last char of the previous buffer
										 * We needed to inspect this char to be LF and did
										 * not terminate the previous READ. Terminate it.
										 */
										rm_ptr->crlastbuff = FALSE;
										assert(ASCII_CR == io_ptr->dollar.key[0]);
										assert(ASCII_CR == io_ptr->dollar.zb[0]);
										SET_UTF8_DOLLARKEY_DOLLARZB(u32_line_term[U32_LT_LF]
											, &io_ptr->dollar.key[UTF8CRLEN],
											&io_ptr->dollar.zb[UTF8CRLEN]);
										break;
									}
									continue;
								} else
								{
									line_term_seen = TRUE;
									rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
									SET_UTF8_DOLLARKEY_DOLLARZB(u32_line_term[U32_LT_LF],
										 &io_ptr->dollar.key[0], &io_ptr->dollar.zb[0]);
									if (!rdone)
										break;
								}
							} else if (rm_ptr->crlastbuff)
							{
								/* CR was the last char of the previous buffer.
								 * We needed to inspect this char to be LF.
								 * The previous READ was not terminated, terminate it.
								 * Also reset the buffer pointers so this char goes into next read.
								 */
								rm_ptr->start_pos -= min_bytes_to_copy;
								rm_ptr->inbuf_pos = char_start;
								rm_ptr->crlastbuff = FALSE;
								if (!rdone)
									break;
							}
							rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
							if (ASCII_CR == *char_start)
							{
								rm_ptr->crlast = TRUE;
								line_term_seen = TRUE;
								SET_UTF8_DOLLARKEY_DOLLARZB(u32_line_term[U32_LT_CR],
									 &io_ptr->dollar.key[0], &io_ptr->dollar.zb[0]);
								/* Peep into the next char to see if it's LF and append it to $KEY.
								 * If The buffer ends with 'CR', the next char in the buffer not yet
								 * read can be LF. So don't terminate this read and force the next
								 * read to check for LF in 1st char.
								 * Same is the case if reading from BOM bytes.
								 */
								if (0 == rm_ptr->tot_bytes_in_buffer)
								{	/* Reading from BOM */
									if (rm_ptr->bom_buf_off == rm_ptr->bom_buf_cnt)
										rm_ptr->crlastbuff = TRUE;
									else
									{
										rm_ptr->crlastbuff = FALSE;
										if (ASCII_LF ==
											rm_ptr->bom_buf[rm_ptr->bom_buf_off])
										{
											SET_UTF8_DOLLARKEY_DOLLARZB(
											u32_line_term[U32_LT_LF],
											&io_ptr->dollar.key[UTF8CRLEN],
											&io_ptr->dollar.zb[UTF8CRLEN]);
										}
									}
								} else
								{	/* Reading from the buffer */
									if (rm_ptr->start_pos == rm_ptr->tot_bytes_in_buffer)
										rm_ptr->crlastbuff = TRUE;
									else
									{
										rm_ptr->crlastbuff = FALSE;
										if (ASCII_LF ==
											rm_ptr->tmp_buffer[rm_ptr->start_pos])
										{
											SET_UTF8_DOLLARKEY_DOLLARZB(
											u32_line_term[U32_LT_LF],
											&io_ptr->dollar.key[UTF8CRLEN],
											&io_ptr->dollar.zb[UTF8CRLEN]);
										}
									}
								}
								if (!rdone && !rm_ptr->crlastbuff)
									break;
							} else
							{
								rm_ptr->crlast = FALSE;
								rm_ptr->crlastbuff = FALSE;
							}
							if (ASCII_FF == *char_start)
							{
								line_term_seen = TRUE;
								SET_UTF8_DOLLARKEY_DOLLARZB(u32_line_term[U32_LT_FF],
									 &io_ptr->dollar.key[0], &io_ptr->dollar.zb[0]);
								if (!rdone)
									break;
							}
							if (!rm_ptr->crlastbuff)
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
								SET_UTF8_DOLLARKEY_DOLLARZB(utf_code, &io_ptr->dollar.key[0],
									&io_ptr->dollar.zb[0]);
								if (!rdone)
									break;
							}
							memcpy(temp, char_start, char_bytes_read);
							PIPE_DEBUG(PRINTF("10: move char_bytes_read: %d to *temp\n",
									  char_bytes_read); DEBUGPIPEFLUSH);
							temp += char_bytes_read;
						}
						if (!rm_ptr->crlastbuff)
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
										char_bytes_read, char_start, rm_ptr->inbuf_pos);
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
						if ( (WEOF == utf_code) || (nextmb != rm_ptr->inbuf_pos) )
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
							if (rm_ptr->crlastbuff)
								assert(rm_ptr->crlast);
							if (rm_ptr->crlast && !rdone)
							{	/* ignore LF following CR */
								rm_ptr->crlast = FALSE;
								rm_ptr->inbuf_pos = char_start;
								bytes2read = 2;				/* reset */
								bytes_read = char_bytes_read = 0;	/* start fresh */
								tot_bytes_read -= 2;
								if (rm_ptr->crlastbuff)
								{
									/* $KEY contains CR at this point. append LF
									 * Also, CR was the last char of the previous buffer.
									 * We needed to inspect this char to be LF and did
									 * not terminate the previous READ. Terminate it.
									 */
									rm_ptr->crlastbuff = FALSE;
									assert(io_ptr->dollar.key[0] == ASCII_CR);
									assert(io_ptr->dollar.zb[0] == ASCII_CR);
									SET_UTF8_DOLLARKEY_DOLLARZB(u32_line_term[U32_LT_LF],
										&io_ptr->dollar.key[UTF8CRLEN],
										&io_ptr->dollar.zb[UTF8CRLEN]);
									break;
								}
								continue;
							} else
							{
								line_term_seen = TRUE;
								rm_ptr->inbuf_off = rm_ptr->inbuf_pos;  /* mark as read */
								SET_UTF8_DOLLARKEY_DOLLARZB(u32_line_term[U32_LT_LF],
									&io_ptr->dollar.key[0], &io_ptr->dollar.zb[0]);
								if (!rdone)
									break;
							}
						} else if (rm_ptr->crlastbuff)
						{
							/* CR was the last char of the previous buffer.
							 * We needed to inspect this char for LF.
							 * The previous READ was not terminated, terminate it.
							 * Also reset the buffer pointers so this char goes into next read.
							 */
							rm_ptr->start_pos -= min_bytes_to_copy;
							rm_ptr->inbuf_pos = char_start;
							rm_ptr->crlastbuff = FALSE;
							if (!rdone)
								break;
						}
						rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
						for (ltind = 0; 0 < u32_line_term[ltind]; ltind++)
							if (u32_line_term[ltind] == utf_code)
							{
								line_term_seen = TRUE;
								SET_UTF8_DOLLARKEY_DOLLARZB(utf_code, &io_ptr->dollar.key[0],
									&io_ptr->dollar.zb[0]);
								break;
							}
						if (u32_line_term[U32_LT_CR] == utf_code)
						{
							rm_ptr->crlast = TRUE;
							/* Peep into the next char to see if it's LF and append it to $KEY.
							 * If The buffer ends with 'CR', the next char in the buffer not yet read
							 * can be LF. So don't terminate this read and force the next read
							 * to check for LF in 1st char.
							 * Same is the case if reading from BOM bytes.
							 */
							if (0 == rm_ptr->tot_bytes_in_buffer)
								rm_ptr->crlastbuff = TRUE;	/* CR read as BOM. */
							else
							{	/* Reading from buffer */
								if (rm_ptr->start_pos == rm_ptr->tot_bytes_in_buffer)
									rm_ptr->crlastbuff = TRUE;	/* CR last char of buf */
								else
								{
						 	 		/* This is UTF-16. Consider the BE/LE differences. */
									rm_ptr->crlastbuff = FALSE;
									if ((CHSET_UTF16BE == chset) ?
										((0x0 == rm_ptr->tmp_buffer[rm_ptr->start_pos]) &&
										 (ASCII_LF ==
										   rm_ptr->tmp_buffer[rm_ptr->start_pos+1])) :
										((0x0 == rm_ptr->tmp_buffer[rm_ptr->start_pos+1]) &&
										 (ASCII_LF ==
										   rm_ptr->tmp_buffer[rm_ptr->start_pos])))
									{
										SET_UTF8_DOLLARKEY_DOLLARZB(u32_line_term[U32_LT_LF]
										, &io_ptr->dollar.key[UTF8CRLEN],
										&io_ptr->dollar.zb[UTF8CRLEN]);
									}
								}
							}
						} else
						{
							rm_ptr->crlast = FALSE;
							rm_ptr->crlastbuff = FALSE;
						}
						if (line_term_seen && !rdone && !rm_ptr->crlastbuff)
							break;		/* out of do loop */
						if (!rm_ptr->crlastbuff)
						{
							temp_start = temp;
							temp = (char *)UTF8_WCTOMB(utf_code, temp_start);
							bytes_count += (int4)(temp - temp_start);
						}
						if (bytes_count > MAX_STRLEN)
						{	/* need to leave bytes for this character in buffer */
							bytes_count -= (int4)(temp - temp_start);
							rm_ptr->inbuf_off = char_start;
							rm_ptr->file_pos -= char_bytes_read;
							break;
						}
					} else
						assertpro(FALSE);
					if (!rm_ptr->crlastbuff)
					{
						char_count++;
					}
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
		CLEARERR(filstr);
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
		{	/* Cancel the timer before taking the error return */
			if (timed && !out_of_time)
				cancel_timer(timer_id);
			io_ptr->dollar.za = 9;
			/* save error in $device */
			SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, real_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) real_errno);
		}
		ret = FALSE;
	}
	if (timed)
	{	/* No timer if msec_timeout is zero, so handle the timer in the else. */
		if (msec_timeout == 0)
		{
			if (!rm_ptr->is_pipe || FALSE == blocked_in)
			{
				FCNTL3(fildes, F_SETFL, flags, fcntl_res);
				if (0 > fcntl_res)
				{
					save_errno = errno;
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(io_ptr, save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"),
						      CALLFROM, save_errno);
				}
			}
			if ((rm_ptr->is_pipe && (0 == status)) || (rm_ptr->fifo && (0 == status || real_errno == EAGAIN)))
				ret = FALSE;
		} else
		{
			if (out_of_time)
				ret = FALSE;
			else if (!rm_ptr->follow) /* if follow then no timer started */
				cancel_timer(timer_id);
		}
	}
	if ((0 == status) && (0 == tot_bytes_read))
	{
		v->str.len = 0;
		v->str.addr = (char *)stringpool.free;		/* ensure valid address */
		UNICODE_ONLY(v->str.char_len = 0;)
		if (rm_ptr->follow)
		{
			if (TRUE == io_ptr->dollar.zeof)
				io_ptr->dollar.zeof = FALSE; /* no EOF in follow mode */
			REVERT_GTMIO_CH(&io_curr_device, ch_set);
			return FALSE;
		}
		/* on end of file set $za to 9 */
		if (WBTEST_ENABLED(WBTEST_DOLLARDEVICE_BUFFER))
			SET_DOLLARDEVICE_ERRSTR(io_ptr, ONE_COMMA_DEV_DET_EOF_DOLLARDEVICE);
		else
			SET_DOLLARDEVICE_ERRSTR(io_ptr, ONE_COMMA_DEV_DET_EOF);
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
		if (rm_ptr->is_pipe)
		{
			if ((tot_bytes_read > 0) || (out_of_time && blocked_in))
				ret = TRUE;
			else
				ret = FALSE;
		}
		if (rm_ptr->follow && !rm_ptr->fixed && !line_term_seen)
		{
			if (utf_active)
				follow_width = char_count;
			else
				follow_width = bytes_count;
			if (!follow_width || (follow_width < width))
				ret = FALSE;
		}
		if (!utf_active || !rm_ptr->fixed)
		{	/* if Unicode and fixed, already setup the mstr */
			v->str.len = bytes_count;
			v->str.addr = (char *)stringpool.free;
			UNICODE_ONLY(v->str.char_len = char_count;)
			if (!utf_active)
				char_count = bytes_count;
		}
		if (!rm_ptr->fixed && line_term_seen)
		{
		    	*dollarx_ptr = 0;
			(*dollary_ptr)++;
		} else
		{
			*dollarx_ptr += char_count;
			if ((*dollarx_ptr >= io_ptr->width) && (rm_ptr->fixed || io_ptr->wrap))
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
	assert (FALSE == rm_ptr->mupintr);
	REVERT_GTMIO_CH(&io_curr_device, ch_set);
	return (rm_ptr->is_pipe && out_of_time) ? FALSE : ret;
}
