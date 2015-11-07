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

#include <iodef.h>
#include <ssdef.h>
#include <efndef.h>

#include "io.h"
#include "iottdef.h"
#include "xfer_enum.h"
#include "deferred_events.h"

GBLREF io_pair	io_curr_device;
GBLREF io_pair	io_std_device;
GBLREF bool	prin_out_dev_failure;
GBLREF int	(*xfer_table[])();

void iott_flush(io_desc *ioptr)
{
	void		op_fetchintrrpt(), op_startintrrpt(), op_forintrrpt();
	char		*c_ptr;
	short		iosb[4], length;
	uint4		status;
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct*)ioptr->dev_sp;
	sys$cantim(tt_ptr, 0);
	tt_ptr->clock_on = FALSE;
	if (tt_ptr->io_free != tt_ptr->io_inuse)
	{
		c_ptr = tt_ptr->io_inuse;
		if (tt_ptr->io_free < tt_ptr->io_inuse)
		{
			length = tt_ptr->io_buftop - tt_ptr->io_inuse;
			tt_ptr->io_inuse = tt_ptr->io_buffer;
		} else
		{
			length = tt_ptr->io_free - tt_ptr->io_inuse;
			tt_ptr->io_inuse = tt_ptr->io_free;
		}
		tt_ptr->io_pending = c_ptr + length;
		if (tt_ptr->io_pending == tt_ptr->io_buftop)
			tt_ptr->io_pending = tt_ptr->io_buffer;
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel, tt_ptr->write_mask, iosb, 0, 0, c_ptr, length, 0, 0, 0, 0);
		if (status & 1)
			status = iosb[0];
		if (status & 1)
			prin_out_dev_failure = FALSE;
		else
		{
			if (io_curr_device.out == io_std_device.out)
			{
				if (prin_out_dev_failure)
					sys$exit(status);
				else
					prin_out_dev_failure = TRUE;
			}
			xfer_set_handlers(tt_write_error_event, tt_write_error_set, status);
		}
	} else
		while (tt_ptr->sb_free != tt_ptr->sb_pending)
			sys$hiber();
}
