/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gtm_poll.h"
#include "gtm_stdlib.h"

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
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "error.h"
#include "std_dev_outbndset.h"
#include "wake_alarm.h"
#include "min_max.h"
#include "svnames.h"
#include "op.h"
#include "util.h"
#ifdef UTF8_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif
#include "comline.h"

GBLREF	boolean_t		gtm_utf8_mode, hup_on, prin_in_dev_failure, prin_out_dev_failure;
GBLREF	char			*KEY_BACKSPACE, *KEY_DC, *KEY_DOWN, *KEY_INSERT, *KEY_LEFT, *KEYPAD_LOCAL, *KEYPAD_XMIT, *KEY_RIGHT,
					*KEY_UP;
GBLREF	int			AUTO_RIGHT_MARGIN, EAT_NEWLINE_GLITCH;
GBLDEF	int			term_error_line;		/* record for cores */
GBLREF	int4			exi_condition;
GBLREF	io_pair			io_curr_device, io_std_device;
GBLREF	mval			dollar_zstatus;
GBLREF	mv_stent		*mv_chain;
GBLREF	spdesc			stringpool;
GBLREF	stack_frame		*frame_pointer;
GBLREF	unsigned char		*msp, *stackbase, *stacktop, *stackwarn;
GBLREF	volatile boolean_t	dollar_zininterrupt;
GBLREF	volatile int4		outofband;
GBLREF	char			*KEY_HOME, *KEY_END;

LITREF	unsigned char		lower_to_upper_table[];

#ifdef UTF8_SUPPORTED
LITREF	UChar32		u32_line_term[];
#endif

#ifdef __MVS__
#	define SEND_KEYPAD_LOCAL
#else
#	define	SEND_KEYPAD_LOCAL					\
		if (edit_mode && NULL != KEYPAD_LOCAL && (keypad_len = STRLEN(KEYPAD_LOCAL)))	/* embedded assignment */	\
			DOWRITE(tt_ptr->fildes, KEYPAD_LOCAL, keypad_len)
#endif

/* dc1 & dc3 have the same value in ASCII and EBCDIC */
static readonly char		dc1 = 17;
static readonly char		dc3 = 19;
static readonly unsigned char	eraser[3] = { NATIVE_BS, NATIVE_SP, NATIVE_BS };

error_def(ERR_IOEOF);
error_def(ERR_NOPRINCIO);
error_def(ERR_TERMHANGUP);
error_def(ERR_ZINTRECURSEIO);

#define IOTT_MOVE_START_OF_LINE(TT_PTR_FILDES, DX, DX_INSTR, DX_START, IOPTR_WIDTH, MASK, TERM_ERROR_LINE, INSTR)	\
MBSTART {														\
	int	num_lines_above;											\
	int	num_chars_left;												\
															\
	num_lines_above = (dx_instr + dx_start) / ioptr_width;								\
	num_chars_left = dx - dx_start;											\
	if (!(mask & TRM_NOECHO))											\
	{														\
		if (0 != move_cursor(tt_ptr->fildes, num_lines_above, num_chars_left))					\
		{													\
			term_error_line = __LINE__;									\
			goto term_error;										\
		}													\
	}														\
	instr = dx_instr = 0;												\
	dx = dx_start;													\
} MBEND

#define IOTT_MOVE_END_OF_LINE(TT_PTR_FILDES, DX, DX_INSTR, DX_START, DX_OUTLEN, IOPTR_WIDTH, MASK, TERM_ERROR_LINE, INSTR, OUTLEN)	\
MBSTART {																\
	int	num_lines_above;													\
	int	num_chars_left;														\
																	\
	num_lines_above = (dx_instr + dx_start) / ioptr_width - (dx_outlen + dx_start) / ioptr_width;					\
																	\
	/* For some reason, a CURSOR_DOWN ("\n") seems to reposition the cursor								\
	 * at the beginning of the next line rather than maintain the vertical								\
	 * position. Therefore if we are moving down, we need to calculate								\
	 * the num_chars_left differently to accommodate this.										\
	 */																\
	if (0 <= num_lines_above)													\
		num_chars_left = dx - (dx_outlen + dx_start) % ioptr_width;								\
	else																\
		num_chars_left = - ((dx_outlen + dx_start) % ioptr_width);								\
																	\
	if (!(mask & TRM_NOECHO))													\
	{																\
		if (0 != move_cursor(tt_ptr->fildes, num_lines_above, num_chars_left))							\
		{															\
			term_error_line = __LINE__;											\
			goto term_error;												\
		}															\
	}																\
	instr = outlen;															\
	dx_instr = dx_outlen;														\
	dx = (dx_outlen + dx_start) % ioptr_width;											\
} MBEND

