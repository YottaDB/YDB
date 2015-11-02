/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* WARNING: this module contains a mixture of ASCII and EBCDIC on S390*/
#include "mdef.h"

#include "gtm_string.h"
#include <errno.h>
#include <wctype.h>
#include <wchar.h>
#include <signal.h>
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "iotcp_select.h"
#include "io.h"
#include "trmdef.h"
#include "iottdef.h"
#include "iottdefsp.h"
#include "iott_edit.h"
#include "stringpool.h"
#include "comline.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "cli.h"
#include "outofband.h"
#include "dm_read.h"
#include "gtm_tputs.h"
#include "op.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF bool		prin_in_dev_failure;
GBLREF boolean_t	dollar_zininterrupt, gtm_utf8_mode;
GBLREF char		*CLR_EOL, *CURSOR_DOWN, *CURSOR_LEFT, *CURSOR_RIGHT, *CURSOR_UP;
GBLREF char		*KEY_BACKSPACE, *KEY_DC, *KEY_DOWN, *KEY_INSERT, *KEY_LEFT, *KEY_RIGHT, *KEY_UP;
GBLREF char		*KEYPAD_LOCAL, *KEYPAD_XMIT;
GBLREF int		AUTO_RIGHT_MARGIN, EAT_NEWLINE_GLITCH;
GBLDEF int		comline_index, recall_num;
GBLREF io_pair 		io_curr_device, io_std_device;
GBLREF io_desc		*active_device;
GBLREF mstr		*comline_base;
GBLREF mv_stent		*mv_chain;
GBLREF stack_frame	*frame_pointer;
GBLREF spdesc 		stringpool;
GBLREF unsigned char	*msp, *stackbase, *stacktop, *stackwarn;
GBLREF volatile int4	outofband;

LITREF unsigned char	lower_to_upper_table[];
#ifdef UNICODE_SUPPORTED
LITREF	UChar32		u32_line_term[];
#endif

#define	MAX_ERR_MSG_LEN		40
#define	REC			"rec"
#define	RECALL			"recall"

enum	RECALL_ERR_CODE
{
	NO_ERROR,
	ERR_OUT_OF_BOUNDS,
	ERR_NO_MATCH_STR
};

static unsigned char	cr = '\r';
static	unsigned char	recall_error_msg[][MAX_ERR_MSG_LEN] =
{
	"",
	"Recall Error : Number exceeds limit",
	"Recall Error : No matching string"
};

error_def(ERR_IOEOF);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);
error_def(ERR_ZINTDIRECT);
error_def(ERR_ZINTRECURSEIO);

#define	WRITE_GTM_PROMPT													\
if (0 < (TREF(gtmprompt)).len)													\
{																\
	write_str((TREF(gtmprompt)).addr, (TREF(gtmprompt)).len, 0, TRUE, TRUE);						\
	if (utf8_active)													\
	{	/* cannot use "gtm_wcswidth" function as we also want to compute "dx_start" in the loop */			\
		dx_start = 0;													\
		for (ptr = (unsigned char *)(TREF(gtmprompt)).addr, ptrtop = ptr + (TREF(gtmprompt)).len; 			\
			ptr < ptrtop; ptr = ptrnext)										\
		{														\
			ptrnext = UTF8_MBTOWC(ptr, ptrtop, codepoint);								\
			if (WEOF == codepoint)											\
				UTF8_BADCHAR(0, ptr, ptrtop, 0, NULL);								\
			GTM_IO_WCWIDTH(codepoint, inchar_width);								\
			assert(0 <= inchar_width);										\
			assert(inchar_width <= ioptr_width);/* width cannot be less than a wide character's display width */	\
			if (((dx_start % ioptr_width) + inchar_width) > ioptr_width)						\
				dx_start = ROUND_UP(dx_start, ioptr_width);/* add $X padding for wide char in last column */	\
			dx_start += inchar_width;										\
		}														\
		assert(0 <= dx_start);												\
	} else															\
		dx_start = (TREF(gtmprompt)).len;										\
	dx = dx_start % ioptr_width;												\
} else																\
	dx = dx_start = 0;

