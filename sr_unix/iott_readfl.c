/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"

#include "iotcp_select.h"
#include "io_params.h"
#include "io.h"
#include "trmdef.h"
#include "iotimer.h"
#include "iottdef.h"
#include "iott_edit.h"
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
GBLDEF	int		term_error_line;		/* record for cores */

GBLREF	io_pair		io_curr_device;
GBLREF	io_pair		io_std_device;
GBLREF	bool		prin_in_dev_failure;
GBLREF	spdesc		stringpool;
GBLREF	int4		outofband;
GBLREF	int4		ctrap_action_is;

GBLREF	int		AUTO_RIGHT_MARGIN, EAT_NEWLINE_GLITCH;
GBLREF	char		*CURSOR_UP, *CURSOR_DOWN, *CURSOR_LEFT, *CURSOR_RIGHT, *CLR_EOL;
GBLREF	char		*KEY_BACKSPACE, *KEY_DC;
GBLREF	char		*KEY_DOWN, *KEY_LEFT, *KEY_RIGHT, *KEY_UP;
GBLREF	char		*KEY_INSERT;
GBLREF	char		*KEYPAD_LOCAL, *KEYPAD_XMIT;

#ifdef __MVS__
LITREF	unsigned char	ebcdic_lower_to_upper_table[];
LITREF	unsigned char	e2a[];
#	define	INPUT_CHAR	asc_inchar
#	define	GETASCII(OUTPARM, INPARM)	{OUTPARM = e2a[INPARM];}
#	define	NATIVE_CVT2UPPER(OUTPARM, INPARM)	{OUTPARM = ebcdic_lower_to_upper_table[INPARM];}
#	define SEND_KEYPAD_LOCAL
#else
#	define	INPUT_CHAR	inchar
#	define	GETASCII(OUTPARM, INPARM)
#	define NATIVE_CVT2UPPER(OUTPARM, INPARM)       {OUTPARM = lower_to_upper_table[INPARM];}
#	define	SEND_KEYPAD_LOCAL					\
		if (edit_mode && NULL != KEYPAD_LOCAL && (keypad_len = strlen(KEYPAD_LOCAL)))	/* embedded assignment */	\
			DOWRITE(tt_ptr->fildes, KEYPAD_LOCAL, keypad_len);
#endif

LITREF	unsigned char	lower_to_upper_table[];

/* dc1 & dc3 have the same value in ASCII and EBCDIC */
static readonly char		dc1 = 17;
static readonly char		dc3 = 19;
static readonly unsigned char	eraser[3] = { NATIVE_BS, NATIVE_SP, NATIVE_BS };