#ifdef UTF8_SUPPORTED

/* Maintenance of $ZB on a badchar error and returning partial data (if any) */
void iott_readfl_badchar(mval *vmvalptr, wint_t *dataptr32, int datalen,
			 int delimlen, unsigned char *delimptr, unsigned char *strend, unsigned char *buffer_start)
{
	int		 i, len, tmplen;
	unsigned char   *delimend, *outptr, *outtop;
	wint_t		*curptr32;
	io_desc		*iod;

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
		if (IS_AT_END_OF_STRINGPOOL(buffer_start, 0))
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

int	iott_readfl(mval *v, int4 length, uint8 nsec_timeout)	/* timeout in milliseconds */
{
	ABS_TIME	cur_time, end_time;
	boolean_t	buffer_moved, ch_set, edit_mode, empterm, escape_edit, insert_mode, nonzerotimeout, ret, timed, utf8_active,
				zint_restart;
	d_tt_struct	*tt_ptr;
	int		backspace, delete, down, i, insert_key, ioptr_width, exp_length, keypad_len, left, msk_in, msk_num,
				rdlen, right, save_errno, selstat, status, up, utf8_more;
	int		utf8_seen;
	int		delchar_width;		/* display width of deleted char */
	int		dx, dx_start;		/* local dollar X, starting value */
	int		dx_instr, dx_outlen;	/* wcwidth of string to insert point, whole string */
	int		dx_prev, dx_cur, dx_next;/* wcwidth of string to char BEFORE, AT and AFTER the insert point */
	int		inchar_width;		/* display width of inchar */
	int		instr;			/* insert point in input string */
	int		outlen;			/* total characters in line so far */
	int		home, end;
	int		recall_index;
	boolean_t	no_up_or_down_cursor_yet;
	io_desc		*io_ptr;
	io_terminator	outofbands;
	io_termmask	mask_term;
	mv_stent	*mvc, *mv_zintdev;
	tt_interrupt	*tt_state;
	uint4		mask;
	unsigned char	inbyte, *outptr, *outtop, *zb_ptr, *zb_top;
	unsigned char	more_buf[GTM_MB_LEN_MAX + 1], *more_ptr;	/* to build up multi byte for character */
	unsigned char	*buffer_start;		/* beginning of non UTF8 buffer */
	wint_t		inchar, *buffer_32_start, switch_char;
	int		poll_timeout, save_poll_timeout;
	nfds_t		poll_nfds;
	struct pollfd	poll_fdlist[1];
#ifdef __MVS__
	wint_t		asc_inchar;
#endif
	recall_ctxt_t	*recall;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	io_ptr = io_curr_device.in;
	if (ERR_TERMHANGUP == error_condition)
	{
		TERMHUP_NOPRINCIO_CHECK(FALSE);				/* FALSE for READ */
		io_ptr->dollar.za = ZA_IO_ERR;
		return FALSE;
	}
	ESTABLISH_RET_GTMIO_CH(&io_curr_device, -1, ch_set);
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);
	SETTERM_IF_NEEDED(io_ptr, tt_ptr);
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
		assertpro(ttwhichinvalid != tt_state->who_saved);
		if (dollar_zininterrupt)
		{
			tt_ptr->mupintr = FALSE;
			tt_state->who_saved = ttwhichinvalid;
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTRECURSEIO);
		}
		assert(length == tt_state->length);
		assertpro(ttread == tt_state->who_saved);	/* ZINTRECURSEIO should have caught */
		mv_zintdev = io_find_mvstent(io_ptr, FALSE);
		if (NULL != mv_zintdev && mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid)
		{	/* looks good so use it */
			buffer_start = (unsigned char *)mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr;
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
				utf8_more = tt_state->utf8_more;
				if (utf8_more)
				{
					utf8_seen = tt_state->utf8_seen;
					assert(0 < utf8_seen);
					assert(GTM_MB_LEN_MAX >= (utf8_seen + utf8_more));
					memcpy(more_buf, tt_state->more_buf, utf8_seen);
					more_ptr = more_buf + utf8_seen;
				}
			}
			instr = tt_state->instr;
			outlen = tt_state->outlen;
			dx = tt_state->dx;
			dx_start = tt_state->dx_start;
			dx_instr = tt_state->dx_instr;
			dx_outlen = tt_state->dx_outlen;
			recall_index = tt_state->recall_index;
			no_up_or_down_cursor_yet = tt_state->no_up_or_down_cursor_yet;
			insert_mode = tt_state->insert_mode;
			/* The below two asserts ensure the invocation of "iott_rdone" after a job interrupt has
			 * the exact same "nsec_timeout" as well as "timed" variable context. This is needed to
			 * ensure that the "end_time" usages in the post-interrupt invocation always happen
			 * only if the pre-interrupt invocation had initialized "end_time".
			 * Note: Since "timed" is not yet set, we cannot use it but instead use the variables that it derives from.
			 */
			assert((NO_M_TIMEOUT != nsec_timeout) == tt_state->timed);
			assert(nsec_timeout == tt_state->nsec_timeout);
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
		buffer_start = stringpool.free;
		if (utf8_active)
		{
			buffer_32_start = (wint_t *)ROUND_UP2((INTPTR_T)(stringpool.free + (GTM_MB_LEN_MAX * length)),
					SIZEOF(gtm_int64_t));
		}
		instr = outlen = 0;
		dx_instr = dx_outlen = 0;
		recall_index = tt_ptr->recall_index;
		no_up_or_down_cursor_yet = TRUE;	/* No UP or DOWN cursor keys have been pressed yet. This is useful
							 * when the DOWN cursor key is the first one pressed we know to use
							 * "recall_index" as is instead of going "recall_index++". Gives us
							 * one more history element this way.
							 */
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
				io_ptr->dollar.za = ZA_IO_ERR;
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
					io_ptr->dollar.za = ZA_IO_ERR;
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
	if (NO_M_TIMEOUT == nsec_timeout)
	{
		timed = FALSE;
		poll_timeout = 100 * MILLISECS_IN_SEC;
	} else
	{
		timed = TRUE;
		poll_timeout = DIVIDE_ROUND_UP(nsec_timeout, NANOSECS_IN_MSEC);
		if (!nsec_timeout)
		{
			if (!zint_restart)
				iott_mterm(io_ptr);
		} else
		{
			nonzerotimeout = TRUE;
   			sys_get_curr_time(&cur_time);
			if (!zint_restart)
				add_uint8_to_abs_time(&cur_time, nsec_timeout, &end_time);
		}
	}
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
					if (utf8_more)
					{
						utf8_seen = (int)((UINTPTR_T)more_ptr - (UINTPTR_T)more_buf);
						assert(0 < utf8_seen);
						assert(GTM_MB_LEN_MAX >= (utf8_seen + utf8_more));
						tt_state->utf8_seen = utf8_seen;
						memcpy(tt_state->more_buf, more_buf, utf8_seen);
					}
				}
				if (IS_AT_END_OF_STRINGPOOL(buffer_start, 0))
					stringpool.free += exp_length;	/* reserve space */
				tt_state->instr = instr;
				tt_state->outlen = outlen;
				tt_state->dx = dx;
				tt_state->dx_start = dx_start;
				tt_state->dx_instr = dx_instr;
				tt_state->dx_outlen = dx_outlen;
				tt_state->recall_index = recall_index;
				tt_state->no_up_or_down_cursor_yet = no_up_or_down_cursor_yet;
				tt_state->insert_mode = insert_mode;
				tt_state->end_time = end_time;
				tt_state->zb_ptr = zb_ptr;
				tt_state->zb_top = zb_top;
