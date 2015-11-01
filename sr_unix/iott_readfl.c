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

#include "mdef.h"

#include <errno.h>
#include <string.h>

#include "iotcp_select.h"

#include "io_params.h"
#include "io.h"
#include "trmdef.h"
#include "iotimer.h"
#include "iottdef.h"
#include "stringpool.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "outofband.h"
#include "error.h"
#include "std_dev_outbndset.h"
#include "wake_alarm.h"

GBLDEF	int4		spc_inp_prc;			/* dummy: not used currently */
GBLDEF	bool		ctrlu_occurred;			/* dummy: not used currently */

GBLREF	io_pair		io_curr_device;
GBLREF	io_pair		io_std_device;
GBLREF	bool		prin_in_dev_failure;
GBLREF	spdesc		stringpool;
GBLREF	int4		outofband;
GBLREF	int4		ctrap_action_is;
GBLREF	bool		out_of_time;

#ifdef __MVS__
LITREF	unsigned char	ebcdic_lower_to_upper_table[];
GBLREF	unsigned char	e2a[];
GBLREF	unsigned char	a2e[];
#	define	INPUT_CHAR	asc_inchar
#	define	GETASCII(OUTPARM, INPARM)	{OUTPARM = e2a[INPARM];}
#	define	NATIVE_CVT2UPPER(OUTPARM, INPARM)	{OUTPARM = ebcdic_lower_to_upper_table[INPARM];}
#else
#	define	INPUT_CHAR	inchar
#	define	GETASCII(OUTPARM, INPARM)
#	define NATIVE_CVT2UPPER(OUTPARM, INPARM)       {OUTPARM = lower_to_upper_table[INPARM];}
#endif
LITREF	unsigned char	lower_to_upper_table[];

static readonly unsigned char	eraser[3] = { NATIVE_BS, NATIVE_SP, NATIVE_BS };
/* dc1 & dc3 have the same value in ASCII and EBCDIC */
static readonly char		dc1 = 17;
static readonly char		dc3 = 19;

