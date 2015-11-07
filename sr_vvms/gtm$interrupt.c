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
#include "io.h"
#include "iottdef.h"
#include "outofband.h"
#include "deferred_events.h"

GBLREF io_pair 	io_std_device;
GBLREF bool	std_dev_outbnd;

void gtm$interrupt(int4 ob_char)
{
	uint4	mask;
	d_tt_struct      *tt_ptr;

	if (io_std_device.in->type != tt || ob_char > MAXOUTOFBAND)
		return;

	tt_ptr = (d_tt_struct *) io_std_device.in->dev_sp;
	std_dev_outbnd = TRUE;
	mask = 1 << ob_char;
	if (mask & tt_ptr->enbld_outofbands.mask)
	{	ctrap_set(ob_char);
	}
	else if (mask & CTRLC_MSK)
	{	ctrlc_set(0);
	}
	else if (mask & CTRLY_MSK)
	{	ctrly_set(0);
	}
	return;
}