#				ifdef DEBUG
				/* Store debug-only context used later to assert when restoring this context */
				tt_state->timed = timed;
				tt_state->nsec_timeout = nsec_timeout;
#				endif
				tt_ptr->mupintr = TRUE;
				REVERT_GTMIO_CH(&io_curr_device, ch_set);
			} else
			{
				instr = outlen = 0;
				SEND_KEYPAD_LOCAL;
				if (!nsec_timeout)
					iott_rterm(io_ptr);
			}
			async_action(FALSE);
			break;
		}
		errno = 0;
		/* the checks for EINTR below are valid and should not be converted to EINTR
		 * wrapper macros, since the poll/read is not retried on EINTR.
		 */
		poll_fdlist[0].fd = tt_ptr->fildes;
		poll_fdlist[0].events = POLLIN;
		poll_nfds = 1;
		save_poll_timeout = poll_timeout;	/* take a copy and pass it because poll() below might change it */
		selstat = poll(&poll_fdlist[0], poll_nfds, save_poll_timeout);
		HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
		if (selstat < 0)
		{
			if (EINTR != errno)
			{
				term_error_line = __LINE__;
				goto term_error;
			}
			eintr_handling_check();
		} else if (0 == selstat)
		{
			if (timed)
			{
				ret = FALSE;
				break;
			}
			continue;	/* poll() timeout; keep going */
		} else if (0 < (rdlen = (int)(read(tt_ptr->fildes, &inbyte, 1))))	/* This read is protected */
		{
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			/* set prin_in_dev_failure to FALSE to indicate input device is working now */
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
					io_ptr->dollar.za = ZA_IO_ERR;
					io_ptr->dollar.y++;
					tt_ptr->discard_lf = FALSE;
					if (io_ptr->error_handler.len > 0)
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IOEOF);
					break;
				} else
					io_ptr->dollar.zeof = FALSE;
			}
