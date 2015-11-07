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
#include <wctype.h>
#include <wchar.h>
#include "gtm_string.h"

#include "iotcp_select.h"
#include "io_params.h"
#include "io.h"
#include "trmdef.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "iottdef.h"
#include "iott_edit.h"
#include "stringpool.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "outofband.h"
#include "error.h"
#include "std_dev_outbndset.h"
#include "wake_alarm.h"
#include "min_max.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLDEF	int4		spc_inp_prc;			/* dummy: not used currently */
GBLDEF	bool		ctrlu_occurred;			/* dummy: not used currently */
GBLDEF	int		term_error_line;		/* record for cores */

GBLREF	io_pair		io_curr_device;
GBLREF	io_pair		io_std_device;
GBLREF	bool		prin_in_dev_failure;
GBLREF	spdesc		stringpool;
GBLREF	volatile int4	outofband;
GBLREF	mv_stent	*mv_chain;
GBLREF	stack_frame	*frame_pointer;
GBLREF	unsigned char	*msp, *stackbase, *stacktop, *stackwarn;
GBLREF	boolean_t	dollar_zininterrupt;
GBLREF	int4		ctrap_action_is;
GBLREF	boolean_t	gtm_utf8_mode;

#ifdef UNICODE_SUPPORTED
LITREF	UChar32		u32_line_term[];
#endif

GBLREF	int		AUTO_RIGHT_MARGIN, EAT_NEWLINE_GLITCH;
GBLREF	char		*CURSOR_UP, *CURSOR_DOWN, *CURSOR_LEFT, *CURSOR_RIGHT, *CLR_EOL;
GBLREF	char		*KEY_BACKSPACE, *KEY_DC;
GBLREF	char		*KEY_DOWN, *KEY_LEFT, *KEY_RIGHT, *KEY_UP;
GBLREF	char		*KEY_INSERT;
GBLREF	char		*KEYPAD_LOCAL, *KEYPAD_XMIT;

#ifdef __MVS__
#	define SEND_KEYPAD_LOCAL
#else
#	define	SEND_KEYPAD_LOCAL					\
		if (edit_mode && NULL != KEYPAD_LOCAL && (keypad_len = STRLEN(KEYPAD_LOCAL)))	/* embedded assignment */	\
			DOWRITE(tt_ptr->fildes, KEYPAD_LOCAL, keypad_len);
#endif

LITREF	unsigned char	lower_to_upper_table[];

/* dc1 & dc3 have the same value in ASCII and EBCDIC */
static readonly char		dc1 = 17;
static readonly char		dc3 = 19;
static readonly unsigned char	eraser[3] = { NATIVE_BS, NATIVE_SP, NATIVE_BS };

error_def(ERR_CTRAP);
error_def(ERR_IOEOF);
error_def(ERR_NOPRINCIO);
error_def(ERR_ZINTRECURSEIO);
error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

#ifdef UNICODE_SUPPORTED

/* Maintenance of $ZB on a badchar error and returning partial data (if any) */
void iott_readfl_badchar(mval *vmvalptr, wint_t *dataptr32, int datalen,
			 int delimlen, unsigned char *delimptr, unsigned char *strend, unsigned char *buffer_start)
{
        int             i, tmplen, len;
        unsigned char   *delimend, *outptr, *outtop;
	wint_t		*curptr32;
        io_desc         *iod;

        if (0 < datalen && NULL != dataptr32)
        {       /* Return how much input we got */
		if (gtm_utf8_mode)
		{
			outptr = buffer_start;
			outtop = ((unsigned char *)dataptr32);
			curptr32 = dataptr32;
			for (i = 0; i < datalen && outptr < outtop; i++, curptr32++)
				outptr = UTF8_WCTOMB(*curptr32, outptr);
			vmvalptr->str.len = INTCAST(outptr - buffer_start);
		} else
			vmvalptr->str.len = datalen;
		vmvalptr->str.addr = (char *)buffer_start;
		if (buffer_start == stringpool.free)
			stringpool.free += vmvalptr->str.len;	/* The BADCHAR error after this won't do this for us */
        }
        if (NULL != strend && NULL != delimptr)
        {       /* First find the end of the delimiter (max of 4 bytes) */
                if (0 == delimlen)
		{
			for (delimend = delimptr; 4 >= delimlen && delimend < strend; ++delimend, ++delimlen)
			{
				if (UTF8_VALID(delimend, strend, tmplen))
					break;
			}
                }
                if (0 < delimlen)
                {       /* Set $ZB with the failing badchar */
                        iod = io_curr_device.in;
                        memcpy(iod->dollar.zb, delimptr, MIN(delimlen, ESC_LEN - 1));
                        iod->dollar.zb[MIN(delimlen, ESC_LEN - 1)] = '\0';
                        memcpy(iod->dollar.key, delimptr, MIN(delimlen, DD_BUFLEN - 1));
                        iod->dollar.key[MIN(delimlen, DD_BUFLEN - 1)] = '\0';
                }
        }
}
#endif