#define	MOVE_CURSOR_LEFT_ONE_CHAR(dx, instr, dx_instr, dx_start, ioptr_width)		\
{											\
	dx_prev = compute_dx(BUFF_ADDR(0), instr - 1, ioptr_width, dx_start);		\
	delchar_width = dx_instr - dx_prev;						\
	move_cursor_left(dx, delchar_width);						\
	dx = (dx - delchar_width + ioptr_width) % ioptr_width;				\
	instr--;									\
	dx_instr -= delchar_width;							\
}

#define	DEL_ONE_CHAR_AT_CURSOR(outlen, dx_outlen, dx, dx_instr, dx_start, ioptr_width)			\
{													\
	assert(instr <= outlen);									\
	if (instr != outlen)										\
	{	/* First write spaces on all the display columns that the current string occupied.	\
		 * Then overwrite that with the new string. This way we are guaranteed all		\
		 * display columns are clean.								\
		 */											\
		STORE_OFF(' ', outlen);									\
		outlen--;										\
		IOTT_COMBINED_CHAR_CHECK;								\
		MOVE_BUFF(instr, BUFF_ADDR(instr + 1), outlen - instr);					\
		write_str_spaces(dx_outlen - dx_instr, dx, FALSE);					\
		write_str(BUFF_ADDR(instr), outlen - instr, dx, FALSE, FALSE);				\
		dx_outlen = compute_dx(BUFF_ADDR(0), outlen, ioptr_width, dx_start);			\
	}												\
}

void	dm_read (mval *v)
{
	boolean_t	buffer_moved, insert_mode, terminator_seen, utf8_active, zint_restart;
#	ifdef UNICODE_SUPPORTED
	boolean_t	matched;
	char		*recptr = RECALL;
#	endif
	char		*argv[3];
	char		temp_str[MAX_RECALL_NUMBER_LENGTH + 1];
	const char	delimiter_string[] = " \t";
	d_tt_struct 	*tt_ptr;
	enum RECALL_ERR_CODE	err_recall = NO_ERROR;
	fd_set		input_fd;
	int		backspace, cl, cur_cl, cur_value, delete, down, hist, histidx, index, insert_key, keypad_len, left;
	int		delchar_width;		/* display width of deleted char */
	int		delta_width;		/* display width change for replaced char */
	int		dx, dx_start;		/* local dollar X, starting value */
	int		dx_instr, dx_outlen;	/* wcwidth of string to insert point, whole string */
	int		dx_prev, dx_cur, dx_next;/* wcwidth of string to char BEFORE, AT and AFTER the insert point */
	int		inchar_width;		/* display width of inchar */
	int		instr;			/* insert point in input string */
	int		ioptr_width;		/* display width of the IO device */
	int		outlen;			/* total characters in line so far */
	int		match_length, msk_in, msk_num, num_chars_left, num_lines_above, right, selstat, status, up, utf8_more;
	io_desc 	*io_ptr;
	io_termmask	mask_term;
	mv_stent	*mvc, *mv_zintdev;
	struct timeval	input_timeval;
	tt_interrupt	*tt_state;
	uint4		mask;
	unsigned int	exp_length, len, length;
	unsigned char	*buffer_start;		/* beginning of non UTF8 buffer */
	unsigned char	*current_ptr;		/* insert next character into buffer here */
	unsigned char	escape_sequence[ESC_LEN];
	unsigned char	inbyte, *outptr, *outtop, *ptr, *ptrnext, *ptrtop;
	unsigned char	more_buf[GTM_MB_LEN_MAX + 1], *more_ptr;	/* to build up multi byte for character */
	unsigned short	escape_length = 0;
	wint_t		*buffer_32_start, codepoint, *current_32_ptr, inchar, *ptr32;
#	ifdef __MVS__
	wint_t		asc_inchar;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	if ( NULL == comline_base)
	{
		comline_base = (mstr *)malloc(MAX_RECALL * SIZEOF(mstr));
		memset(comline_base, 0, (MAX_RECALL * SIZEOF(mstr)));
	}
	active_device = io_curr_device.in;
	io_ptr = io_curr_device.in;
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);
	assert (io_ptr->state == dev_open);
	if (tt == io_curr_device.out->type)
		iott_flush(io_curr_device.out);
	insert_mode = !(TT_NOINSERT & tt_ptr->ext_cap);
	utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE;
	length = tt_ptr->in_buf_sz + ESC_LEN;	/* for possible escape sequence */
	exp_length = utf8_active ? (uint4)((SIZEOF(wint_t) * length) + (GTM_MB_LEN_MAX * length) + SIZEOF(gtm_int64_t)) : length;
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
			rts_error(VARLSTCNT(1) ERR_ZINTDIRECT);
		}
		assert(length == tt_state->length);
		if (dmread != tt_state->who_saved)
			GTMASSERT;	/* ZINTRECURSEIO should have caught */
		mv_zintdev = io_find_mvstent(io_ptr, FALSE);
		if (mv_zintdev && mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid)
		{
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
			index = tt_state->index;
			insert_mode = tt_state->insert_mode;
			cl = tt_state->cl;
			escape_length = tt_state->escape_length;
			memcpy(escape_sequence, tt_state->escape_sequence, ESC_LEN);
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
		io_ptr->esc_state = START;
		io_ptr->dollar.za = 0;
		io_ptr->dollar.zeof = FALSE;
		dx_start = (int)io_ptr->dollar.x;
		index = 0;
		cl = clmod(comline_index - index);
	}
	mask = tt_ptr->term_ctrl;
	mask_term = tt_ptr->mask_term;
	mask_term.mask[ESC / NUM_BITS_IN_INT4] &= ~(1 << ESC);
	ioptr_width = io_ptr->width;
	if (!zint_restart)
	{
		DOWRITE_A(tt_ptr->fildes, &cr, 1);
		WRITE_GTM_PROMPT;
	}
	/* to turn keypad on if possible */