#ifdef UTF8_SUPPORTED
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
						io_ptr->dollar.za = ZA_IO_ERR;
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
						io_ptr->dollar.za = ZA_IO_ERR;
						*more_ptr++ = inbyte;
						iott_readfl_badchar(v, buffer_32_start, outlen,
								1, more_buf, more_ptr, buffer_start);
						utf8_badchar(1, more_buf, more_ptr, 0, NULL);	/* ERR_BADCHAR */
						break;
					} else if (GTM_MB_LEN_MAX < utf8_more)
					{	/* too big to be valid */
						io_ptr->dollar.za = ZA_IO_ERR;
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
						io_ptr->dollar.za = ZA_IO_ERR;
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
#ifdef UTF8_SUPPORTED
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
				io_ptr->dollar.za = ZA_IO_ERR;
				std_dev_outbndset(INPUT_CHAR);	/* it needs ASCII?	*/
				SEND_KEYPAD_LOCAL;
				if (!nsec_timeout)
					iott_rterm(io_ptr);
				outofband = ctrap;
				async_action(FALSE);
				break;
			}
			if (((0 != (mask & TRM_ESCAPE)) || edit_mode)
			     && ((NATIVE_ESC == inchar) || (START != io_ptr->esc_state)))
			{
				if (zb_ptr >= zb_top UTF8_ONLY(|| (utf8_active && ASCII_MAX < inchar)))
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
				{	/* UTF and default terminators and UTF terminators above ASCII_MAX */
					zb_ptr = UTF8_WCTOMB(INPUT_CHAR, zb_ptr);
					break;
				}
				assert(0 <= instr);
				assert(!edit_mode || 0 <= dx);
				assert(outlen >= instr);
				/* For most of the terminal the 'kbs' string capability is a byte in length. It means that it is
				  Not treated as escape sequence. So explicitly check if the input corresponds to the 'kbs' */
				if ((((int)inchar == tt_ptr->ttio_struct->c_cc[VERASE]) ||
				    (empterm && (NULL != KEY_BACKSPACE) && ('\0' == KEY_BACKSPACE[1])
				     && (inchar == KEY_BACKSPACE[0])))
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
									DOWRITERC(tt_ptr->fildes, eraser, SIZEOF(eraser), status);
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
							IOTT_MOVE_START_OF_LINE(tt_ptr->fildes, dx, dx_instr, dx_start, ioptr_width, mask, term_error_line, instr);
							break;
						}
						case EDIT_EOL:	/* ctrl E  end of line */
						{
							IOTT_MOVE_END_OF_LINE(tt_ptr->fildes, dx, dx_instr, dx_start, dx_outlen, ioptr_width, mask, term_error_line, instr, outlen);
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
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			if (0 < selstat)
			{	/* this should be the only possibility */
				io_ptr->dollar.zeof = TRUE;
				io_ptr->dollar.x = 0;
				io_ptr->dollar.za = 0;
				io_ptr->dollar.y++;
				ISSUE_NOPRINCIO_IF_NEEDED(io_ptr, FALSE, FALSE);	/* FALSE, FALSE: READ, not socket*/
				if (io_ptr->dollar.zeof)
				{
					io_ptr->dollar.za = ZA_IO_ERR;
					SEND_KEYPAD_LOCAL;
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IOEOF);
				} else
				{
					io_ptr->dollar.zeof = TRUE;
					io_ptr->dollar.za = 0;
					SEND_KEYPAD_LOCAL;
					if (0 < io_ptr->error_handler.len)
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IOEOF);
				}
				break;
			}
			if (0 == errno)
			{	/* eof */
				io_ptr->dollar.zeof = TRUE;
				io_ptr->dollar.x = 0;
				io_ptr->dollar.za = 0;
				io_ptr->dollar.y++;
				SEND_KEYPAD_LOCAL;
				if (0 < io_ptr->error_handler.len)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IOEOF);
				break;
			}
		} else if (EINTR != errno)	/* rdlen < 0 */
		{
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			term_error_line = __LINE__;
			goto term_error;
		} else
			eintr_handling_check();
		if (FINI == io_ptr->esc_state)
		{
			int zb_len = (int)(zb_ptr - io_ptr->dollar.zb);

			escape_edit = FALSE;
			/* The arbitrary value -1 signifies inequality in case KEY_* is NULL */
			down = (NULL != KEY_DOWN) ? strncmp((const char *)io_ptr->dollar.zb, KEY_DOWN, zb_len) : -1;
			up = (NULL != KEY_UP) ? strncmp((const char *)io_ptr->dollar.zb, KEY_UP, zb_len) : -1;
			right = (NULL != KEY_RIGHT) ? strncmp((const char *)io_ptr->dollar.zb, KEY_RIGHT, zb_len) : -1;
			left = (NULL != KEY_LEFT) ? strncmp((const char *)io_ptr->dollar.zb, KEY_LEFT, zb_len) : -1;
			backspace = (KEY_BACKSPACE != NULL) ? strncmp((const char *)io_ptr->dollar.zb, KEY_BACKSPACE, zb_len) : -1;
			delete = (KEY_DC != NULL) ? strncmp((const char *)io_ptr->dollar.zb, KEY_DC, zb_len) : -1;
			insert_key = (KEY_INSERT != NULL && '\0' != KEY_INSERT[0])
				?  strncmp((const char *)io_ptr->dollar.zb, KEY_INSERT, zb_len) : -1;
			home = (NULL != KEY_HOME) ? strncmp((const char *)io_ptr->dollar.zb, KEY_HOME, zb_len) : -1;
			end = (NULL != KEY_END) ? strncmp((const char *)io_ptr->dollar.zb, KEY_END, zb_len) : -1;

			if (0 == backspace || 0 == delete)
			{
				if ((0 == backspace) && (instr > 0))
				{	/* Move one character to the left */
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
				}
				if (instr != outlen)
				{
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
				if (NULL != tt_ptr->recall_array)
				{
					if (0 == up)
					{	/* Go back one history item (in circular array) */
						if (0 == recall_index)
							recall_index = MAX_RECALL - 1;
						else
							recall_index--;
					} else if (!no_up_or_down_cursor_yet)
					{	/* Go forward one history item (in circular array) */
						recall_index++;
						if (MAX_RECALL == recall_index)
							recall_index = 0;
					}
					no_up_or_down_cursor_yet = FALSE;
					assert(0 <= recall_index);
					assert(MAX_RECALL > recall_index);
					recall = &tt_ptr->recall_array[recall_index];
					instr = (int)recall->nchars;
					if (length < instr)
						instr = length;	/* restrict to length of read */
					if (outlen)
					{	/* need to blank old output first */
						SET_BUFF(instr, ' ', outlen);
						if (0 != write_str_spaces(dx_outlen, dx_start, FALSE))
						{
							term_error_line = __LINE__;
							goto term_error;
						}
					}
					MOVE_BUFF(0, recall->buff, instr);
					if (0 != write_str(BUFF_ADDR(0), instr, dx_start, TRUE, FALSE))
					{
						term_error_line = __LINE__;
						goto term_error;
					}
					dx_instr = dx_outlen = recall->width;
				} else
				{
					instr = 0;
					dx_instr = dx_outlen = 0;
				}
				dx = (unsigned)(dx_instr + dx_start) % ioptr_width;
				outlen = instr;
				escape_edit = TRUE;
			} else if (!(mask & TRM_NOECHO))
			{
				// When the cursor is at the beginning of the line, we can move it 1 position to the right or to the end of the line.
				// I have grouped these possibilities in pair. This is more understandable and logical. Sergey Kamenev.
				if (instr != outlen)
				{
					if (0 == right)
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
					} else if (0 == end) /* End - end of line */
						IOTT_MOVE_END_OF_LINE(tt_ptr->fildes, dx, dx_instr, dx_start, dx_outlen, ioptr_width, mask, term_error_line, instr, outlen);
				}
				// When the cursor is at the end of the line, we can move it 1 position to the left or to the beginning of the line.
				if (0 != instr)
				{
					if (0 == left)
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
					} else if (0 == home) /* Home - start of line */
					    IOTT_MOVE_START_OF_LINE(tt_ptr->fildes, dx, dx_instr, dx_start, ioptr_width, mask, term_error_line, instr);
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
			if (0 > cur_time.tv_sec)
			{
				ret = FALSE;
				break;
			}
			poll_timeout = (long)((cur_time.tv_sec * MILLISECS_IN_SEC) +
				DIVIDE_ROUND_UP((gtm_tv_usec_t)cur_time.tv_nsec, NANOSECS_IN_MSEC));
		}
	} while (outlen < length);
	*zb_ptr++ = 0;
	memcpy(io_ptr->dollar.key, io_ptr->dollar.zb, (zb_ptr - io_ptr->dollar.zb));
	if (!nsec_timeout)
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
			io_ptr->dollar.za = ZA_IO_ERR;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
		}
	}
	SEND_KEYPAD_LOCAL;	/* to turn keypad off if possible */
	if (outofband && (jobinterrupt != outofband))
	{
		v->str.len = 0;
		io_ptr->dollar.za = ZA_IO_ERR;
		REVERT_GTMIO_CH(&io_curr_device, ch_set);
		RESETTERM_IF_NEEDED(io_ptr, EXPECT_SETTERM_DONE_TRUE);
		return(FALSE);
	}
