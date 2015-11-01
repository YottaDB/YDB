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
#include "gtm_unistd.h"

#include <signal.h>
#include <string.h>
#include <errno.h>

#include "io.h"
#include "iottdef.h"
#include "gtmio.h"
#include "gt_timer.h"
#include "send_msg.h"
#include "error.h"
#include "dollarx.h"
#include "iott_flush_time.h"

GBLREF io_pair	io_curr_device;
GBLREF io_pair	io_std_device;
GBLREF bool	prin_out_dev_failure;


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
	buff_left = IOTT_BUFF_LEN - (tt_ptr->tbuffp - tt_ptr->ttybuff);
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
	int		status;
	char		*str;
	io_desc		*io_ptr, *flush_parm;
	d_tt_struct	*tt_ptr;

	str_len = v->len;
	if (0 != str_len)
	{
		str = v->addr;
		io_ptr = io_curr_device.out;
		tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
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
				if ((START != io_ptr->esc_state) || ((int)(io_ptr->dollar.x + str_len) <= (int)io_ptr->width))
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
				    sizeof(flush_parm),
				    (char *)&flush_parm);
		}

	}
}
