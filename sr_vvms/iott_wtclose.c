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

#include <dcdef.h>
#include <iodef.h>
#include <ssdef.h>
#include <efndef.h>

#include "io.h"
#include "iottdef.h"

GBLREF io_pair		io_std_device;

void iott_wtclose(d_tt_struct *tt_ptr)
{
	uint4		status;
	short		length;
	unsigned char	sensemode[8];

	if (tt_ptr->clock_on)
	{
		if (SS$_NORMAL != (status = sys$cantim(tt_ptr, 0)))
			rts_error(VARLSTCNT(1) status);
	}
	if (tt_ptr->io_free < tt_ptr->io_inuse)
		length = tt_ptr->io_buftop - tt_ptr->io_inuse;
	else
		length = tt_ptr->io_free - tt_ptr->io_inuse;
	tt_ptr->sb_free->start_addr = tt_ptr->io_inuse;
	if (SS$_NORMAL != (status = sys$qiow(EFN$C_ENF, tt_ptr->channel, tt_ptr->write_mask, &(tt_ptr->sb_free->iosb_val),
						iott_wtfini, tt_ptr, tt_ptr->io_inuse, length, 0, 0, 0, 0)))
		rts_error(VARLSTCNT(1) status);
	status = sys$qiow(EFN$C_ENF, tt_ptr->channel, IO$_SENSEMODE | IO$M_RD_MODEM, &tt_ptr->sb_free->iosb_val, 0, 0,
				sensemode, 0 ,0, 0, 0, 0);
	if ((io_std_device.in->dev_sp != tt_ptr) &&
		(io_std_device.out->dev_sp != tt_ptr) &&
		(SS$_NORMAL == status) && (DT$_LAT == sensemode[0]))
	{
		if (SS$_NORMAL != (status = sys$qiow(EFN$C_ENF, tt_ptr->channel, IO$_TTY_PORT | IO$M_LT_DISCON,
					&tt_ptr->sb_free->iosb_val, 0, 0, 0, 0, 0, 0, 0, 0)))
			rts_error(VARLSTCNT(1) status);
	}
	tt_ptr->io_inuse = tt_ptr->io_free;
	tt_ptr->sb_free++;
	tt_ptr->sb_free = new_sbfree(tt_ptr);
	return;
}