#	ifdef UTF8_SUPPORTED
	if (utf8_active)
	{
		wint_t *current_32_ptr;

		outptr = buffer_start;
		outtop = ((unsigned char *)buffer_32_start);
		current_32_ptr = buffer_32_start;
		for (i = 0; i < outlen && outptr < outtop; i++, current_32_ptr++)
			outptr = UTF8_WCTOMB(*current_32_ptr, outptr);
		v->str.len = INTCAST(outptr - buffer_start);
	} else
#	endif
		v->str.len = outlen;
	v->str.addr = (char *)buffer_start;
	if (edit_mode)	/* store in recall buffer */
		iott_recall_array_add(tt_ptr, outlen, dx_outlen, BUFF_CHAR_SIZE * outlen, BUFF_ADDR(0));
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
	REVERT_GTMIO_CH(&io_curr_device, ch_set);
	RESETTERM_IF_NEEDED(io_ptr, EXPECT_SETTERM_DONE_TRUE);
	return ((short)ret);

term_error:
	save_errno = errno;
	io_ptr->dollar.za = ZA_IO_ERR;
	tt_ptr->discard_lf = FALSE;
	SEND_KEYPAD_LOCAL;	/* to turn keypad off if possible */
	if (!nsec_timeout)
		iott_rterm(io_ptr);
	RESETTERM_IF_NEEDED(io_ptr, EXPECT_SETTERM_DONE_TRUE);
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
	return FALSE;
}
