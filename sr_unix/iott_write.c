/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_unistd.h"

#include <signal.h>
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
#include "iott_flush_time.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF io_pair		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF bool		prin_out_dev_failure;
GBLREF boolean_t	gtm_utf8_mode;

void  iott_write_buffered_text(io_desc *io_ptr, char *text, int textlen);

void  iott_write_buffered_text(io_desc *io_ptr, char *text, int textlen)
{
	d_tt_struct	*tt_ptr;
	int		buff_left, status;
	error_def(ERR_NOPRINCIO);
	error_def(ERR_TERMWRITE);

	tt_ptr = io_ptr->dev_sp;
	assert(tt_ptr->write_active == FALSE);
	tt_ptr->write_active = TRUE;
 	buff_left = IOTT_BUFF_LEN - (int)((tt_ptr->tbuffp - tt_ptr->ttybuff));
	assert(buff_left > IOTT_BUFF_MIN || prin_out_dev_failure);
	if (buff_left < textlen)
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
		        iott_flush_buffer(io_ptr, FALSE);	/* tt_ptr->write_active = FALSE */
		else
			tt_ptr->write_active = FALSE;
        } else
	{
		DOWRITERC(tt_ptr->fildes, text, textlen, status);
		tt_ptr->write_active = FALSE;
		if (0 == status)
		{
			if (io_ptr == io_std_device.out)
			{	/* ------------------------------------------------
				 * set prin_out_dev_failure to FALSE in case it
				 * had been set TRUE earlier and is now working.
				 * for eg. a write fails and the next write works.
				 * ------------------------------------------------
				 */
				prin_out_dev_failure = FALSE;
			}
		} else 	/* (0 != status) */
		{
			if (io_ptr == io_std_device.out)
			{
				if (!prin_out_dev_failure)
					prin_out_dev_failure = TRUE;
				else
				{
					send_msg(VARLSTCNT(1) ERR_NOPRINCIO);
					/* rts_error(VARLSTCNT(1) ERR_NOPRINCIO); This causes a core dump */
					stop_image_no_core();
				}
			}
			rts_error(VARLSTCNT(3) ERR_TERMWRITE, 0, status);
		}
	}
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
	error_def(ERR_TERMWRITE);
	error_def(ERR_ZINTRECURSEIO);

	str_len = v->len;
	if (0 != str_len)
	{
		str = v->addr;
		io_ptr = io_curr_device.out;
		tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
		if (tt_ptr->mupintr)
			rts_error(VARLSTCNT(1) ERR_ZINTRECURSEIO);
		UNICODE_ONLY(utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ochset) : FALSE;)
		for (;  ;)
		{
			if (FALSE == io_ptr->wrap)
				len = str_len;
			else
			{
				if ((io_ptr->dollar.x >= io_ptr->width) && (START == io_ptr->esc_state))
				{
					iott_write_buffered_text(io_ptr, STR_AND_LEN(NATIVE_TTEOL));
					io_ptr->dollar.y++;
					io_ptr->dollar.x = 0;
				}
				if (START != io_ptr->esc_state)
					len = str_len;			/* write all if in escape sequence */
#ifdef UNICODE_SUPPORTED
				else if (utf8_active)
				{
					ptrtop = (unsigned char *)str + str_len;
					avail_width = io_ptr->width - io_ptr->dollar.x;
					for (this_width = 0, ptr = (unsigned char *)str; ptr < ptrtop; ptr = ptrnext)
					{
						ptrnext = UTF8_MBTOWC(ptr, ptrtop, codepoint);
						if (WEOF == codepoint)
							UTF8_BADCHAR(0, ptr, ptrtop, 0, NULL);
						GTM_IO_WCWIDTH(codepoint, char_width);
						if ((this_width + char_width) > avail_width)
							break;
						this_width += char_width;
					}
					len = (int)(ptr - (unsigned char *)str);
					if (0 == len)
					{
						if (char_width <= io_ptr->width)
						{
							io_ptr->dollar.x = io_ptr->width;	/* force wrap */
							continue;
						} else
							rts_error(VARLSTCNT(1) ERR_TERMWRITE);
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

		if (FALSE == tt_ptr->timer_set)
		{
			flush_parm = io_ptr;
			tt_ptr->timer_set = TRUE;
			start_timer((TID)io_ptr,
				    IOTT_FLUSH_WAIT,
				    &iott_flush_time,
				    SIZEOF(flush_parm),
				    (char *)&flush_parm);
		}

	}
}