#	ifndef __MVS__
	if (!zint_restart && (NULL != KEYPAD_XMIT) && (keypad_len = STRLEN(KEYPAD_XMIT)))	/* NOTE assignment */
		DOWRITE(tt_ptr->fildes, KEYPAD_XMIT, keypad_len);
#	endif
	while (outlen < length)
	{
		if (outofband)
		{
			if (jobinterrupt == outofband)
			{	/* save state if jobinterrupt */
				tt_state = &tt_ptr->tt_state_save;
				tt_state->who_saved = dmread;
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
				if (buffer_start == stringpool.free)	/* BYPASSOK */
					stringpool.free += exp_length;	/* reserve space */
				tt_state->instr = instr;
				tt_state->outlen = outlen;
				tt_state->dx = dx;
				tt_state->dx_start = dx_start;
				tt_state->dx_instr = dx_instr;
				tt_state->dx_outlen = dx_outlen;
				tt_state->index = index;
				tt_state->insert_mode = insert_mode;
				tt_state->cl = cl;
				tt_state->escape_length = escape_length;
				memcpy(tt_state->escape_sequence, escape_sequence, ESC_LEN);
				tt_ptr->mupintr = TRUE;
			} else
				instr = 0;
			outofband_action(FALSE);
			break;
		}
		FD_ZERO(&input_fd);
		FD_SET(tt_ptr->fildes, &input_fd);
		assert(0 != FD_ISSET(tt_ptr->fildes, &input_fd));
		/* Arbitrarily-chosen timeout value to prevent consumption of resources in tight loop when no input is available. */
		input_timeval.tv_sec = 100;
		input_timeval.tv_usec = 0;
		/* N.B. On some Unix systems, the documentation for select() is ambiguous with respect to the first argument.
		 * It specifies the number of contiguously-numbered file descriptors to be tested, starting with descriptor zero.
		 * Thus, it should be equal to the highest-numbered file descriptor to test plus one.
		 * (See _UNIX_Network_Programming_ by W. Richard Stevens, Section 6.13, pp. 328-331)
		 */
		selstat = select(tt_ptr->fildes + 1, (void *)&input_fd, (void *)NULL, (void *)NULL, &input_timeval);
		if (0 > selstat)
			if (EINTR != errno)
				rts_error(VARLSTCNT(1) errno);
			else
				continue;
		else if (0 == selstat)
			continue;	/* timeout but still not ready for reading, try again */
		/* selstat > 0; try reading something */
		else if (0 > (status = (int)read(tt_ptr->fildes, &inbyte, 1)))
		{	/* Error return from read(). */
			if (EINTR != errno)
			{	/* If error was EINTR, go to the top of the loop to check for outofband. */
				tt_ptr->discard_lf = FALSE;
				io_ptr->dollar.za = 9;
				rts_error(VARLSTCNT(1) errno);
			} else
				continue;
		} else if (0 == status)
		{	/* select() says there's something to read, but read() found zero characters; assume connection dropped. */
			if (io_curr_device.in == io_std_device.in)
			{
				if (!prin_in_dev_failure)
					prin_in_dev_failure = TRUE;
				else
					exit(errno);
			}
			tt_ptr->discard_lf = FALSE;
			rts_error(VARLSTCNT(1) ERR_IOEOF);
		}
		else if (0 < status)
		{	/* select() says it's ready to read and read() found some data */
			assert(0 != FD_ISSET(tt_ptr->fildes, &input_fd));
			/* set prin_in_dev_failure to FALSE in case it was set to TRUE in the previous read which may have failed */
			prin_in_dev_failure = FALSE;
#			ifdef UNICODE_SUPPORTED
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
						utf8_badchar((int)(more_ptr - more_buf), more_buf, more_ptr, 0, NULL);
						break;
					}
				} else if (0 < (utf8_more = UTF8_MBFOLLOW(&inbyte)))	/* NOTE assignment */
				{
					if (0 > utf8_more)
					{	/* invalid character */
						io_ptr->dollar.za = 9;
						more_buf[0] = inbyte;
						utf8_badchar(1, more_buf, &more_buf[1], 0, NULL);	/* ERR_BADCHAR */
						break;
					} else if (GTM_MB_LEN_MAX < utf8_more)
					{	/* too big to be valid */
						io_ptr->dollar.za = 9;
						more_buf[0] = inbyte;
						utf8_badchar(1, more_buf, &more_buf[1], 0, NULL);	/* ERR_BADCHAR */
						break;
					} else
					{
						more_ptr = more_buf;
						*more_ptr++ = inbyte;
						continue;	/* get next byte */
					}
				} else
				{	/* single byte */
					more_buf[0] = inbyte;
					more_buf[1] = 0;
					UTF8_MBTOWC(more_buf, &more_buf[1], inchar);
					if (WEOF == inchar)
					{	/* invalid char */
						io_ptr->dollar.za = 9;
						utf8_badchar(1, more_buf, &more_buf[1], 0, NULL);	/* ERR_BADCHAR */
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
#			endif
				if (mask & TRM_CONVERT)
					NATIVE_CVT2UPPER(inbyte, inbyte);
				inchar = inbyte;
				inchar_width = 1;
#			ifdef UNICODE_SUPPORTED
			}
#			endif
			GETASCII(asc_inchar,inchar);
			if ((dx >= ioptr_width) && io_ptr->wrap && !(mask & TRM_NOECHO))
			{
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));	/* BYPASSOK */
				dx = 0;
			}
			terminator_seen = FALSE;
			if (!utf8_active || (ASCII_MAX >= INPUT_CHAR))
			{
				msk_num = (uint4)INPUT_CHAR / NUM_BITS_IN_INT4;
				msk_in = (1 << ((uint4)INPUT_CHAR % NUM_BITS_IN_INT4));
				if (msk_in & mask_term.mask[msk_num])
					terminator_seen = TRUE;
			} else if (utf8_active && tt_ptr->default_mask_term && ((INPUT_CHAR == u32_line_term[U32_LT_NL])
				 || (INPUT_CHAR == u32_line_term[U32_LT_LS]) || (INPUT_CHAR == u32_line_term[U32_LT_PS])))
			{	/* UTF and default terminators and Unicode terminators above ASCII_MAX */
				terminator_seen = TRUE;
			}
			if (terminator_seen)
			{	/* carriage return or other line terminator has been typed */
				STORE_OFF(0, outlen);
				if (utf8_active && (ASCII_CR == INPUT_CHAR))
					tt_ptr->discard_lf = TRUE;
					/* exceeding the maximum length exits the while loop, so it must fit here . */
#				ifdef UNICODE_SUPPORTED
				if (utf8_active)
				{	/* recall buffer kept as UTF-8 */
					matched = TRUE;
					for (match_length = 0; (SIZEOF(RECALL) - 1) > match_length && outlen > match_length;
							match_length++)
					{
						if (ASCII_MAX < GET_OFF(match_length)
								|| recptr[match_length] != (char)GET_OFF(match_length))
						{
							matched = FALSE;
							break;
						}
					}
					if (!matched && (outlen > match_length)
						&& (((SIZEOF(REC) - 1) == match_length) || (SIZEOF(RECALL) == match_length))
						&& ((' ' == GET_OFF(match_length)) || ('\t' == GET_OFF(match_length))))
							matched = TRUE;		/* REC or RECALL then space or tab */
					else if (matched)
						if (((SIZEOF(RECALL) - 1) != match_length) && ((SIZEOF(REC) - 1) != match_length))
							matched = FALSE;	/* wrong size */
						else if ((outlen > match_length)
							&& (' ' != GET_OFF(match_length) && ('\t' != GET_OFF(match_length))))
								matched = FALSE;	/* or RECALL then not eol, space, or tab */
					if (!matched)
						break;		/* not RECALL so end of line */
					match_length++;		/* get past space or tab */
					if (outlen <= match_length)
						argv[1] = NULL;		/* nothing after RECALL */
					else
						argv[1] = (char *)buffer_start;
					for (outptr = buffer_start ; outlen > match_length; match_length++)
					{
						inchar = GET_OFF(match_length);
						outptr = UTF8_WCTOMB(inchar, outptr);
						if ((ASCII_MAX > GET_OFF(match_length))
							&& ((' ' == GET_OFF(match_length)) || ('\t' == GET_OFF(match_length))))
								break;
					}
					*outptr = '\0';
				} else
				{
#				endif
					match_length = (uint4)strcspn((const char *)buffer_start, delimiter_string);
					/* only "rec" and "recall" should be accepted */
					if (((strlen(REC) == match_length) || (strlen(RECALL) == match_length))
						&& (0 == strncmp((const char *)buffer_start, RECALL, match_length)))
					{
						strtok((char *)buffer_start, delimiter_string);
						argv[1] = strtok(NULL, "");
					} else
						break;		/* not RECALL so end of line */
#				ifdef UNICODE_SUPPORTED
				}
#				endif
				index = 0;
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));	/* BYPASSOK */
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));	/* BYPASSOK */
				if (argv[1] == NULL)
				{	/* print out all the history strings */
					for (hist = recall_num; hist > 0; hist--)
					{
						cur_value = recall_num + 1 - hist;
						temp_str[MAX_RECALL_NUMBER_LENGTH] = ' ';
						for (histidx = MAX_RECALL_NUMBER_LENGTH - 1; histidx >= 0; histidx--)
						{
							temp_str[histidx] = '0' + cur_value % 10;
							cur_value = cur_value / 10;
							if (('0' == temp_str[histidx]) && (0 == cur_value))
								temp_str[histidx] = ' ';
						}
						cur_cl = clmod(comline_index - hist);
						DOWRITE_A(tt_ptr->fildes, temp_str, SIZEOF(temp_str));
						write_str((unsigned char *)comline_base[cur_cl].addr,
							comline_base[cur_cl].len, SIZEOF(temp_str), TRUE, TRUE);
						DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));	/* BYPASSOK */
					}
					DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));	/* BYPASSOK */
					WRITE_GTM_PROMPT;
					instr = dx_instr = outlen = dx_outlen = 0;
				} else
				{
					histidx = -1;
					if (!cli_is_dcm(argv[1]))
					{	/* Not a positive decimal number */
						len = STRLEN(argv[1]);
						for (hist = 1; hist <= recall_num; hist++)
						{
							if (0 == strncmp(comline_base[clmod(comline_index - hist)].addr, argv[1]
								, len))
							{
								histidx = clmod(comline_index - hist);
									break;
							}
						}
						if (-1 == histidx)
							err_recall = ERR_NO_MATCH_STR;	/* no matching string found */
					} else
					{
						if (ATOI(argv[1]) > recall_num)
							err_recall = ERR_OUT_OF_BOUNDS;	/* out of bounds error */
						else
							histidx = clmod(comline_index + ATOI(argv[1]) - recall_num - 1);
					}
					if (-1 != histidx)
					{
						WRITE_GTM_PROMPT;
						write_str((unsigned char *)comline_base[histidx].addr,
							comline_base[histidx].len, dx_start, TRUE, TRUE);
#						ifdef UNICODE_SUPPORTED
						if (utf8_active)
						{	/* note out* variables are used for input here */
							len = comline_base[histidx].len;
							instr = dx_instr = 0;
							outtop = (unsigned char *)comline_base[histidx].addr + len;
							for (outptr = (unsigned char *)comline_base[histidx].addr;
								outptr < outtop; )
							{
								outptr = UTF8_MBTOWC(outptr, outtop, inchar);
								if (WEOF != inchar)
								{
									STORE_OFF(inchar, instr);
									instr++;
									GTM_IO_WCWIDTH(inchar, inchar_width);
									dx_instr += inchar_width;
								}
							}
							outlen = instr;
							dx_outlen = dx_instr;
						} else
						{	/* using memcpy since areas definitely don't overlap. */
#						endif
							memcpy(buffer_start, comline_base[histidx].addr,
								comline_base[histidx].len);
							instr = outlen = comline_base[histidx].len;
							dx_instr = dx_outlen = instr;
#						ifdef UNICODE_SUPPORTED
						}
#						endif
						dx = (dx_start + dx_outlen) % ioptr_width;
					}
				}
				if (NO_ERROR != err_recall)
				{
					DOWRITE_A(tt_ptr->fildes, recall_error_msg[err_recall],
						strlen((char *)recall_error_msg[err_recall]));
					DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));	/* BYPASSOK */
					DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));	/* BYPASSOK */
					WRITE_GTM_PROMPT;
					instr = dx_instr = outlen = dx_outlen = 0;
				}
				continue;	/* to allow more input */
			}
			if ((((int)inchar == tt_ptr->ttio_struct->c_cc[VERASE])
				|| ((('\0' == KEY_BACKSPACE[1]) && (inchar == KEY_BACKSPACE[0])))) && !(mask & TRM_PASTHRU))
			{
				if (0 < instr)
				{
					MOVE_CURSOR_LEFT_ONE_CHAR(dx, instr, dx_instr, dx_start, ioptr_width);
					DEL_ONE_CHAR_AT_CURSOR(outlen, dx_outlen, dx, dx_instr, dx_start, ioptr_width);
				}
			} else if (NATIVE_ESC == inchar)
			{
				escape_sequence[escape_length++] = (unsigned char)inchar;
				io_ptr->esc_state = START;
				iott_escape(&escape_sequence[escape_length - 1], &escape_sequence[escape_length], io_ptr);
			} else if (0 != escape_length)
			{
				if (utf8_active && (ASCII_MAX < inchar))
					continue;		/* skip invalid char in escape sequence */
				escape_sequence[escape_length++] = (unsigned char)inchar;
				iott_escape(&escape_sequence[escape_length - 1], &escape_sequence[escape_length], io_ptr);
			} else
			{
				switch (inchar)
				{
					case EDIT_SOL:
					{	/* ctrl A - start of line */
						num_lines_above = (dx_instr + dx_start) / ioptr_width;
						num_chars_left = dx - dx_start;
						move_cursor(tt_ptr->fildes, num_lines_above, num_chars_left);
						instr = dx_instr = 0;
						dx = dx_start;
						break;
					}
					case EDIT_EOL:
					{	/* ctrl E- end of line */
						num_lines_above = (dx_instr + dx_start) / ioptr_width -
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
						move_cursor(tt_ptr->fildes, num_lines_above, num_chars_left);
						instr = outlen;
						dx_instr = dx_outlen;
						dx = (dx_outlen + dx_start) % ioptr_width;
						break;
					}
					case EDIT_LEFT:
					{	/* ctrl B - left one */
						if (0 != instr)
						{
							dx_prev = compute_dx(BUFF_ADDR(0), instr - 1, ioptr_width, dx_start);
							inchar_width = dx_instr - dx_prev;
							move_cursor_left(dx, inchar_width);
							instr--;
							dx = (dx - inchar_width + ioptr_width) % ioptr_width;
							dx_instr -= inchar_width;
						}
						break;
					}
					case EDIT_RIGHT:
					{	/* ctrl F - right one */
						if (instr != outlen)
						{
							dx_next = compute_dx(BUFF_ADDR(0), instr + 1, ioptr_width, dx_start);
							inchar_width = dx_next - dx_instr;
							move_cursor_right(dx, inchar_width);
							instr++;
							dx = (dx + inchar_width) % ioptr_width;
							dx_instr += inchar_width;
						}
						break;
					}
					case EDIT_DEOL:
					{	/* ctrl K - delete to end of line */
						write_str_spaces(dx_outlen - dx_instr, dx, FALSE);
						SET_BUFF(instr, ' ', outlen - instr);
						outlen = instr;
						dx_outlen = dx_instr;
						break;
					}
					case EDIT_ERASE:
					{	/* ctrl U - delete whole line */
						num_lines_above = (dx_instr + dx_start) / ioptr_width;
						num_chars_left = dx - dx_start;
						move_cursor(tt_ptr->fildes, num_lines_above, num_chars_left);
						SET_BUFF(0, ' ', outlen);
						write_str_spaces(dx_outlen, dx_start, FALSE);
						instr = 0;
						outlen = 0;
						dx = dx_start;
						dx_instr = dx_outlen = 0;
						break;
					}
					case EDIT_DELETE:
					{	/* ctrl D - delete char */
						if (0 == outlen)
						{	/* line is empty new line and exit - Thanks to Sam Habiel */
							op_wteol(1);
							op_halt();
						}
						DEL_ONE_CHAR_AT_CURSOR(outlen, dx_outlen, dx, dx_instr, dx_start, ioptr_width);
						break;
					}
					default:
					{
						if (instr == outlen)
						{	/* at end of input */
							STORE_OFF(inchar, instr);
							write_str(BUFF_ADDR(instr), 1, dx, FALSE, FALSE);
						} else
						{
							if (insert_mode)
								MOVE_BUFF(instr + 1, BUFF_ADDR(instr), outlen - instr)
							else
							{
								GTM_IO_WCWIDTH(GET_OFF(instr), delchar_width);
								delta_width = inchar_width - delchar_width;
							}
							STORE_OFF(inchar, instr);
							/* First write spaces on all the display columns that the current string
							 * occupied. Then overwrite that with the new string. This way we are
							 * guaranteed all display columns are clean. Note that this space
							 * overwrite is needed even in case of insert mode because due to
							 * differing wide-character alignments before and after the insert, it
							 * is possible that a column might be left empty in the post insert
							 * write of the new string even though it had something displayed before.
							 */
							write_str_spaces(dx_outlen - dx_instr, dx, FALSE);
							write_str(BUFF_ADDR(instr), outlen - instr + (insert_mode ? 1 : 0),
								dx, FALSE, FALSE);
						}
						if (insert_mode || (instr == outlen))
							outlen++;
						instr++;
						/* Compute value of dollarx at the new cursor position */
						dx_cur = compute_dx(BUFF_ADDR(0), instr, ioptr_width, dx_start);
						inchar_width = dx_cur - dx_instr;
						move_cursor_right(dx, inchar_width);
						dx = (dx + inchar_width) % ioptr_width;
						dx_instr = dx_cur;
						dx_outlen = compute_dx(BUFF_ADDR(0), outlen, ioptr_width, dx_start);
						break;
					}
				}
				/* Ensure that the actual display position of the current character matches the computed value */
				assert(dx_instr == compute_dx(BUFF_ADDR(0), instr, ioptr_width, dx_start));
				assert(dx_outlen == compute_dx(BUFF_ADDR(0), outlen, ioptr_width, dx_start));
			}
		}
		if ((0 != escape_length) && (FINI <= io_ptr->esc_state))
		{
			down = strncmp((const char *)escape_sequence, KEY_DOWN, escape_length);
			up = strncmp((const char *)escape_sequence, KEY_UP, escape_length);
			right = strncmp((const char *)escape_sequence, KEY_RIGHT, escape_length);
			left = strncmp((const char *)escape_sequence, KEY_LEFT, escape_length);
			backspace = delete = insert_key = -1;
			if (NULL != KEY_BACKSPACE)
				backspace = strncmp((const char *)escape_sequence, KEY_BACKSPACE, escape_length);
			if (NULL != KEY_DC)
				delete = strncmp((const char *)escape_sequence, KEY_DC, escape_length);
			if ((NULL != KEY_INSERT) && ('\0' != KEY_INSERT[0]))
				insert_key = strncmp((const char *)escape_sequence, KEY_INSERT, escape_length);
			memset(escape_sequence, '\0', escape_length);
			escape_length = 0;
			if (BADESC == io_ptr->esc_state)
			{
				io_ptr->esc_state = START;
				break;
			}
			if ((0 == backspace) || (0 == delete))
			{
				if (0 < instr)
				{
					MOVE_CURSOR_LEFT_ONE_CHAR(dx, instr, dx_instr, dx_start, ioptr_width);
					DEL_ONE_CHAR_AT_CURSOR(outlen, dx_outlen, dx, dx_instr, dx_start, ioptr_width);
				}
			}
			if (0 == insert_key)
				insert_mode = !insert_mode;	/* toggle */
			else if ((0 == up) || (0 == down))
			{
				DOWRITE_A(tt_ptr->fildes, &cr, 1);
				WRITE_GTM_PROMPT;
				gtm_tputs(CLR_EOL, 1, outc);
				instr = dx_instr = outlen = dx_outlen = 0;
				if (0 == up)
				{
					if (((MAX_RECALL + 1 != index) && (0 != (*(comline_base + cl)).len) || (0 == index)))
						index++;
				} else
				{
					assert (0 == down);
					if (0 != index)
						 --index;
				}
				if ((0 < index) && (MAX_RECALL >= index))
				{
					cl = clmod (comline_index - index);
					instr = outlen = comline_base[cl].len;
#					ifdef UNICODE_SUPPORTED
					if (utf8_active)
					{
						len = comline_base[cl].len;
						instr = dx_instr = 0;
						outtop = (unsigned char *)comline_base[cl].addr + len;
						for (outptr = (unsigned char *)comline_base[cl].addr; outptr < outtop; )
						{
							outptr = UTF8_MBTOWC(outptr, outtop, inchar);
							if (WEOF != inchar)
							{
								STORE_OFF(inchar, instr);
								instr++;
							}
						}
						outlen = instr;
						dx_instr = compute_dx(BUFF_ADDR(0), outlen, ioptr_width, dx_start);
						dx_outlen = dx_instr;
					} else
					{
#					endif
						if (0 != instr)
							memcpy(buffer_start, comline_base[cl].addr, outlen);
						dx_instr = dx_outlen = comline_base[cl].len;
#					ifdef UNICODE_SUPPORTED
					}
#					endif
					dx = (unsigned)(dx_instr + dx_start) % ioptr_width;
					if (0 != instr)
						write_str(BUFF_ADDR(0), instr, dx_start, TRUE, FALSE);
				}
			} else if (!(mask & TRM_NOECHO) && !((0 == right) && (instr == outlen)) && !((0 == left) && (0 == instr)))
			{
				if (0 == right)
				{
					dx_next = compute_dx(BUFF_ADDR(0), instr + 1, ioptr_width, dx_start);
					inchar_width = dx_next - dx_instr;
					move_cursor_right(dx, inchar_width);
					instr++;
					dx = (dx + inchar_width) % ioptr_width;
					dx_instr += inchar_width;
				}
				if (0 == left)
				{
					dx_prev = compute_dx(BUFF_ADDR(0), instr - 1, ioptr_width, dx_start);
					inchar_width = dx_instr - dx_prev;
					move_cursor_left(dx, inchar_width);
					instr--;
					dx = (dx - inchar_width + ioptr_width) % ioptr_width;
					dx_instr -= inchar_width;
				}
			}
			io_ptr->esc_state = START;
		}
	}
	/* turn keypad off */
