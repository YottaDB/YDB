/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <ssdef.h>

#include "efn.h"
#include "io.h"
#include "iottdef.h"
#include "xfer_enum.h"
#include "op.h"
#include "deferred_events.h"

GBLREF io_pair	io_curr_device;
GBLREF io_pair	io_std_device;
GBLREF bool	prin_out_dev_failure;
GBLREF int	(*xfer_table[])();

void iott_wtstart(d_tt_struct *tt_ptr)
{
	int	length;
	uint4	status;

	if (tt_ptr->io_free != tt_ptr->io_inuse)
	{
		tt_ptr->sb_free->start_addr = tt_ptr->io_inuse;
		if (tt_ptr->io_free < tt_ptr->io_inuse)
		{
			length = tt_ptr->io_buftop - tt_ptr->io_inuse;
			tt_ptr->io_inuse = tt_ptr->io_buffer;
		} else
		{
			length = tt_ptr->io_free - tt_ptr->io_inuse;
			tt_ptr->io_inuse = tt_ptr->io_free;
		}
		if (SS$_NORMAL == (status = sys$qio(
					 efn_iott_write
					,tt_ptr->channel
					,tt_ptr->write_mask
					,&(tt_ptr->sb_free->iosb_val)
					,iott_wtfini
					,tt_ptr
					,tt_ptr->sb_free->start_addr
					,length
					,0
					,0
					,0
					,0)))
		{
			prin_out_dev_failure = FALSE;
			tt_ptr->sb_free++;
			if (tt_ptr->sb_free == tt_ptr->sb_buftop)
				tt_ptr->sb_free = tt_ptr->sb_buffer;
		} else
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
	}
	return;
}