short	iott_readfl (mval *v, int4 length, int4 timeout)	/* timeout in seconds */
{
	bool		ret, timed, zerotimeout;
	uint4		mask;
	unsigned char	inchar, *temp;
#ifdef __MVS__
	unsigned char	asc_inchar;
#endif
	short int	i, j, width;
	io_desc		*io_ptr;
	d_tt_struct	*tt_ptr;
	io_terminator	outofbands;
	io_termmask	mask_term;
	unsigned char	*zb_ptr, *zb_top;
	int		rdlen, selstat, status;
	int		msk_in, msk_num;
	int4		msec_timeout;			/* timeout in milliseconds */
	TID		timer_id;
	fd_set		input_fd;
	struct timeval	input_timeval;
	struct timeval	save_input_timeval;

	error_def(ERR_CTRAP);
	error_def(ERR_IOEOF);
	error_def(ERR_NOPRINCIO);

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);

	io_ptr = io_curr_device.in;
	tt_ptr = (d_tt_struct*) (io_ptr->dev_sp);
	assert (io_ptr->state == dev_open);
	iott_flush(io_curr_device.out);
	timer_id = (TID) iott_readfl;
	width = io_ptr->width;
	if (stringpool.free + length > stringpool.top)
		stp_gcol (length);
	i = 0;

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
	io_ptr->dollar.zeof = FALSE;
	j = io_ptr->dollar.x;
	ret = TRUE;
	out_of_time = FALSE;
	zerotimeout = FALSE;
	temp = stringpool.free;
	mask = tt_ptr->term_ctrl;
	mask_term = tt_ptr->mask_term;
	if (mask & TRM_NOTYPEAHD)
		TCFLUSH(tt_ptr->fildes, TCIOFLUSH, status);

	if (mask & TRM_READSYNC)
	{
		DOWRITERC(tt_ptr->fildes, &dc1, 1, status);
		if (0 != status)
		{
			io_ptr->dollar.za = 9;
			rts_error(VARLSTCNT(1) status);
		}
	}

	if (timeout == NO_M_TIMEOUT)
	{
		timed = FALSE;
		msec_timeout = NO_M_TIMEOUT;
	}
	else
	{
		timed = TRUE;
		msec_timeout = timeout2msec(timeout);

		if (msec_timeout == 0)
		{
			iott_mterm(io_ptr);
			zerotimeout = TRUE;
		}
	}

	do
	{
		if (outofband)
		{
			i = 0;
			if (timed)
			{
				if (msec_timeout == 0)
					iott_rterm(io_ptr);
			}
			outofband_action(FALSE);
			break;
		}
		errno = 0;

		FD_ZERO(&input_fd);
		FD_SET(tt_ptr->fildes, &input_fd);
		assert(FD_ISSET(tt_ptr->fildes, &input_fd) != 0);

		if (timed)
			input_timeval.tv_sec = timeout;
		else
			input_timeval.tv_sec  = 100;

		input_timeval.tv_usec = 0;

		/*
		 * the checks for EINTR below are valid and should not be converted to EINTR
		 * wrapper macros, since the select/read is not retried on EINTR.
		 */
                save_input_timeval = input_timeval;
		selstat = select(tt_ptr->fildes + 1, (void *)&input_fd,
					(void *)NULL, (void *)NULL, &input_timeval);
                input_timeval = save_input_timeval;

		if (selstat < 0)
		{
			if (errno != EINTR)
			{
				goto term_error;
			}

			if (out_of_time)
			{
				break;
			}
		}
		else if (selstat == 0)
		{
			if (timed)
			{
				wake_alarm();	/* sets out_of_time for zero timeout
							as well as non-zero timeout */
				break;
			}
			continue;	/* select() timeout; keep going */
		}
		else if ((rdlen = read(tt_ptr->fildes, &inchar, 1)) > 0)	/* This read is protected */
		{
			assert(FD_ISSET(tt_ptr->fildes, &input_fd) != 0);

			/* --------------------------------------------------
			 * set prin_in_dev_failure to FALSE to indicate that
			 * input device is working now.
			 * --------------------------------------------------
			 */
			prin_in_dev_failure = FALSE;

			if (tt_ptr->canonical)
			{
				if (inchar == 0)
				{
					/* --------------------------------------
					 * This means that the device has hungup
					 * --------------------------------------
					 */

					io_ptr->dollar.zeof = TRUE;
					io_ptr->dollar.x = 0;
					io_ptr->dollar.za = 9;
					io_ptr->dollar.y++;
					if (io_ptr->error_handler.len > 0)
					{
						rts_error(VARLSTCNT(1) ERR_IOEOF);
					}
					break;
				}
				else
				{
					io_ptr->dollar.zeof = FALSE;
				}
			}
			if (mask & TRM_CONVERT)
				NATIVE_CVT2UPPER(inchar, inchar);
                        GETASCII(asc_inchar,inchar);
			if ((j >= width) && io_ptr->wrap && !(mask & TRM_NOECHO))
			{
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
				j = 0;
			}

			if (INPUT_CHAR < ' '  &&  (tt_ptr->enbld_outofbands.mask & (1 << INPUT_CHAR)))
			{
				i = 0;
				io_ptr->dollar.za = 9;
				std_dev_outbndset(INPUT_CHAR);	/* it needs ASCII?	*/
				outofband = 0;
				if (timed)
				{
					if (msec_timeout == 0)
						iott_rterm(io_ptr);
				}
				rts_error(VARLSTCNT(3) ERR_CTRAP, 1, ctrap_action_is);
				break;
			}
			if ((   (mask & TRM_ESCAPE) != 0)
			     && (inchar == NATIVE_ESC  ||  io_ptr->esc_state != START))
			{
				if (zb_ptr >= zb_top)
				{
					/* -------------
					 * $zb overflow
					 * -------------
					 */

					io_ptr->dollar.za = 2;
					break;
				}
				*zb_ptr++ = inchar;
				iott_escape(zb_ptr - 1, zb_ptr, io_ptr);
				*(zb_ptr - 1) = INPUT_CHAR;     /* need to store ASCII value    */

				if (io_ptr->esc_state == FINI)
					break;
				if (io_ptr->esc_state == BADESC)
				{
					/* -----------------------------
					 * Escape sequence failed parse
					 * -----------------------------
					 */

					io_ptr->dollar.za = 2;
					break;
				}

				/* ---------------------------------------------
				 * In escape sequence...do not process further,
				 * but get next character
				 * ---------------------------------------------
				 */

				continue;
			}
			/* SIMPLIFY THIS! */
			msk_num = (uint4)INPUT_CHAR / NUM_BITS_IN_INT4;
			msk_in = (1 << ((uint4)INPUT_CHAR % NUM_BITS_IN_INT4));
			if (msk_in & mask_term.mask[msk_num])
			{
				*zb_ptr++ = INPUT_CHAR;
				break;
			}
			if (((int) inchar == tt_ptr->ttio_struct->c_cc[VERASE])
								&&  !(mask & TRM_PASTHRU))
			{
				if ((i > 0)  &&  (j > 0))
				{
					i--;
					j--;
					*temp--;
					if (!(mask & TRM_NOECHO))
					{
						DOWRITERC(tt_ptr->fildes, eraser, sizeof(eraser), status);
						if (0 != status)
							goto term_error;
					}
				}
			}
			else
			{
				if (!(mask & TRM_NOECHO))
				{
					DOWRITERC(tt_ptr->fildes, &inchar, 1, status);
					if (0 != status)
						goto term_error;
				}
				*temp++ = inchar;
				i++;
				j++;
			}
		}
		else if (rdlen == 0)
		{
			if (selstat > 0)	/* this should be the only possibility */
			{
				io_ptr->dollar.zeof = TRUE;
				io_ptr->dollar.x = 0;
				io_ptr->dollar.za = 0;
				io_ptr->dollar.y++;

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
				}
				else
				{
					io_ptr->dollar.zeof = TRUE;
					io_ptr->dollar.za   = 0;
					if (io_ptr->error_handler.len > 0)
						rts_error(VARLSTCNT(1) ERR_IOEOF);
				}
				break;
			}

			if (errno == 0) 	/* eof */
			{
				io_ptr->dollar.zeof = TRUE;
				io_ptr->dollar.x = 0;
				io_ptr->dollar.za = 0;
				io_ptr->dollar.y++;
				if (io_ptr->error_handler.len > 0)
				{
					rts_error(VARLSTCNT(1) ERR_IOEOF);
				}
				break;
			}
			else if (out_of_time)
			{
				break;
			}
		}
		else
		/* ----------
		 * rdlen < 0
		 * ----------
		 */
		{
			if (errno != EINTR)
				goto term_error;
			if (out_of_time)
				break;
		}
	}
	while (i < length);

	*zb_ptr++ = 0;
	if (timed)
	{
		if (msec_timeout == 0)
		{
			iott_rterm(io_ptr);
			if (i == 0)
				ret = FALSE;
		}
		else
		{
			if (out_of_time)
				ret = FALSE;
		}
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
		v->str.len = 0;
		io_ptr->dollar.za = 9;
		return(FALSE);
	}
	v->str.len = i;
	v->str.addr = (char*) stringpool.free;
	if (!(mask & TRM_NOECHO))
	{
		if ((io_ptr->dollar.x += v->str.len ) >= io_ptr->width && io_ptr->wrap)
		{
			io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
			if(io_ptr->length)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x %= io_ptr->width;
			if (io_ptr->dollar.x == 0)
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
		}
	}

	return((short) ret);

term_error:
	i = 0;
	io_ptr->dollar.za = 9;
	if (timed)
	{
		if (msec_timeout == 0)
			iott_rterm(io_ptr);
	}
	rts_error(VARLSTCNT(1) errno);
	return FALSE;
}