#	ifndef __MVS__
	if ((NULL != KEYPAD_LOCAL) && (keypad_len = STRLEN(KEYPAD_LOCAL)))	/* NOTE assignment */
		DOWRITE(tt_ptr->fildes, KEYPAD_LOCAL, keypad_len);
#	endif
	if (outlen == length)
		outlen = length - 1;
	io_ptr->dollar.za = 0;
	v->mvtype = MV_STR;
#	ifdef UNICODE_SUPPORTED
	if (utf8_active)
	{
		int	i;
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
	if (0 != v->str.len)
	{
		cl = clmod (comline_index - 1);
		if ((v->str.len != comline_base[cl].len) || (memcmp(comline_base[cl].addr, buffer_start, v->str.len)))
		{
			comline_base[comline_index] = v->str;
			comline_index = clmod (comline_index + 1);
			if (MAX_RECALL != recall_num)
				recall_num ++;
		}
		if (buffer_start == stringpool.free)	/* BYPASSOK */
			stringpool.free += v->str.len;	/* otherwise using space from before interrupt */
	}
	if (!(mask & TRM_NOECHO))
	{
		if ((io_ptr->dollar.x += dx_outlen) >= ioptr_width && io_ptr->wrap)
		{
			io_ptr->dollar.y += (io_ptr->dollar.x / ioptr_width);
			if (0 != io_ptr->length)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x %= ioptr_width;
			if (0 == io_ptr->dollar.x)
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));	/* BYPASSOK */
		}
	}
	active_device = 0;
	return;
}