int	iott_readfl(mval *v, int4 length, int4 timeout)	/* timeout in seconds */
{
	boolean_t	ret, nonzerotimeout, timed, insert_mode, edit_mode, utf8_active, zint_restart, buffer_moved;
	uint4		mask;
	wint_t		inchar, *current_32_ptr, *buffer_32_start, switch_char;
	unsigned char	inbyte, *outptr, *outtop;
#ifdef __MVS__
	wint_t		asc_inchar;
#endif
	unsigned char	more_buf[GTM_MB_LEN_MAX + 1], *more_ptr;	/* to build up multi byte for character */
	unsigned char	*current_ptr;		/* insert next character into buffer here */
	unsigned char	*buffer_start;		/* beginning of non UTF8 buffer */
	int		msk_in, msk_num, rdlen, save_errno, selstat, status, ioptr_width, i, utf8_more;
	int		exp_length;
	int		inchar_width;		/* display width of inchar */
	int		delchar_width;		/* display width of deleted char */
	int		delta_width;		/* display width change for replaced char */
	int		dx, dx_start;		/* local dollar X, starting value */
	int		dx_instr, dx_outlen;	/* wcwidth of string to insert point, whole string */
	int		dx_prev, dx_cur, dx_next;/* wcwidth of string to char BEFORE, AT and AFTER the insert point */
	int		instr;			/* insert point in input string */
	int		outlen;			/* total characters in line so far */
	int		keypad_len, backspace, delete;
	int		up, down, right, left, insert_key;
	boolean_t	escape_edit, empterm;
	int4		msec_timeout;		/* timeout in milliseconds */
	io_desc		*io_ptr;
	d_tt_struct	*tt_ptr;
	io_terminator	outofbands;
	io_termmask	mask_term;
	tt_interrupt	*tt_state;
	mv_stent	*mvc, *mv_zintdev;
	unsigned char	*zb_ptr, *zb_top;
	ABS_TIME	cur_time, end_time;
	fd_set		input_fd;
	struct timeval	input_timeval;
	struct timeval	save_input_timeval;

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	io_ptr = io_curr_device.in;
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);
	assert(dev_open == io_ptr->state);
	iott_flush(io_curr_device.out);
	insert_mode = !(TT_NOINSERT & tt_ptr->ext_cap);	/* get initial mode */
	empterm	= (TT_EMPTERM & tt_ptr->ext_cap);
	ioptr_width = io_ptr->width;
	utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE;
	/* if utf8_active, need room for multi byte characters plus wint_t buffer */
	exp_length = utf8_active ? (int)(((SIZEOF(wint_t) * length) + (GTM_MB_LEN_MAX * length) + SIZEOF(gtm_int64_t))) : length;
	zint_restart = FALSE;
	if (tt_ptr->mupintr)
	{	/* restore state to before job interrupt */
		tt_state = &tt_ptr->tt_state_save;
		if (ttwhichinvalid == tt_state->who_saved)
			GTMASSERT;
		if (dollar_zininterrupt)
		{
			tt_ptr->mupintr = FALSE;
			tt_state->who_saved = ttwhichinvalid;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
		}
		assert(length == tt_state->length);
		if (ttread != tt_state->who_saved)
			GTMASSERT;	/* ZINTRECURSEIO should have caught */
		mv_zintdev = io_find_mvstent(io_ptr, FALSE);
		if (NULL != mv_zintdev && mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid)
		{	/* looks good so use it */
			buffer_start = (unsigned char *)mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr;
			current_ptr = buffer_start;
			mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
			mv_zintdev->mv_st_cont.mvs_zintdev.io_ptr = NULL;
			if (mv_chain == mv_zintdev)
				POP_MV_STENT();		/* pop if top of stack */
			buffer_moved = (buffer_start != tt_state->buffer_start);
			if (utf8_active)
			{	/* need to properly align U32 buffer */
				assert(exp_length == tt_state->exp_length);
				buffer_32_start = (wint_t *)ROUND_UP2((INTPTR_T)(buffer_start + (GTM_MB_LEN_MAX * length)),
							SIZEOF(gtm_int64_t));
				if (buffer_moved && (((unsigned char *)buffer_32_start - buffer_start)
						!= ((unsigned char *)tt_state->buffer_32_start - tt_state->buffer_start)))
					memmove(buffer_32_start, buffer_start + ((unsigned char *)tt_state->buffer_32_start
						- tt_state->buffer_start), (SIZEOF(wint_t) * length));
				current_32_ptr = buffer_32_start;
				utf8_more = tt_state->utf8_more;
				more_ptr = tt_state->more_ptr;
				memcpy(more_buf, tt_state->more_buf, SIZEOF(more_buf));
			}
			instr = tt_state->instr;
			outlen = tt_state->outlen;
			dx = tt_state->dx;
			dx_start = tt_state->dx_start;
			dx_instr = tt_state->dx_instr;
			dx_outlen = tt_state->dx_outlen;
			insert_mode = tt_state->insert_mode;
			end_time = tt_state->end_time;
			zb_ptr = tt_state->zb_ptr;
			zb_top = tt_state->zb_top;
			tt_state->who_saved = ttwhichinvalid;
			tt_ptr->mupintr = FALSE;
			zint_restart = TRUE;
		}
	}
	if (!zint_restart)
	{
		ENSURE_STP_FREE_SPACE(exp_length);
		buffer_start = current_ptr = stringpool.free;
		if (utf8_active)
		{
			buffer_32_start = (wint_t *)ROUND_UP2((INTPTR_T)(stringpool.free + (GTM_MB_LEN_MAX * length)),
					SIZEOF(gtm_int64_t));
			current_32_ptr = buffer_32_start;
		}
		instr = outlen = 0;
		dx_instr = dx_outlen = 0;
		utf8_more = 0;
		/* ---------------------------------------------------------
		 * zb_ptr is used to fill-in the value of $zb as we go
		 * If we drop-out with error or otherwise permaturely,
		 * consider $zb to be null.
		 * ---------------------------------------------------------
		 */
		zb_ptr = io_ptr->dollar.zb;
		zb_top = zb_ptr + SIZEOF(io_ptr->dollar.zb) - 1;
		*zb_ptr = 0;
		io_ptr->esc_state = START;
		io_ptr->dollar.za = 0;
		io_ptr->dollar.zeof = FALSE;
		dx_start = (int)io_ptr->dollar.x;
	}
	v->str.len = 0;
	ret = TRUE;
	mask = tt_ptr->term_ctrl;
	mask_term = tt_ptr->mask_term;
	/* keep test in next line in sync with test in iott_rdone.c */
	edit_mode = (0 != (TT_EDITING & tt_ptr->ext_cap) && !((TRM_NOECHO|TRM_PASTHRU) & mask));
	if (!zint_restart)
	{
		if (mask & TRM_NOTYPEAHD)
			TCFLUSH(tt_ptr->fildes, TCIFLUSH, status);
		if (mask & TRM_READSYNC)
		{
			DOWRITERC(tt_ptr->fildes, &dc1, 1, status);
			if (0 != status)
			{
				io_ptr->dollar.za = 9;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
			}
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
		if (!zint_restart)
		{
			/* to turn keypad on if possible */
#ifndef __MVS__
			if (NULL != KEYPAD_XMIT && (keypad_len = STRLEN(KEYPAD_XMIT)))	/* embedded assignment */
			{
				DOWRITERC(tt_ptr->fildes, KEYPAD_XMIT, keypad_len, status);
				if (0 != status)
				{
					io_ptr->dollar.za = 9;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
				}
			}
#endif
			dx_start = (dx_start + ioptr_width) % ioptr_width;	/* normalize within width */
		}
	}
	if (!zint_restart)
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
		{
			if (!zint_restart)
				iott_mterm(io_ptr);
		} else
		{
			nonzerotimeout = TRUE;
   			sys_get_curr_time(&cur_time);
			if (!zint_restart)
				add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
		}
	}
	input_timeval.tv_usec = 0;
	do
	{
		if (outofband)
		{
			if (jobinterrupt == outofband)
			{	/* save state if jobinterrupt */
				tt_state = &tt_ptr->tt_state_save;
				tt_state->who_saved = ttread;
				tt_state->length = length;
				tt_state->buffer_start = buffer_start;
				PUSH_MV_STENT(MVST_ZINTDEV);
				mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = (char *)buffer_start;
				mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = exp_length;
				mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
				mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
				if (utf8_active)
				{
					tt_state->exp_length = exp_length;
					tt_state->buffer_32_start = buffer_32_start;
					tt_state->utf8_more = utf8_more;
					tt_state->more_ptr = more_ptr;
					memcpy(tt_state->more_buf, more_buf, SIZEOF(more_buf));
				}
				if (buffer_start == stringpool.free)
					stringpool.free += exp_length;	/* reserve space */
				tt_state->instr = instr;
				tt_state->outlen = outlen;
				tt_state->dx = dx;
				tt_state->dx_start = dx_start;
				tt_state->dx_instr = dx_instr;
				tt_state->dx_outlen = dx_outlen;
				tt_state->insert_mode = insert_mode;
				tt_state->end_time = end_time;
				tt_state->zb_ptr = zb_ptr;
				tt_state->zb_top = zb_top;
				tt_ptr->mupintr = TRUE;
			} else
			{
				instr = outlen = 0;
				SEND_KEYPAD_LOCAL
				if (!msec_timeout)
					iott_rterm(io_ptr);
			}
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
		} else if (0 < (rdlen = (int)(read(tt_ptr->fildes, &inbyte, 1))))	/* This read is protected */
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
				if (0 == inbyte)
				{
					/* --------------------------------------
					 * This means that the device has hungup
					 * --------------------------------------
					 */
					io_ptr->dollar.zeof = TRUE;
					io_ptr->dollar.x = 0;
					io_ptr->dollar.za = 9;
					io_ptr->dollar.y++;
					tt_ptr->discard_lf = FALSE;
					if (io_ptr->error_handler.len > 0)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IOEOF);
					break;
				} else
					io_ptr->dollar.zeof = FALSE;
			}
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
						iott_readfl_badchar(v, buffer_32_start, outlen,
								    (int)(more_ptr - more_buf), more_buf, more_ptr, buffer_start);
						utf8_badchar((int)(more_ptr - more_buf), more_buf, more_ptr, 0, NULL); /* BADCHAR */
						break;
					}
				} else if (0 < (utf8_more = UTF8_MBFOLLOW(&inbyte)))	/* assignment */
				{
					more_ptr = more_buf;
					if (0 > utf8_more)
					{	/* invalid character */
						io_ptr->dollar.za = 9;
						*more_ptr++ = inbyte;
						iott_readfl_badchar(v, buffer_32_start, outlen,
								1, more_buf, more_ptr, buffer_start);
						utf8_badchar(1, more_buf, more_ptr, 0, NULL);	/* ERR_BADCHAR */
						break;
					} else if (GTM_MB_LEN_MAX < utf8_more)
					{	/* too big to be valid */
						io_ptr->dollar.za = 9;
						*more_ptr++ = inbyte;
						iott_readfl_badchar(v, buffer_32_start, outlen,
								1, more_buf, more_ptr, buffer_start);
						utf8_badchar(1, more_buf, more_ptr, 0, NULL);	/* ERR_BADCHAR */
						break;
					} else
					{
						*more_ptr++ = inbyte;
						continue;	/* get next byte */
					}
				} else
				{	/* single byte */
					more_ptr = more_buf;
					*more_ptr++ = inbyte;
					UTF8_MBTOWC(more_buf, more_ptr, inchar);
					if (WEOF == inchar)
					{	/* invalid char */
						io_ptr->dollar.za = 9;
						iott_readfl_badchar(v, buffer_32_start, outlen,
								1, more_buf, more_ptr, buffer_start);
						utf8_badchar(1, more_buf, more_ptr, 0, NULL);	/* ERR_BADCHAR */
						break;
					}
				}
				if (!tt_ptr->done_1st_read)
				{
					tt_ptr->done_1st_read = TRUE;
					if (BOM_CODEPOINT == inchar)
						continue;
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
                        GETASCII(asc_inchar,inchar);
			if (!edit_mode && (dx >= ioptr_width) && io_ptr->wrap && !(mask & TRM_NOECHO))
			{
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, STRLEN(NATIVE_TTEOL));
				dx = 0;
			}
			if ((' ' > INPUT_CHAR) && (tt_ptr->enbld_outofbands.mask & (1 << INPUT_CHAR)))
			{	/* ctrap supercedes editing so check first */
				instr = outlen = 0;
				io_ptr->dollar.za = 9;
				std_dev_outbndset(INPUT_CHAR);	/* it needs ASCII?	*/
				SEND_KEYPAD_LOCAL
				if (!msec_timeout)
					iott_rterm(io_ptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_CTRAP, 1, ctrap_action_is);
				break;
			}
			if (((0 != (mask & TRM_ESCAPE)) || edit_mode)
			     && ((NATIVE_ESC == inchar) || (START != io_ptr->esc_state)))
			{
				if (zb_ptr >= zb_top UNICODE_ONLY(|| (utf8_active && ASCII_MAX < inchar)))
				{	/* $zb overflow or not ASCII in utf8 mode */
					io_ptr->dollar.za = 2;
					break;
				}
				*zb_ptr++ = (unsigned char)inchar;
				iott_escape(zb_ptr - 1, zb_ptr, io_ptr);
				*(zb_ptr - 1) = (unsigned char)INPUT_CHAR;     /* need to store ASCII value    */
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
				if (!utf8_active || ASCII_MAX >= INPUT_CHAR)
				{	/* may need changes to allow terminator > MAX_ASCII and/or LS and PS if default_mask_term */
					msk_num = (uint4)INPUT_CHAR / NUM_BITS_IN_INT4;
					msk_in = (1 << ((uint4)INPUT_CHAR % NUM_BITS_IN_INT4));
					if (msk_in & mask_term.mask[msk_num])
					{
						*zb_ptr++ = (unsigned char)INPUT_CHAR;
						if (utf8_active && ASCII_CR == INPUT_CHAR)
							tt_ptr->discard_lf = TRUE;
						break;
					}
				} else if (utf8_active && tt_ptr->default_mask_term && (u32_line_term[U32_LT_NL] == INPUT_CHAR ||
					u32_line_term[U32_LT_LS] == INPUT_CHAR || u32_line_term[U32_LT_PS] == INPUT_CHAR))
				{	/* UTF and default terminators and Unicode terminators above ASCII_MAX */
					zb_ptr = UTF8_WCTOMB(INPUT_CHAR, zb_ptr);
					break;
				}
				assert(0 <= instr);
				assert(!edit_mode || 0 <= dx);
				assert(outlen >= instr);
				/* For most of the terminal the 'kbs' string capability is a byte in length. It means that it is
				  Not treated as escape sequence. So explicitly check if the input corresponds to the 'kbs' */
				if ((((int)inchar == tt_ptr->ttio_struct->c_cc[VERASE]) ||
				    (empterm && ('\0' == KEY_BACKSPACE[1]) && (inchar == KEY_BACKSPACE[0])))
					&& !(mask & TRM_PASTHRU))
				{
					if (0 < instr && (edit_mode || 0 < dx))
					{
						dx_prev = compute_dx(BUFF_ADDR(0), instr - 1, ioptr_width, dx_start);
						delchar_width = dx_instr - dx_prev;
						if (edit_mode)
						{
							if (!(mask & TRM_NOECHO))
								move_cursor_left(dx, delchar_width);
							dx = (dx - delchar_width + ioptr_width) % ioptr_width;
						} else
							dx -= delchar_width;
						instr--;
						dx_instr -= delchar_width;
						STORE_OFF(' ', outlen);
						outlen--;
						if (!(mask & TRM_NOECHO) && edit_mode)
						{
							IOTT_COMBINED_CHAR_CHECK;
						}
						MOVE_BUFF(instr, BUFF_ADDR(instr + 1), outlen - instr);
						if (!(mask & TRM_NOECHO))
						{
							if (!edit_mode)
							{
								for (i = 0; i < delchar_width; i++)
								{
									DOWRITERC(tt_ptr->fildes, eraser,
										SIZEOF(eraser), status)
									if (0 != status)
										break;
								}
							} else
							{	/* First write spaces on all the display columns that
								 * the current string occupied. Then overwrite that
								 * with the new string. This way we are guaranteed all
								 * display columns are clean.
								 */
								status = write_str_spaces(dx_outlen - dx_instr, dx, FALSE);
								if (0 == status)
									status = write_str(BUFF_ADDR(instr),
											outlen - instr, dx, FALSE, FALSE);
							}
							if (0 != status)
							{
								term_error_line = __LINE__;
								goto term_error;
							}
						}
						dx_outlen = compute_dx(BUFF_ADDR(0), outlen, ioptr_width, dx_start);
					} else if (empterm && (0 == outlen))
					{
						assert(zb_ptr == io_ptr->dollar.zb);
						*zb_ptr++ = (unsigned char)INPUT_CHAR;
						break;
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

							num_lines_above = (dx_instr + dx_start) /
										ioptr_width;
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
							instr = dx_instr = 0;
							dx = dx_start;
							break;
						}
						case EDIT_EOL:	/* ctrl E  end of line */
						{
							int	num_lines_above;
							int	num_chars_left;

							num_lines_above =
								(dx_instr + dx_start) / ioptr_width -
									(dx_outlen + dx_start) / ioptr_width;
							/* For some reason, a CURSOR_DOWN ("\n") seems to reposition the cursor
							 * at the beginning of the next line rather than maintain the vertical
							 * position. Therefore if we are moving down, we need to calculate
							 * the num_chars_left differently to accommodate this.
							 */
							if (0 <= num_lines_above)
								num_chars_left = dx - (dx_outlen + dx_start) % ioptr_width;
							else
								num_chars_left = - ((dx_outlen + dx_start) % ioptr_width);
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
							dx_instr = dx_outlen;
							dx = (dx_outlen + dx_start) % ioptr_width;
							break;
						}
						case EDIT_LEFT:	/* ctrl B  left one */
						{
							if (instr != 0)
							{
								dx_prev = compute_dx(BUFF_ADDR(0), instr - 1,
												ioptr_width, dx_start);
								inchar_width = dx_instr - dx_prev;
								if (!(mask & TRM_NOECHO))
								{
									if (0 != move_cursor_left(dx, inchar_width))
									{
										term_error_line = __LINE__;
										goto term_error;
									}
								}
								instr--;
								dx = (dx - inchar_width + ioptr_width) % ioptr_width;
								dx_instr -= inchar_width;
							}
							break;
						}
						case EDIT_RIGHT:	/* ctrl F  right one */
						{
							if (instr < outlen)
							{
								dx_next = compute_dx(BUFF_ADDR(0), instr + 1,
												ioptr_width, dx_start);
								inchar_width = dx_next - dx_instr;
								if (!(mask & TRM_NOECHO))
								{
									if (0 != move_cursor_right(dx, inchar_width))
									{
										term_error_line = __LINE__;
										goto term_error;
									}
								}
								instr++;
								dx = (dx + inchar_width) % ioptr_width;
								dx_instr += inchar_width;
							}
							break;
						}
						case EDIT_DEOL:	/* ctrl K  delete to end of line */
						{
							if (!(mask & TRM_NOECHO))
							{
								if (0 != write_str_spaces(dx_outlen - dx_instr, dx, FALSE))
								{
									term_error_line = __LINE__;
									goto term_error;
								}
							}
							SET_BUFF(instr, ' ', outlen - instr);
							outlen = instr;
							dx_outlen = dx_instr;
							break;
						}
						case EDIT_ERASE:	/* ctrl U  delete whole line */
						{
							int	num_lines_above;
							int	num_chars_left;

							num_lines_above = (dx_instr + dx_start) /
											ioptr_width;
							num_chars_left = dx - dx_start;
							SET_BUFF(0, ' ', outlen);
							if (!(mask & TRM_NOECHO))
							{
								status = move_cursor(tt_ptr->fildes,
									num_lines_above, num_chars_left);
								if (0 != status
									|| 0 != write_str_spaces(dx_outlen, dx_start, FALSE))
								{
									term_error_line = __LINE__;
									goto term_error;
								}
							}
							instr = 0;
							outlen = 0;
							dx = dx_start;
							dx_instr = dx_outlen = 0;
							break;
						}
						case EDIT_DELETE:	/* ctrl D delete char */
						{
							if (instr < outlen)
							{
								STORE_OFF(' ', outlen);
								outlen--;
								if (!(mask & TRM_NOECHO))
								{
									IOTT_COMBINED_CHAR_CHECK;
								}
								MOVE_BUFF(instr, BUFF_ADDR(instr + 1), outlen - instr);
								if (!(mask & TRM_NOECHO))
								{	/* First write spaces on all the display columns that
									 * the current string occupied. Then overwrite that
									 * with the new string. This way we are guaranteed all
									 * display columns are clean.
									 */
									if (0 != write_str_spaces(dx_outlen - dx_instr, dx, FALSE))
									{
										term_error_line = __LINE__;
										goto term_error;
									}
									if (0 != write_str(BUFF_ADDR(instr), outlen - instr,
												dx, FALSE, FALSE))
									{
										term_error_line = __LINE__;
										goto term_error;
									}
								}
								dx_outlen = compute_dx(BUFF_ADDR(0), outlen,
												ioptr_width, dx_start);
							}
							break;
						}
						default:
						{
							if (outlen > instr)
							{
								if (insert_mode)
									MOVE_BUFF(instr + 1, BUFF_ADDR(instr), outlen - instr)
								else if (edit_mode && !insert_mode)
								{	/* only needed if edit && !insert_mode && not at end */
									GTM_IO_WCWIDTH(GET_OFF(instr), delchar_width);
									delta_width = inchar_width - delchar_width;
								}
							}
							STORE_OFF(inchar, instr);
							if (!(mask & TRM_NOECHO))
							{
								if (!edit_mode)
								{
									term_error_line = __LINE__;
									status = iott_write_raw(tt_ptr->fildes,
											BUFF_ADDR(instr), 1);
									if (0 <= status)
									    status = 0;
									else
									    status = errno;
								} else if (instr == outlen)
								{
									term_error_line = __LINE__;
									status = write_str(BUFF_ADDR(instr), 1, dx, FALSE, FALSE);
								} else
								{	/* First write spaces on all the display columns that the
									 * current string occupied. Then overwrite that with the
									 * new string. This way we are guaranteed all display
									 * columns are clean. Note that this space overwrite is
									 * needed even in case of insert mode because due to
									 * differing wide-character alignments before and after
									 * the insert, it is possible that a column might be left
									 * empty in the post insert write of the new string even
									 * though it had something displayed before.
									 */
									term_error_line = __LINE__;
									status = write_str_spaces(dx_outlen - dx_instr, dx, FALSE);
									if (0 == status)
									{
										term_error_line = __LINE__;
										status = write_str(BUFF_ADDR(instr),
											outlen - instr + (insert_mode ? 1 : 0),
											dx, FALSE, FALSE);
									}
								}
								if (0 != status)
									goto term_error;
							}
							if (insert_mode || instr == outlen)
								outlen++;
							instr++;
							if (edit_mode)
							{	/* Compute value of dollarx at the new cursor position */
								dx_cur = compute_dx(BUFF_ADDR(0), instr, ioptr_width, dx_start);
								inchar_width = dx_cur - dx_instr;
								if (!(mask & TRM_NOECHO))
								{
									term_error_line = __LINE__;
									status = move_cursor_right(dx, inchar_width);
									if (0 != status)
										goto term_error;
								}
							}
							if (!edit_mode)
							{
								dx += inchar_width;
								dx_instr += inchar_width;
								dx_outlen += inchar_width;
							} else if (insert_mode || instr >= outlen)
							{	/* at end  or insert */
								dx = (dx + inchar_width) % ioptr_width;
								dx_instr = dx_cur;
								dx_outlen = compute_dx(BUFF_ADDR(0), outlen,
											ioptr_width, dx_start);
							} else
							{	/* replaced character */
								dx = (dx + inchar_width) % ioptr_width;
								dx_instr = dx_cur;
								dx_outlen = compute_dx(BUFF_ADDR(0), outlen,
											ioptr_width, dx_start);
							}
							break;
						}
					}
				}
			}
			/* Ensure that the actual display position of the current character matches the computed value */
			assert(!edit_mode || dx_instr == compute_dx(BUFF_ADDR(0), instr, ioptr_width, dx_start));
			assert(!edit_mode || dx_outlen == compute_dx(BUFF_ADDR(0), outlen, ioptr_width, dx_start));
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
                                        	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOPRINCIO);
                                        	stop_image_no_core();
					}
                                }
				if (io_ptr->dollar.zeof)
				{
					io_ptr->dollar.za = 9;
					SEND_KEYPAD_LOCAL
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IOEOF);
				} else
				{
					io_ptr->dollar.zeof = TRUE;
					io_ptr->dollar.za = 0;
					SEND_KEYPAD_LOCAL
					if (0 < io_ptr->error_handler.len)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IOEOF);
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
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IOEOF);
				break;
			}
		} else if (EINTR != errno)	/* rdlen < 0 */
		{
			term_error_line = __LINE__;
			goto term_error;
		}
		if (FINI == io_ptr->esc_state)
		{
			int zb_len = (int)(zb_ptr - io_ptr->dollar.zb);

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
					dx_prev = compute_dx(BUFF_ADDR(0), instr - 1, ioptr_width, dx_start);
					delchar_width = dx_instr - dx_prev;
					if (!(mask & TRM_NOECHO))
					{
						term_error_line = __LINE__;
						status = move_cursor_left(dx, delchar_width);
					}
					dx = (dx - delchar_width + ioptr_width) % ioptr_width;
					instr--;
					dx_instr -= delchar_width;
					STORE_OFF(' ', outlen);
					outlen--;
					if (!(mask & TRM_NOECHO))
					{
						IOTT_COMBINED_CHAR_CHECK;
					}
					MOVE_BUFF(instr, BUFF_ADDR(instr + 1), outlen - instr);
					if (!(mask & TRM_NOECHO))
					{	/* First write spaces on all the display columns that the current string occupied.
						 * Then overwrite that with the new string. This way we are guaranteed all
						 * display columns are clean.
						 */
						if (0 == status)
						{
							term_error_line = __LINE__;
							status = write_str_spaces(dx_outlen - dx_instr, dx, FALSE);
						}
						if (0 == status)
						{
							term_error_line = __LINE__;
							status = write_str(BUFF_ADDR(instr), outlen - instr, dx, FALSE, FALSE);
						}
						if (0 != status)
							goto term_error;
					}
					dx_outlen = compute_dx(BUFF_ADDR(0), outlen, ioptr_width, dx_start);
				} else if (empterm && 0 == outlen)
				{
					assert(instr == 0);
					break;
				}
				escape_edit = TRUE;
			}
			if (up == 0  ||  down == 0)
			{	/* move cursor to start of field */
				if (0 < instr)
				{
					if (0 != move_cursor(tt_ptr->fildes, ((dx_instr + dx_start) / ioptr_width),
						(dx - dx_start)))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
				}
				instr = (int)tt_ptr->recall_buff.len;
				if (length < instr)
					instr = length;	/* restrict to length of read */
				if (0 != instr)
				{	/* need to blank old output first */
					SET_BUFF(instr, ' ', outlen);
					if (0 != write_str_spaces(dx_outlen, dx_start, FALSE))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
					MOVE_BUFF(0, tt_ptr->recall_buff.addr, instr);
					if (0 != write_str(BUFF_ADDR(0), instr, dx_start, TRUE, FALSE))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
				}
				dx_instr = dx_outlen = tt_ptr->recall_width;
				dx = (unsigned)(dx_instr + dx_start) % ioptr_width;
				outlen = instr;
				escape_edit = TRUE;
			} else if (   !(mask & TRM_NOECHO)
				 && !(right == 0  &&  instr == outlen)
				 && !(left == 0   &&  instr == 0))
			{
				if (right == 0)
				{
					dx_next = compute_dx(BUFF_ADDR(0), instr + 1, ioptr_width, dx_start);
					inchar_width = dx_next - dx_instr;
					if (0 != move_cursor_right(dx, inchar_width))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
					instr++;
					dx = (dx + inchar_width) % ioptr_width;
					dx_instr += inchar_width;
				}
				if (left == 0)
				{
					dx_prev = compute_dx(BUFF_ADDR(0), instr - 1, ioptr_width, dx_start);
					inchar_width = dx_instr - dx_prev;
					if (0 != move_cursor_left(dx, inchar_width))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
					instr--;
					dx = (dx - inchar_width + ioptr_width) % ioptr_width;
					dx_instr -= inchar_width;
				}
			}
			if (0 == insert_key)
				insert_mode = !insert_mode;	/* toggle */
			if (0 == right || 0 == left || 0 == insert_key)
				escape_edit = TRUE;
			if (escape_edit || (0 == (TRM_ESCAPE & mask)))
			{	/* reset dollar zb if editing function or not trm_escape */
				memset(io_ptr->dollar.zb, '\0', SIZEOF(io_ptr->dollar.zb));
				io_ptr->esc_state = START;
				zb_ptr = io_ptr->dollar.zb;
				zb_top = zb_ptr + SIZEOF(io_ptr->dollar.zb) - 1;
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
			input_timeval.tv_usec = (gtm_tv_usec_t)cur_time.at_usec;
		}
	} while (outlen < length);
	*zb_ptr++ = 0;
	memcpy(io_ptr->dollar.key, io_ptr->dollar.zb, (zb_ptr - io_ptr->dollar.zb));
	if (!msec_timeout)
	{
		iott_rterm(io_ptr);
		if (0 == outlen && ((io_ptr->dollar.zb + 1) == zb_ptr)) /* No input and no delimiter seen */
			ret = FALSE;
	}
	if (mask & TRM_READSYNC)
	{
		DOWRITERC(tt_ptr->fildes, &dc3, 1, status);
		if (0 != status)
		{
			io_ptr->dollar.za = 9;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
		}
	}
	SEND_KEYPAD_LOCAL	/* to turn keypad off if possible */
	if (outofband && jobinterrupt != outofband)
	{
		v->str.len = 0;
		io_ptr->dollar.za = 9;
		return(FALSE);
	}
