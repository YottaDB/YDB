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
#include <signal.h>
#include "gtm_string.h"

#include "iotcp_select.h"

#include "io.h"
#include "iottdef.h"
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

GBLREF	int4		outofband;
GBLREF	io_pair		io_curr_device;
GBLREF	io_pair		io_std_device;
GBLREF	bool		prin_in_dev_failure;
GBLREF	int4		ctrap_action_is;
GBLREF	bool		out_of_time;

#ifdef __MVS__
LITREF	unsigned char	ebcdic_lower_to_upper_table[];
LITREF	unsigned char	e2a[];
#define	INPUT_CHAR	asc_inchar
#define	GETASCII(OUTPARM, INPARM)	{OUTPARM = e2a[INPARM];}
#define	NATIVE_CVT2UPPER(OUTPARM, INPARM)	{OUTPARM = ebcdic_lower_to_upper_table[INPARM];}
#else
#define	INPUT_CHAR	inchar
#define	GETASCII(OUTPARM, INPARM)
#define NATIVE_CVT2UPPER(OUTPARM, INPARM)       {OUTPARM = lower_to_upper_table[INPARM];}
#endif
LITREF	unsigned char	lower_to_upper_table[];

short	iott_rdone (mint *v, int4 timeout)	/* timeout in seconds */
{
	bool		ret = FALSE, timed, zerotimeout;
	unsigned char	inchar;
#ifdef __MVS__
	unsigned char	asc_inchar;
#endif
	char		dc1, dc3;
	short int	i;
	io_desc		*io_ptr;
	d_tt_struct	*tt_ptr;
	TID		timer_id;
	int		rdlen, selstat, status;
	int4		msec_timeout;		/* timeout in milliseconds */
	uint4		mask;
	int		msk_in, msk_num;
	unsigned char	*zb_ptr, *zb_top;
	fd_set		input_fd;
	struct timeval	input_timeval;
	struct timeval	save_input_timeval;

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
	zerotimeout = FALSE;

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
			if (timed)
			{
				if (msec_timeout == 0)
					iott_rterm(io_ptr);
			}
			outofband_action(FALSE);
			break;
		}

		FD_ZERO(&input_fd);
		FD_SET(tt_ptr->fildes, &input_fd);
		assert(FD_ISSET(tt_ptr->fildes, &input_fd) != 0);

		if (timed)
			input_timeval.tv_sec  = timeout;
		else
			input_timeval.tv_sec  = 100;

		input_timeval.tv_usec = 0;

		/*
		 * the checks for EINTR below are valid and should not be converted to EINTR
		 * wrapper macros, since the select/read is not retried on EINTR.
		 */
                save_input_timeval = input_timeval;
		selstat = select(tt_ptr->fildes + 1, (void *)&input_fd, (void *)NULL, (void *)NULL, &input_timeval);
                input_timeval = save_input_timeval;
		if (selstat < 0)
		{
			if (errno != EINTR)
			{
				io_ptr->dollar.za = 9;
				if (timed)
				{
					if (0 == msec_timeout)
						iott_rterm(io_ptr);
				}
				rts_error(VARLSTCNT(1) errno);
				break;
			}
		}
		else if (selstat == 0)
		{
			if (timed)
			{
				wake_alarm();	/* sets out_of_time to be true for
							zero as well as non-zero timeouts */
				break;
			}
			continue;	/* select() timeout; try again */
		}
		else if ((rdlen = read(tt_ptr->fildes, &inchar, 1)) == 1)	/* This read is protected */
		{
			assert(FD_ISSET(tt_ptr->fildes, &input_fd) != 0);

			/* --------------------------------------------------
			 * set prin_in_dev_failure to FALSE to indicate that
			 * input device is working now.
			 * --------------------------------------------------
			 */
			prin_in_dev_failure = FALSE;

			if (mask & TRM_CONVERT)
				NATIVE_CVT2UPPER(inchar, inchar);
			GETASCII(asc_inchar, inchar);
			if (INPUT_CHAR < ' '  &&  ((1 << INPUT_CHAR) & tt_ptr->enbld_outofbands.mask))
			{
				std_dev_outbndset(INPUT_CHAR);
				if (timed)
				{
					if (msec_timeout == 0)
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
					/*	RESTORE_DOLLARZB(asc_inchar);	*/
					*(zb_ptr - 1) = INPUT_CHAR;	/* need to store ASCII value	*/
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
				ret = TRUE;
				msk_num = (uint4)INPUT_CHAR / NUM_BITS_IN_INT4;
				msk_in = (1 << ((uint4)INPUT_CHAR % NUM_BITS_IN_INT4));
				if ((!(msk_in & tt_ptr->mask_term.mask[msk_num]))  &&  (!(mask & TRM_NOECHO)))
				{
					DOWRITERC(tt_ptr->fildes, &inchar, 1, status);
					if (0 != status)
					{
						io_ptr->dollar.za = 9;
						if (timed)
						{
							if (msec_timeout == 0)
								iott_rterm(io_ptr);
						}
						rts_error(VARLSTCNT(1)  status);
					}
				}
				break;
			}
		}
		else if (rdlen < 0)
		{
			if (errno != EINTR)
			{
				io_ptr->dollar.za = 9;
				if (timed)
				{
					if (msec_timeout == 0)
						iott_rterm(io_ptr);
				}
				rts_error(VARLSTCNT(1) errno);
				break;
			}
		}
		else
		/* ------------
		 * rdlen == 0
		 * ------------
		 */
		{
			/* ---------------------------------------------------------
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
	} while (!out_of_time);

	if (timed)
	{
   		if (msec_timeout == 0)
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
			tt_ptr->recall_buff.addr[0] = INPUT_CHAR;
			tt_ptr->recall_buff.len = 1;
		}
		/* SIMPLIFY THIS! */
		msk_num = (uint4)INPUT_CHAR / NUM_BITS_IN_INT4;
		msk_in = (1 << ((uint4)INPUT_CHAR % NUM_BITS_IN_INT4));
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
			if ((io_ptr->dollar.x += 1) >= io_ptr->width && io_ptr->wrap)
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

	return((short) ret);
}