short	iott_readfl (mval *v, int4 length, int4 timeout)	/* timeout in seconds */
{
	boolean_t	ret, nonzerotimeout, timed, insert_mode, edit_mode;
	uint4		mask;
	unsigned char	inchar, *temp, switch_char;
#ifdef __MVS__
	unsigned char	asc_inchar;
#endif
	int		dx, msk_in, msk_num, outlen, rdlen, save_errno, selstat, status, width;
	int		instr, keypad_len, dx_start, backspace, delete;
	int		up, down, right, left, insert_key;
	boolean_t	escape_edit;
	int4		msec_timeout;			/* timeout in milliseconds */
	io_desc		*io_ptr;
	d_tt_struct	*tt_ptr;
	io_terminator	outofbands;
	io_termmask	mask_term;
	unsigned char	*zb_ptr, *zb_top;
	ABS_TIME	cur_time, end_time;
	fd_set		input_fd;
	struct timeval	input_timeval;
	struct timeval	save_input_timeval;

	error_def(ERR_CTRAP);
	error_def(ERR_IOEOF);
	error_def(ERR_NOPRINCIO);

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	io_ptr = io_curr_device.in;
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);
	assert(dev_open == io_ptr->state);
	iott_flush(io_curr_device.out);
	width = io_ptr->width;
	if (stringpool.free + length > stringpool.top)
		stp_gcol (length);
	instr = outlen = 0;
	/* ---------------------------------------------------------
	 * zb_ptr is be used to fill-in the value of $zb as we go
	 * If we drop-out with error or otherwise permaturely,
	 * consider $zb to be null.
	 * ---------------------------------------------------------
	 */
	zb_ptr = io_ptr->dollar.zb;
	zb_top = zb_ptr + sizeof(io_ptr->dollar.zb) - 1;
	*zb_ptr = 0;
	io_ptr->esc_state = START;
	io_ptr->dollar.za = 0;
	io_ptr->dollar.zeof = FALSE;
	v->str.len = 0;
	dx_start = (int)io_ptr->dollar.x;
	ret = TRUE;
	temp = stringpool.free;
	mask = tt_ptr->term_ctrl;
	mask_term = tt_ptr->mask_term;
	/* keep test in next line in sync with test in iott_rdone.c */
	edit_mode = (0 != (TT_EDITING & tt_ptr->ext_cap) && !((TRM_NOECHO|TRM_PASTHRU) & mask));
	insert_mode = !(TT_NOINSERT & tt_ptr->ext_cap);	/* get initial mode */
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
	if (edit_mode)
	{	/* remove ESC and editing control characters from terminator list */
		mask_term.mask[ESC / NUM_BITS_IN_INT4] &= ~(1 << ESC);
		mask_term.mask[EDIT_SOL / NUM_BITS_IN_INT4] &= ~(1 << EDIT_SOL);
		mask_term.mask[EDIT_EOL / NUM_BITS_IN_INT4] &= ~(1 << EDIT_EOL);
		mask_term.mask[EDIT_DEOL / NUM_BITS_IN_INT4] &= ~(1 << EDIT_DEOL);
		mask_term.mask[EDIT_DELETE / NUM_BITS_IN_INT4] &= ~(1 << EDIT_DELETE);
		mask_term.mask[EDIT_LEFT / NUM_BITS_IN_INT4] &= ~(1 << EDIT_LEFT);
		mask_term.mask[EDIT_RIGHT / NUM_BITS_IN_INT4] &= ~(1 << EDIT_RIGHT);
		mask_term.mask[EDIT_ERASE / NUM_BITS_IN_INT4] &= ~(1 << EDIT_ERASE);
		/* to turn keypad on if possible */
#ifndef __MVS__
		if (NULL != KEYPAD_XMIT && (keypad_len = strlen(KEYPAD_XMIT)))	/* embedded assignment */
		{
			DOWRITERC(tt_ptr->fildes, KEYPAD_XMIT, keypad_len, status);
			if (0 != status)
			{
				io_ptr->dollar.za = 9;
				rts_error(VARLSTCNT(1) status);
			}
		}
#endif
		dx_start = (dx_start + width) % width;	/* normalize within width */
	}
	dx = dx_start;
	nonzerotimeout = FALSE;
	if (NO_M_TIMEOUT == timeout)
	{
		timed = FALSE;
		input_timeval.tv_sec = 100;
		msec_timeout = NO_M_TIMEOUT;
	} else
	{
		timed = TRUE;
		input_timeval.tv_sec = timeout;
		msec_timeout = timeout2msec(timeout);
		if (!msec_timeout)
			iott_mterm(io_ptr);
		else
		{
			nonzerotimeout = TRUE;
   			sys_get_curr_time(&cur_time);
			add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
		}
	}
	input_timeval.tv_usec = 0;
	do
	{
		if (outofband)
		{
			instr = outlen = 0;
			SEND_KEYPAD_LOCAL
			if (!msec_timeout)
				iott_rterm(io_ptr);
			outofband_action(FALSE);
			break;
		}
		errno = 0;
		FD_ZERO(&input_fd);
		FD_SET(tt_ptr->fildes, &input_fd);
		assert(0 != FD_ISSET(tt_ptr->fildes, &input_fd));
		/* the checks for EINTR below are valid and should not be converted to EINTR
		 * wrapper macros, since the select/read is not retried on EINTR.
		 */
		save_input_timeval = input_timeval;	/* take a copy and pass it because select() below might change it */
		selstat = select(tt_ptr->fildes + 1, (void *)&input_fd, (void *)NULL, (void *)NULL, &save_input_timeval);
		if (selstat < 0)
		{
			if (EINTR != errno)
			{
				term_error_line = __LINE__;
				goto term_error;
			}
		} else if (0 == selstat)
		{
			if (timed)
			{
				ret = FALSE;
				break;
			}
			continue;	/* select() timeout; keep going */
		} else if (0 < (rdlen = read(tt_ptr->fildes, &inchar, 1)))	/* This read is protected */
		{
			assert(0 != FD_ISSET(tt_ptr->fildes, &input_fd));
			/* --------------------------------------------------
			 * set prin_in_dev_failure to FALSE to indicate that
			 * input device is working now.
			 * --------------------------------------------------
			 */
			prin_in_dev_failure = FALSE;
			if (tt_ptr->canonical)
			{
				if (0 == inchar)
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
						rts_error(VARLSTCNT(1) ERR_IOEOF);
					break;
				} else
					io_ptr->dollar.zeof = FALSE;
			}
			if (mask & TRM_CONVERT)
				NATIVE_CVT2UPPER(inchar, inchar);
                        GETASCII(asc_inchar,inchar);
			if (!edit_mode && (dx >= width) && io_ptr->wrap && !(mask & TRM_NOECHO))
			{
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
				dx = 0;
			}
			if ((' ' > INPUT_CHAR) && (tt_ptr->enbld_outofbands.mask & (1 << INPUT_CHAR)))
			{	/* ctrap supercedes editing so check first */
				instr = outlen = 0;
				io_ptr->dollar.za = 9;
				std_dev_outbndset(INPUT_CHAR);	/* it needs ASCII?	*/
				outofband = 0;
				SEND_KEYPAD_LOCAL
				if (!msec_timeout)
					iott_rterm(io_ptr);
				rts_error(VARLSTCNT(3) ERR_CTRAP, 1, ctrap_action_is);
				break;
			}
			if (((0 != (mask & TRM_ESCAPE)) || edit_mode)
			     && ((NATIVE_ESC == inchar) || (START != io_ptr->esc_state)))
			{
				if (zb_ptr >= zb_top)
				{	/* $zb overflow */
					io_ptr->dollar.za = 2;
					break;
				}
				*zb_ptr++ = inchar;
				iott_escape(zb_ptr - 1, zb_ptr, io_ptr);
				*(zb_ptr - 1) = INPUT_CHAR;     /* need to store ASCII value    */
				if (FINI == io_ptr->esc_state && !edit_mode)
					break;
				if (BADESC == io_ptr->esc_state)
				{	/* Escape sequence failed parse */
					io_ptr->dollar.za = 2;
					break;
				}
				/* --------------------------------------------------------------------
				 * In escape sequence...do not process further, but get next character
				 * --------------------------------------------------------------------
				 */
			} else
			{	/* SIMPLIFY THIS! */
				msk_num = (uint4)INPUT_CHAR / NUM_BITS_IN_INT4;
				msk_in = (1 << ((uint4)INPUT_CHAR % NUM_BITS_IN_INT4));
				if (msk_in & mask_term.mask[msk_num])
				{
					*zb_ptr++ = INPUT_CHAR;
					break;
				}
				assert(0 <= instr);
				assert(0 <= dx);
				assert(outlen >= instr);
				if ((int)inchar == tt_ptr->ttio_struct->c_cc[VERASE]
					&& !(mask & TRM_PASTHRU))
				{
					if (0 < instr)
					{
						if (edit_mode)
						{
							if (!(mask & TRM_NOECHO))
								move_cursor_left(dx);
							dx = (dx - 1 + width) % width;
						} else
							dx--;
						stringpool.free[outlen] = ' ';
						if (!(mask & TRM_NOECHO))
						{
							if (!edit_mode)
								DOWRITERC(tt_ptr->fildes, eraser, sizeof(eraser), status)
							else
								status = write_str(temp, outlen - instr + 1, dx, FALSE);
							if (0 != status)
							{
								term_error_line = __LINE__;
								goto term_error;
							}
						}
						temp--;
						instr--;
						outlen--;
						if (outlen > instr)
							memmove(temp, temp + 1, outlen - instr);
					}
				} else
				{
					if (!edit_mode)
						switch_char = 'A';	/* force to default case */
					else
						switch_char = inchar;
					switch (switch_char)
					{
						case EDIT_SOL:	/* ctrl A  start of line */
						{
							int	num_lines_above;
							int	num_chars_left;

							num_lines_above = (instr + dx_start) /
										io_ptr->width;
							num_chars_left = dx - dx_start;
							if (!(mask & TRM_NOECHO))
							{
								if (0 != move_cursor(tt_ptr->fildes, num_lines_above,
										num_chars_left))
								{
									term_error_line = __LINE__;
									goto term_error;
								}
							}
							instr = 0;
							temp = stringpool.free;
							dx = dx_start;
							break;
						}
						case EDIT_EOL:	/* ctrl E  end of line */
						{
							int	num_lines_above;
							int	num_chars_left;

							num_lines_above =
								(instr + dx_start) / io_ptr->width -
									(outlen + dx_start) / io_ptr->width;
							num_chars_left = dx - (outlen + dx_start) % io_ptr->width;
							if (!(mask & TRM_NOECHO))
							{
								if (0 != move_cursor(tt_ptr->fildes, num_lines_above,
										num_chars_left))
								{
									term_error_line = __LINE__;
									goto term_error;
								}
							}
							instr = outlen;
							temp = stringpool.free + outlen;
							dx = (outlen + dx_start) % io_ptr->width;
							break;
						}
						case EDIT_LEFT:	/* ctrl B  left one */
						{
							if (instr != 0)
							{
								if (!(mask & TRM_NOECHO))
								{
									if (0 != move_cursor_left(dx))
									{
										term_error_line = __LINE__;
										goto term_error;
									}
								}
								temp--;
								instr--;
								dx = (dx - 1 + io_ptr->width) % io_ptr->width;
							}
							break;
						}
						case EDIT_RIGHT:	/* ctrl F  right one */
						{
							if (instr < outlen)
							{
								if (!(mask & TRM_NOECHO))
								{
									if (0 != move_cursor_right(dx))
									{
										term_error_line = __LINE__;
										goto term_error;
									}
								}
								temp++;
								instr++;
								dx = (dx + 1) % io_ptr->width;
							}
							break;
						}
						case EDIT_DEOL:	/* ctrl K  delete to end of line */
						{
							memset(temp, ' ', outlen - instr);
							if (!(mask & TRM_NOECHO))
							{
								if (0 != write_str(temp, outlen - instr, dx, FALSE))
								{
									term_error_line = __LINE__;
									goto term_error;
								}
							}
							outlen = instr;
							break;
						}
						case EDIT_ERASE:	/* ctrl U  delete whole line */
						{
							int	num_lines_above;
							int	num_chars_left;

							num_lines_above = (instr + dx_start) /
											io_ptr->width;
							num_chars_left = dx - dx_start;
							memset(stringpool.free, ' ', outlen);
							if (!(mask & TRM_NOECHO))
							{
								status = move_cursor(tt_ptr->fildes,
									num_lines_above, num_chars_left);
								if (0 != status || 0 != write_str(stringpool.free, outlen,
										dx_start, FALSE))
								{
									term_error_line = __LINE__;
									goto term_error;
								}
							}
							instr = 0;
							outlen = 0;
							dx = dx_start;
							temp = stringpool.free;
							break;
						}
						case EDIT_DELETE:	/* ctrl D delete char */
						{
							if (instr < outlen)
							{
								stringpool.free [outlen] = ' ';
								if (!(mask & TRM_NOECHO))
								{
									if (0 != write_str(temp + 1, outlen - instr, dx, FALSE))
									{
										term_error_line = __LINE__;
										goto term_error;
									}
								}
								memmove(temp, temp + 1, outlen - instr);
								outlen--;
							}
							break;
						}
						default:
						{
							if (insert_mode && (outlen > instr))
								memmove(temp + 1, temp, outlen - instr);
							*temp = inchar;
							if (!(mask & TRM_NOECHO))
							{
								if (!edit_mode)
								{
									DOWRITERC(tt_ptr->fildes, &inchar, 1, status);
									term_error_line = __LINE__;
								} else if (instr == outlen)
								{
									status = write_str(temp, 1, dx, TRUE);
									term_error_line = __LINE__;
								} else
								{
									status = write_str(temp,
										outlen - instr + (insert_mode ? 1 : 0), dx, FALSE);
									if (0 != status || 0 != (status = move_cursor_right(dx)))
									{
										term_error_line = __LINE__;
									}
								}
								if (0 != status)
									goto term_error;
							}
							temp++;
							if (insert_mode || instr == outlen)
								outlen++;
							instr++;
							if (!edit_mode)
								dx++;
							else
								dx = (dx + 1) % io_ptr->width;
							break;
						}
					}
				}
			}
		} else if (0 == rdlen)
		{
			if (0 < selstat)
			{	/* this should be the only possibility */
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
					SEND_KEYPAD_LOCAL
					rts_error(VARLSTCNT(1) ERR_IOEOF);
				} else
				{
					io_ptr->dollar.zeof = TRUE;
					io_ptr->dollar.za = 0;
					SEND_KEYPAD_LOCAL
					if (0 < io_ptr->error_handler.len)
						rts_error(VARLSTCNT(1) ERR_IOEOF);
				}
				break;
			}
			if (0 == errno)
			{	/* eof */
				io_ptr->dollar.zeof = TRUE;
				io_ptr->dollar.x = 0;
				io_ptr->dollar.za = 0;
				io_ptr->dollar.y++;
				SEND_KEYPAD_LOCAL
				if (0 < io_ptr->error_handler.len)
					rts_error(VARLSTCNT(1) ERR_IOEOF);
				break;
			}
		} else if (EINTR != errno)	/* rdlen < 0 */
		{
			term_error_line = __LINE__;
			goto term_error;
		}
		if (FINI ==  io_ptr->esc_state)
		{
			int zb_len = zb_ptr - io_ptr->dollar.zb;

			escape_edit = FALSE;
			down = strncmp((const char *)io_ptr->dollar.zb, KEY_DOWN, zb_len);
			up = strncmp((const char *)io_ptr->dollar.zb, KEY_UP, zb_len);
			right = strncmp((const char *)io_ptr->dollar.zb, KEY_RIGHT, zb_len);
			left = strncmp((const char *)io_ptr->dollar.zb, KEY_LEFT, zb_len);
			backspace = delete = insert_key = -1;

			if (KEY_BACKSPACE != NULL)
				backspace = strncmp((const char *)io_ptr->dollar.zb, KEY_BACKSPACE, zb_len);
			if (KEY_DC != NULL)
				delete = strncmp((const char *)io_ptr->dollar.zb, KEY_DC, zb_len);
			if (KEY_INSERT != NULL && '\0' != KEY_INSERT[0])
				insert_key = strncmp((const char *)io_ptr->dollar.zb, KEY_INSERT, zb_len);

			if (backspace == 0  ||  delete == 0)
			{
				if (instr > 0)
				{
					int temp_dx = dx;
					dx = (dx - 1 + io_ptr->width) % io_ptr->width;
					stringpool.free[outlen] = ' ';
					if (!(mask & TRM_NOECHO))
					{
						status = move_cursor_left(temp_dx);
						if (0 != status || 0 != write_str(temp, outlen - instr + 1, dx, FALSE))
						{
							term_error_line = __LINE__;
							goto term_error;
						}
					}
					temp--;
					instr--;
					outlen--;
					memmove(temp, temp + 1, outlen - instr);
				}
				escape_edit = TRUE;
			}

			if (up == 0  ||  down == 0)
			{
				/* move cursor to start of field */
				if (0 < instr)
				{
					if (0 != move_cursor(tt_ptr->fildes, ((instr + dx_start) / io_ptr->width), (dx - dx_start)))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
				}
				temp = stringpool.free;
				instr = tt_ptr->recall_buff.len;
				if (length < instr)
					instr = length;	/* restrict to length of read */
				if (0 != instr)
				{
					memcpy(temp, tt_ptr->recall_buff.addr, instr);
					if (0 != write_str(temp, instr, dx_start, TRUE))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
					temp += instr;
				}
				dx = (unsigned)(instr + dx_start) % io_ptr->width;
				if (instr < outlen)
				{	/* need to blank old output if longer */
					memset(temp, ' ', outlen - instr);
					if (0 != write_str(temp, outlen - instr, dx, FALSE))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
				}
				outlen = instr;
				escape_edit = TRUE;
			} else if (   !(mask & TRM_NOECHO)
				 && !(right == 0  &&  instr == outlen)
				 && !(left == 0   &&  instr == 0))
			{
				if (right == 0)
				{
					if (0 != move_cursor_right(dx))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
					temp++;
					instr++;
					dx = (dx + 1) % io_ptr->width;
				}
				if (left == 0)
				{
					if (0 != move_cursor_left(dx))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
					temp--;
					instr--;
					dx = (dx - 1 + io_ptr->width) % io_ptr->width;
				}
			}
			if (0 == insert_key)
				insert_mode = !insert_mode;	/* toggle */
			if (0 == right || 0 == left || 0 == insert_key)
				escape_edit = TRUE;
			if (escape_edit || (0 == (TRM_ESCAPE & mask)))
			{	/* reset dollar zb if editing function or not trm_escape */
				memset(io_ptr->dollar.zb, '\0', sizeof(io_ptr->dollar.zb));
				io_ptr->esc_state = START;
				zb_ptr = io_ptr->dollar.zb;
				zb_top = zb_ptr + sizeof(io_ptr->dollar.zb) - 1;
			} else
				break;	/* not edit function and TRM_ESCAPE */
		}
		if (nonzerotimeout)
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
	} while (outlen < length);
	*zb_ptr++ = 0;
	if (!msec_timeout)
	{
		iott_rterm(io_ptr);
		if (0 == outlen)
			ret = FALSE;
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
	SEND_KEYPAD_LOCAL	/* to turn keypad off if possible */
	if (outofband)
	{
		v->str.len = 0;
		io_ptr->dollar.za = 9;
		return(FALSE);
	}
	v->str.len = outlen;
	v->str.addr = (char *)stringpool.free;
	if (edit_mode)
	{	/* store in recall buffer */
		if (v->str.len > tt_ptr->recall_size)
		{
			if (tt_ptr->recall_buff.addr)
				free(tt_ptr->recall_buff.addr);
			tt_ptr->recall_buff.addr = malloc(v->str.len);
			tt_ptr->recall_size = v->str.len;
		}
		memcpy(tt_ptr->recall_buff.addr, v->str.addr, v->str.len);
		tt_ptr->recall_buff.len = v->str.len;
	}
	if (!(mask & TRM_NOECHO))
	{
		if ((io_ptr->dollar.x += v->str.len) >= io_ptr->width && io_ptr->wrap)
		{
			io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
			if (io_ptr->length)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x %= io_ptr->width;
			if (0 == io_ptr->dollar.x)
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
		}
	}
	return ((short)ret);

term_error:
	save_errno = errno;
	io_ptr->dollar.za = 9;
	SEND_KEYPAD_LOCAL	/* to turn keypad off if possible */
	if (!msec_timeout)
		iott_rterm(io_ptr);
	rts_error(VARLSTCNT(1) save_errno);
	return FALSE;
}
