/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"

#include "gtm_signal.h"
#include "gtm_string.h"
#include <errno.h>
#include <wctype.h>
#include <wchar.h>

#include "io.h"
#include "iottdef.h"
#include "gtmio.h"
#include "gt_timer.h"
#include "send_msg.h"
#include "error.h"
#include "dollarx.h"
#include "have_crit.h"
#include "iott_flush_time.h"
#include "svnames.h"
#include "op.h"
#include "util.h"
#include "deferred_events_queue.h"
#ifdef UTF8_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF boolean_t	gtm_utf8_mode, hup_on, prin_in_dev_failure, prin_out_dev_failure;
GBLREF int		exi_condition, process_exiting;
GBLREF int4		error_condition;
GBLREF io_pair		io_curr_device, io_std_device;
GBLREF mval		dollar_zstatus;
GBLREF volatile int4	outofband;

error_def(ERR_NOPRINCIO);
error_def(ERR_TERMHANGUP);
error_def(ERR_TERMWRITE);
error_def(ERR_ZINTRECURSEIO);

void  iott_write_buffered_text(io_desc *io_ptr, char *text, int textlen);

void  iott_write_buffered_text(io_desc *io_ptr, char *text, int textlen)
{
	d_tt_struct	*tt_ptr;
	int		buff_left, status;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	tt_ptr = io_ptr->dev_sp;
	assert(tt_ptr->write_active == FALSE);
	ESTABLISH_GTMIO_CH(&io_ptr->pair, ch_set);
	if (sighup == outofband)
	{
		TERMHUP_NOPRINCIO_CHECK(TRUE);					/* TRUE for WRITE */
		return;
	}
	tt_ptr->write_active = TRUE;
 	buff_left = IOTT_BUFF_LEN - (int)TT_UNFLUSHED_DATA_LEN(tt_ptr);
	assert(buff_left > IOTT_BUFF_MIN || prin_out_dev_failure);
	if (!prin_out_dev_failure && (buff_left < textlen))
        {
	        iott_flush_buffer(io_ptr, TRUE);
		buff_left = IOTT_BUFF_LEN;
        }
	if (textlen <= buff_left)
        {
		memcpy((void *)tt_ptr->tbuffp, text, textlen);
		tt_ptr->tbuffp += textlen;
		buff_left -= textlen;
		if (buff_left <= IOTT_BUFF_MIN)
		        iott_flush_buffer(io_ptr, FALSE);			/* tt_ptr->write_active = FALSE */
		else
			tt_ptr->write_active = FALSE;
        } else
	{
		DOWRITERC(tt_ptr->fildes, text, textlen, status);
		tt_ptr->write_active = FALSE;
		if ((0 == status) && (ERR_TERMHANGUP != error_condition))
		{
			if ((io_ptr == io_std_device.out) && (prin_out_dev_failure))
			{	/* if it was TRUE, try flush we may have skipped above & clear flag because write worked */
				prin_out_dev_failure = FALSE;
				iott_flush_buffer(io_ptr, TRUE);
			}
		} else 	/* (0 != status) */
		{
			ISSUE_NOPRINCIO_IF_NEEDED(io_ptr, TRUE, FALSE);		/* TRUE, FALSE: WRITE, not socket */
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_TERMWRITE, 0, status);
		}
	}
	REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
}

void iott_write(mstr *v)
{
	unsigned	str_len;
	unsigned	len;
	int		status, this_width, char_width, avail_width;
	char		*str;
	unsigned char	*ptr, *ptrnext, *ptrtop;
	io_desc		*io_ptr, *flush_parm;
	d_tt_struct	*tt_ptr;
	boolean_t	utf8_active = FALSE;
	wint_t		codepoint;
	boolean_t	flush_immediately;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* We cannot be starting unsafe timers during process exiting or in an interrupt-deferred window. */
	flush_immediately = (process_exiting || (INTRPT_OK_TO_INTERRUPT != intrpt_ok_state));
	str_len = v->len;
	if (0 != str_len)
	{
		str = v->addr;
		io_ptr = io_curr_device.out;
		tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
		if (tt_ptr->mupintr)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTRECURSEIO);
		ESTABLISH_GTMIO_CH(&io_curr_device, ch_set);
		UTF8_ONLY(utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ochset) : FALSE;)
		for (; ;)
		{
			if (FALSE == io_ptr->wrap)
				len = str_len;
			else
			{
				if ((io_ptr->dollar.x >= io_ptr->width) && (START == io_ptr->esc_state))
				{
					iott_write_buffered_text(io_ptr, STR_AND_LEN(NATIVE_TTEOL));
					io_ptr->dollar.y++;
					if (io_ptr->length)
						io_ptr->dollar.y %= io_ptr->length;
					io_ptr->dollar.x = 0;
				}
				if (START != io_ptr->esc_state)
					len = str_len;			/* write all if in escape sequence */
#ifdef UTF8_SUPPORTED
				else if (utf8_active)
				{
					ptrtop = (unsigned char *)str + str_len;
					avail_width = io_ptr->width - io_ptr->dollar.x;
					this_width = 0;
					ptr = (unsigned char *)str;
					assert(ptr < ptrtop);
					do
					{
						ptrnext = UTF8_MBTOWC(ptr, ptrtop, codepoint);
						if (WEOF == codepoint)
							UTF8_BADCHAR(0, ptr, ptrtop, 0, NULL);
						GTM_IO_WCWIDTH(codepoint, char_width);
						if ((this_width + char_width) > avail_width)
							break;
						this_width += char_width;
						ptr = ptrnext;
					} while(ptr < ptrtop);
					len = (int)(ptr - (unsigned char *)str);
					if (0 == len)
					{
						if (char_width <= io_ptr->width)
						{
							io_ptr->dollar.x = io_ptr->width;	/* force wrap */
							continue;
						} else
							RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TERMWRITE);
					}
				}
#endif
				else
					if ((io_ptr->dollar.x + str_len) <= io_ptr->width)
						len = str_len;
					else
						len = io_ptr->width - io_ptr->dollar.x;
			}
			assert(0 != len);
			iott_write_buffered_text(io_ptr, str, len);
			dollarx(io_ptr, (uchar_ptr_t)str, (uchar_ptr_t)str + len);
			str_len -= len;
			if (0 >= (signed)str_len)
				break;
			str += len;
		}
		if (flush_immediately)
		{
			if (TRUE == tt_ptr->timer_set)
			{
				cancel_timer((TID)io_ptr);
				tt_ptr->timer_set = FALSE;
			}
			tt_ptr->write_active = TRUE;
			iott_flush_buffer(io_ptr, FALSE);
		} else if (FALSE == tt_ptr->timer_set)
		{
			flush_parm = io_ptr;
			tt_ptr->timer_set = TRUE;
			start_timer((TID)io_ptr,
				    IOTT_FLUSH_WAIT,
				    &iott_flush_time,
				    SIZEOF(flush_parm),
				    (char *)&flush_parm);
		}
		REVERT_GTMIO_CH(&io_curr_device, ch_set);
	}
}
