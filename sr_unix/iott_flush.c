/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "have_crit.h"
#include "iott_flush_time.h"
#include "util.h"
#include "svnames.h"
#include "deferred_events.h"
#include "deferred_events_queue.h"

GBLREF boolean_t	hup_on, prin_in_dev_failure, prin_out_dev_failure;
GBLREF int		process_exiting;
GBLREF int4		error_condition, exi_condition;
GBLREF io_pair		io_curr_device, io_std_device;
GBLREF mval		dollar_zstatus;

error_def(ERR_NOPRINCIO);
error_def(ERR_TERMHANGUP);
error_def(ERR_TERMWRITE);

void iott_flush_buffer(io_desc *io_ptr, boolean_t new_write_flag)
{
	d_tt_struct	*tt_ptr;
	int4		status;
	ssize_t		write_len;
	boolean_t	ch_set;

	tt_ptr = io_ptr->dev_sp;
	if (ERR_TERMHANGUP == error_condition)
		TERMHUP_NOPRINCIO_CHECK(TRUE);	/* ensure we never partially lose the HANGUP */
	if (!tt_ptr->write_active)
		return;	/* Was assert but that ended up causing endless loops -- now we just survive */
	ESTABLISH_GTMIO_CH(&io_ptr->pair, ch_set);
	write_len = (ssize_t)(tt_ptr->tbuffp - tt_ptr->ttybuff);
	if (0 < write_len)
	{
		DOWRITERC(tt_ptr->fildes, tt_ptr->ttybuff, write_len, status);
		if ((0 == status) && (ERR_TERMHANGUP != error_condition))
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
			tt_ptr->write_active = FALSE;				/* In case we come back this-a-way */
			if ((io_ptr == io_std_device.out) && !prin_out_dev_failure)
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3)
					ERR_TERMHANGUP != error_condition ? ERR_TERMWRITE : ERR_TERMHANGUP, 0, status);
				prin_out_dev_failure = TRUE;			/* set flag for NOPRINCIO, to give app a chance */
			}
			xfer_set_handlers(defer_error, status, FALSE);
		}
	}
	tt_ptr->write_active = new_write_flag;
	REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
}

void iott_flush(io_desc *io_ptr)
{
	d_tt_struct	*tt_ptr;
	boolean_t	ch_set;

	ESTABLISH_GTMIO_CH(&io_ptr->pair, ch_set);
	assert(tt == io_ptr->type);
	tt_ptr = io_ptr->dev_sp;
	if (tt_ptr->timer_set)
	{
		cancel_timer((TID)io_ptr);
		tt_ptr->timer_set = FALSE;
	}
	if (tt_ptr->write_active)
	{
		REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
		return;	/* We are nesting the buffer flush -- let original copy do its job */
	}
	tt_ptr->write_active = TRUE;
	iott_flush_buffer(io_ptr, FALSE);
	REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
}

void iott_flush_time(TID id, int4 hd_len, io_desc **io_ptr_parm)
{
	io_desc		*io_ptr, *flush_parm;
	d_tt_struct	*tt_ptr;
	boolean_t	flush_immediately;
	boolean_t	ch_set;

	io_ptr = *io_ptr_parm;
	ESTABLISH_GTMIO_CH(&io_ptr->pair, ch_set);
	assert(tt == io_ptr->type);
	tt_ptr = io_ptr->dev_sp;
	assert(tt_ptr->timer_set);
	if ((FALSE == tt_ptr->write_active) && !prin_out_dev_failure) /* If not in write code and no failure already, do flush */
	{
		tt_ptr->timer_set = FALSE;
		tt_ptr->write_active = TRUE;
		iott_flush_buffer(io_ptr, FALSE);
	} else					/* doing write, reschedule the flush */
	{	/* We cannot be starting unsafe timers during process exiting or in an interrupt-deferred window. */
		flush_immediately = (process_exiting || (INTRPT_OK_TO_INTERRUPT != intrpt_ok_state));
		assert(!flush_immediately || (FALSE == tt_ptr->write_active));
		if (!flush_immediately)
		{
			flush_parm = io_ptr;
			start_timer((TID)io_ptr,
				    IOTT_FLUSH_RETRY,
				    &iott_flush_time,
				    SIZEOF(flush_parm),
				    (char *)&flush_parm);
		}
	}
	REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
}