#ifdef UNICODE_SUPPORTED
	if (utf8_active)
	{
		outptr = buffer_start;
		outtop = ((unsigned char *)buffer_32_start);
		current_32_ptr = buffer_32_start;
		for (i = 0; i < outlen && outptr < outtop; i++, current_32_ptr++)
			outptr = UTF8_WCTOMB(*current_32_ptr, outptr);
		v->str.len = INTCAST(outptr - buffer_start);
	} else
#endif
		v->str.len = outlen;
	v->str.addr = (char *)buffer_start;
	if (edit_mode)
	{	/* store in recall buffer */
		if ((BUFF_CHAR_SIZE * outlen) > tt_ptr->recall_size)
		{
			if (tt_ptr->recall_buff.addr)
				free(tt_ptr->recall_buff.addr);
			tt_ptr->recall_size = (int)(BUFF_CHAR_SIZE * outlen);
			tt_ptr->recall_buff.addr = malloc(tt_ptr->recall_size);
		}
		tt_ptr->recall_width = dx_outlen;
		tt_ptr->recall_buff.len = outlen;
		memcpy(tt_ptr->recall_buff.addr, BUFF_ADDR(0), BUFF_CHAR_SIZE * outlen);
	}
	if (!(mask & TRM_NOECHO))
	{
		if ((io_ptr->dollar.x += dx_outlen) >= ioptr_width && io_ptr->wrap)
		{
			io_ptr->dollar.y += (io_ptr->dollar.x / ioptr_width);
			if (io_ptr->length)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x %= ioptr_width;
			if (0 == io_ptr->dollar.x)
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, STRLEN(NATIVE_TTEOL));
		}
	}
	return ((short)ret);

term_error:
	save_errno = errno;
	io_ptr->dollar.za = 9;
	tt_ptr->discard_lf = FALSE;
	SEND_KEYPAD_LOCAL	/* to turn keypad off if possible */
	if (!msec_timeout)
		iott_rterm(io_ptr);
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
	return FALSE;
}
