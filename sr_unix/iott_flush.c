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

#include <errno.h>

#include "error.h"
#include "io.h"
#include "iottdef.h"
#include "gtmio.h"
#include "gt_timer.h"
#include "send_msg.h"
#include "iott_flush_time.h"
#include "deferred_events.h"

GBLREF io_pair	io_curr_device;
GBLREF io_pair	io_std_device;
GBLREF bool	prin_out_dev_failure;

void iott_flush_buffer(io_desc *io_ptr, boolean_t new_write_flag)
{
	d_tt_struct	*tt_ptr;
	int4		status;
	ssize_t		write_len;
	error_def(ERR_NOPRINCIO);

	tt_ptr = io_ptr->dev_sp;
	if (!tt_ptr->write_active)
		return;	/* Was assert but that ended up causing endless loops -- now we just survive */
	write_len = (ssize_t)(tt_ptr->tbuffp - tt_ptr->ttybuff);

	if (0 < write_len)
	{
		DOWRITERC(tt_ptr->fildes, tt_ptr->ttybuff, write_len, status);
		if (0 == status)
		{
			tt_ptr->tbuffp = tt_ptr->ttybuff;
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
			tt_ptr->write_active = FALSE;		/* In case we come back this-a-way */
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
			xfer_set_handlers(tt_write_error_event, tt_write_error_set, status);
		}
	}
	tt_ptr->write_active = new_write_flag;
}

void iott_flush(io_desc *io_ptr)
{
	d_tt_struct	*tt_ptr;

	tt_ptr = io_ptr->dev_sp;
	if (tt_ptr->timer_set)
	{
		cancel_timer((TID)io_ptr);
		tt_ptr->timer_set = FALSE;
	}
	if (tt_ptr->write_active)
		return;	/* We are nesting the buffer flush -- let original copy do its job */
	tt_ptr->write_active = TRUE;
	iott_flush_buffer(io_ptr, FALSE);
}

void iott_flush_time(TID id, int4 hd_len, io_desc **io_ptr_parm)
{
	io_desc		*io_ptr, *flush_parm;
	d_tt_struct	*tt_ptr;

	io_ptr = *io_ptr_parm;
	tt_ptr = io_ptr->dev_sp;

	assert(tt_ptr->timer_set);
	if (FALSE == tt_ptr->write_active && !prin_out_dev_failure) /* If not in write code and no failure already, do flush */
	{
		tt_ptr->timer_set = FALSE;
		tt_ptr->write_active = TRUE;
		iott_flush_buffer(io_ptr, FALSE);
	} else					/* doing write, reschedule the flush */
	{
		flush_parm = io_ptr;
		start_timer((TID)io_ptr,
			    IOTT_FLUSH_RETRY,
			    &iott_flush_time,
			    SIZEOF(flush_parm),
			    (char *)&flush_parm);
	}
}

