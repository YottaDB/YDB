/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <wctype.h>
#include <wchar.h>
#include <signal.h>
#include "gtm_string.h"

#include "iotcp_select.h"

#include "io.h"
#include "iottdef.h"
#include "iott_edit.h"
#include "trmdef.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "outofband.h"
#include "error.h"
#include "std_dev_outbndset.h"
#include "wake_alarm.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF	int4		outofband;
GBLREF	io_pair		io_curr_device;
GBLREF	io_pair		io_std_device;
GBLREF	bool		prin_in_dev_failure;
GBLREF	int4		ctrap_action_is;
GBLREF	bool		out_of_time;
GBLREF	boolean_t	gtm_utf8_mode;

LITREF	unsigned char	lower_to_upper_table[];

int	iott_rdone (mint *v, int4 timeout)	/* timeout in seconds */
{
	boolean_t	ret = FALSE, timed, utf8_active;
	unsigned char	inbyte;
	wint_t		inchar;
#ifdef __MVS__
	wint_t		asc_inchar;
#endif
	char		dc1, dc3;
	short int	i;
	io_desc		*io_ptr;
	d_tt_struct	*tt_ptr;
	TID		timer_id;
	int		rdlen, selstat, status, utf8_more, inchar_width;
	int4		msec_timeout;		/* timeout in milliseconds */
	uint4		mask;
	int		msk_in, msk_num;
	unsigned char	*zb_ptr, *zb_top;
	unsigned char	more_buf[GTM_MB_LEN_MAX + 1], *more_ptr;	/* to build up multi byte for character */
	fd_set		input_fd;
	struct timeval	input_timeval;
	struct timeval	save_input_timeval;
	ABS_TIME	cur_time, end_time;

	error_def(ERR_CTRAP);
	error_def(ERR_IOEOF);
	error_def(ERR_NOPRINCIO);

	io_ptr = io_curr_device.in;
	assert (io_ptr->state == dev_open);
	iott_flush(io_curr_device.out);
	tt_ptr = (d_tt_struct*) io_ptr->dev_sp;
	timer_id = (TID) iott_rdone;
	*v = -1;
	dc1 = (char) 17;
	dc3 = (char) 19;
	mask = tt_ptr->term_ctrl;
	utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE;
	utf8_more = 0;

	/* ---------------------------------------------------------
	 * zb_ptr will be used to fill-in the value of $zb as we go
	 * ---------------------------------------------------------
	 */

	zb_ptr = io_ptr->dollar.zb;
	zb_top = zb_ptr + sizeof(io_ptr->dollar.zb) - 1;
	/* ----------------------------------------------------
	 * If we drop-out with error or otherwise permaturely,
	 * consider $zb to be null.
	 * ----------------------------------------------------
	 */
	*zb_ptr = 0;
	io_ptr->esc_state = START;
	io_ptr->dollar.za = 0;
	if (mask & TRM_NOTYPEAHD)
		TCFLUSH(tt_ptr->fildes, TCIFLUSH, status);
	if (mask & TRM_READSYNC)
	{
		DOWRITERC(tt_ptr->fildes, &dc1, 1, status);
		if (0 != status)
		{
			io_ptr->dollar.za = 9;
			rts_error(VARLSTCNT(1) status);
		}
	}
	out_of_time = FALSE;
	if (timeout == NO_M_TIMEOUT)
	{
		timed = FALSE;
		input_timeval.tv_sec  = 100;
		msec_timeout = NO_M_TIMEOUT;
	} else
	{
		timed = TRUE;
		input_timeval.tv_sec  = timeout;
		msec_timeout = timeout2msec(timeout);
		if (0 == msec_timeout)
			iott_mterm(io_ptr);
		else
		{
   			sys_get_curr_time(&cur_time);
			add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
		}
	}
	input_timeval.tv_usec = 0;
	do
	{
		if (outofband)
		{
			if (timed && (0 == msec_timeout))
				iott_rterm(io_ptr);
			outofband_action(FALSE);
			break;
		}
		FD_ZERO(&input_fd);
		FD_SET(tt_ptr->fildes, &input_fd);
		assert(FD_ISSET(tt_ptr->fildes, &input_fd) != 0);
		/* the checks for EINTR below are valid and should not be converted to EINTR
		 * wrapper macros, since the select/read is not retried on EINTR.
		 */
		save_input_timeval = input_timeval;	/* take a copy and pass it because select() below might change it */
		selstat = select(tt_ptr->fildes + 1, (void *)&input_fd, (void *)NULL, (void *)NULL, &input_timeval);
		if (0 > selstat)
		{
			if (EINTR != errno)
			{
				io_ptr->dollar.za = 9;
				if (timed && (0 == msec_timeout))
					iott_rterm(io_ptr);
				rts_error(VARLSTCNT(1) errno);
				break;
			}
		} else if (0 == selstat)
		{
			if (timed)
			{
				wake_alarm();	/* sets out_of_time to be true for zero as well as non-zero timeouts */
				break;
			}
			continue;	/* select() timeout; try again */
		}
		else if ((rdlen = read(tt_ptr->fildes, &inbyte, 1)) == 1)	/* This read is protected */
		{
			assert(FD_ISSET(tt_ptr->fildes, &input_fd) != 0);

			/* --------------------------------------------------
			 * set prin_in_dev_failure to FALSE to indicate that
			 * input device is working now.
			 * --------------------------------------------------
			 */
			prin_in_dev_failure = FALSE;

#ifdef UNICODE_SUPPORTED
			if (utf8_active)
			{
				if (tt_ptr->discard_lf)
				{	/* saw CR last time so ignore following LF */
					tt_ptr->discard_lf = FALSE;
					if (NATIVE_LF == inbyte)
						continue;
				}
				if (utf8_more)
				{	/* needed extra bytes */
					*more_ptr++ = inbyte;
					if (--utf8_more)
						continue;	/* get next byte */
					UTF8_MBTOWC(more_buf, more_ptr, inchar);
					if (WEOF == inchar)
					{	/* invalid char */
						io_ptr->dollar.za = 9;
						/* No data to return */
						iott_readfl_badchar(NULL, NULL, 0,
								    (more_ptr- more_buf), more_buf, more_ptr);
						utf8_badchar(more_ptr - more_buf, more_buf, more_ptr, 0, NULL);	/* ERR_BADCHAR */
						break;
					}
				} else
				{
					more_ptr = more_buf;
					if (0 < (utf8_more = UTF8_MBFOLLOW(&inbyte)))	/* assignment */
					{
						if (0 > utf8_more)
						{	/* invalid character */
							io_ptr->dollar.za = 9;
							*more_ptr++ = inbyte;
							 /* No data to return */
							iott_readfl_badchar(NULL, NULL, 0, 1, more_buf, more_ptr);
							utf8_badchar(1, more_buf, more_ptr, 0, NULL);	/* ERR_BADCHAR */
							break;
						} else if (GTM_MB_LEN_MAX < utf8_more)
						{	/* too big to be valid */
							io_ptr->dollar.za = 9;
							*more_ptr++ = inbyte;
							 /* No data to return */
							iott_readfl_badchar(NULL, NULL, 0, 1, more_buf, more_ptr);
							utf8_badchar(1, more_buf, more_ptr, 0, NULL);   /* ERR_BADCHAR */
							break;
						} else
						{
							*more_ptr++ = inbyte;
							continue;	/* get next byte */
						}
					} else
					{	/* single byte */
						*more_ptr++ = inbyte;
						UTF8_MBTOWC(more_buf, more_ptr, inchar);
						if (WEOF == inchar)
						{	/* invalid char */
							io_ptr->dollar.za = 9;
							 /* No data to return */
							iott_readfl_badchar(NULL, NULL, 0, 1, more_buf, more_ptr);
							utf8_badchar(1, more_buf, more_ptr, 0, NULL);   /* ERR_BADCHAR */
							break;
						}
					}
				}
				if (mask & TRM_CONVERT)
					inchar = u_toupper(inchar);
				GTM_IO_WCWIDTH(inchar, inchar_width);
			} else
			{
#endif
				if (mask & TRM_CONVERT)
					NATIVE_CVT2UPPER(inbyte, inbyte);
				inchar = inbyte;
				inchar_width = 1;
#ifdef UNICODE_SUPPORTED
			}
#endif

			GETASCII(asc_inchar, inchar);
			if (INPUT_CHAR < ' '  &&  ((1 << INPUT_CHAR) & tt_ptr->enbld_outofbands.mask))
			{
				std_dev_outbndset(INPUT_CHAR);
				if (timed)
				{
					if (0 == msec_timeout)
				  		iott_rterm(io_ptr);
				}
				rts_error(VARLSTCNT(3) ERR_CTRAP, 1, ctrap_action_is);
				ret = FALSE;
				break;
			}
			else if (   ((mask & TRM_ESCAPE) != 0)
				 && (inchar == NATIVE_ESC  ||  io_ptr->esc_state != START))
			{
				*v = INPUT_CHAR;
				ret = FALSE;
				do
				{
					if (zb_ptr >= zb_top UNICODE_ONLY(|| (utf8_active && ASCII_MAX < inchar)))
					{
						/* -------------
						 * $zb overflow
						 * -------------
						 */

						io_ptr->dollar.za = 2;
						break;
					}
					*zb_ptr++ = (unsigned char)inchar;
					iott_escape(zb_ptr - 1, zb_ptr, io_ptr);
					/*	RESTORE_DOLLARZB(asc_inchar);	*/
					*(zb_ptr - 1) = (unsigned char)INPUT_CHAR;	/* need to store ASCII value	*/
					if (io_ptr->esc_state == FINI)
					{
						ret = TRUE;
						break;
					}
					if (io_ptr->esc_state == BADESC)
					{
						/* ------------------------------
						 * Escape sequence failed parse.
						 * ------------------------------
						 */

						io_ptr->dollar.za = 2;
						break;
					}
					DOREADRL(tt_ptr->fildes, &inchar, 1, rdlen);
					GETASCII(asc_inchar, inchar);
				} while (1 == rdlen);

				*zb_ptr++ = 0;

				if (rdlen != 1  &&  io_ptr->dollar.za == 0)
					io_ptr->dollar.za = 9;

				/* -------------------------------------------------
				 * End of escape sequence...do not process further.
				 * -------------------------------------------------
				 */

				break;
			}
			else
			{
/* may need to deal with terminators > ASCII_MAX and/or LS and PS if default_mask_term */
				ret = TRUE;
				if (!utf8_active || ASCII_MAX >= INPUT_CHAR)
				{
					msk_num = (uint4)INPUT_CHAR / NUM_BITS_IN_INT4;
					msk_in = (1 << ((uint4)INPUT_CHAR % NUM_BITS_IN_INT4));
				} else
					msk_num = msk_in = 0;		/* force to not match terminator */
				if ((!(msk_in & tt_ptr->mask_term.mask[msk_num]))  &&  (!(mask & TRM_NOECHO)))
				{
					status = iott_write_raw(tt_ptr->fildes,
						utf8_active ? (void *)&inchar : (void *)&inbyte, 1);
					if (0 >= status)
					{
						status = errno;
						io_ptr->dollar.za = 9;
						if (timed)
						{
							if (0 == msec_timeout)
								iott_rterm(io_ptr);
						}
						rts_error(VARLSTCNT(1)  status);
					}
				}
				break;
			}
		} else if (rdlen < 0)
		{
			if (errno != EINTR)
			{
				io_ptr->dollar.za = 9;
				if (timed && (0 == msec_timeout))
					iott_rterm(io_ptr);
				rts_error(VARLSTCNT(1) errno);
				break;
			}
		} else
		/* ------------
		 * rdlen == 0
		 * ------------
		 */
		{	/* ---------------------------------------------------------
			 * select() says there's something to read, but
			 * read() found zero characters; assume connection dropped.
			 * ---------------------------------------------------------
			 */
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
			if (io_ptr->dollar.zeof)
			{
				io_ptr->dollar.za = 9;
				rts_error(VARLSTCNT(1) ERR_IOEOF);
			} else
			{
				io_ptr->dollar.zeof = TRUE;
				io_ptr->dollar.za   = 0;
				if (io_ptr->error_handler.len > 0)
					rts_error(VARLSTCNT(1) ERR_IOEOF);
			}
			break;
		}
		if (timed && (0 != msec_timeout))
		{
			sys_get_curr_time(&cur_time);
			cur_time = sub_abs_time(&end_time, &cur_time);
			if (0 > cur_time.at_sec)
			{
				ret = FALSE;
				break;
			}
			input_timeval.tv_sec = cur_time.at_sec;
			input_timeval.tv_usec = cur_time.at_usec;
		}
	} while (!out_of_time);

	if (timed)
	{
   		if (0 == msec_timeout)
	    		iott_rterm(io_ptr);
	}

	if (mask & TRM_READSYNC)
	{
		DOWRITERC(tt_ptr->fildes, &dc3, 1, status);
		if (0 != status)
		{
			io_ptr->dollar.za = 9;
			rts_error(VARLSTCNT(1) status);
		}
        }

	if (outofband)
	{
		io_ptr->dollar.za = 9;
		return FALSE;
	}
	io_ptr->dollar.za = 0;

	if (ret  &&  io_ptr->esc_state != FINI)
	{
		*v = INPUT_CHAR;
		if ((TT_EDITING & tt_ptr->ext_cap) && !((TRM_PASTHRU|TRM_NOECHO) & mask))
		{	/* keep above test in sync with iott_readfl */
			assert(tt_ptr->recall_buff.addr);
			if (!utf8_active)
			{
				tt_ptr->recall_buff.addr[0] = INPUT_CHAR;
				tt_ptr->recall_buff.len = 1;
			}
#ifdef UNICODE_SUPPORTED
			else
			{
				memcpy(tt_ptr->recall_buff.addr, &INPUT_CHAR, sizeof(INPUT_CHAR));
				tt_ptr->recall_buff.len = sizeof(INPUT_CHAR);
			}
#endif
			tt_ptr->recall_width = inchar_width;
		}
		/* SIMPLIFY THIS! */
		if (!utf8_active || ASCII_MAX >= INPUT_CHAR)
		{	/* may need changes to allow terminator > MAX_ASCII and/or LS and PS if default_mask_term */
			msk_num = (uint4)INPUT_CHAR / NUM_BITS_IN_INT4;
			msk_in = (1 << ((uint4)INPUT_CHAR % NUM_BITS_IN_INT4));
		} else
			msk_num = msk_in = 0;		/* force no match to terminator */
		if (msk_in & tt_ptr->mask_term.mask[msk_num])
		{
			*zb_ptr++ = INPUT_CHAR;
			*zb_ptr++ = 0;
		}
		else
		{
			io_ptr->dollar.zb[0] = '\0';
		}
		if ((!(msk_in & tt_ptr->mask_term.mask[msk_num])) && (!(mask & TRM_NOECHO)))
		{
			if ((io_ptr->dollar.x += inchar_width) >= io_ptr->width && io_ptr->wrap)
			{
				io_ptr->dollar.y = ++(io_ptr->dollar.y);
				if (io_ptr->length)
					io_ptr->dollar.y %= io_ptr->length;
				io_ptr->dollar.x %= io_ptr->width;
				if (io_ptr->dollar.x == 0)
					DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
			}
		}
	}

	return ret;
}
