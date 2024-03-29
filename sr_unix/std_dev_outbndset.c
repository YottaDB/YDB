/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "std_dev_outbndset.h"

#define SHFT_MSK 0x00000001

GBLREF boolean_t		ctrlc_on, hup_on;
GBLREF volatile io_pair	io_std_device;
GBLREF volatile bool		std_dev_outbnd;

/* NOTE: xfer_set_handlers() returns success or failure for attempts to set
 *       xfer_table. That value is not currently used here, hence the
 *       cast to void.
 */

void std_dev_outbndset(int4 ob_char)
{
	uint4		mask;
	unsigned short	n;
	d_tt_struct	*tt_ptr;

	assertpro(MAXOUTOFBAND >= ob_char);
	if ((NULL != io_std_device.in) && (tt == io_std_device.in->type))
	{	/* The NULL check above protects against a <CTRL-C> hitting while we're initializing the terminal */
		tt_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
		std_dev_outbnd = TRUE;
		mask = SHFT_MSK << ob_char;
		if (mask & tt_ptr->enbld_outofbands.mask)
			(void)xfer_set_handlers(ctrap, ob_char, FALSE);
		else if (CTRLC_MSK & mask)
		{
			if (ctrlc_on)
				(void)xfer_set_handlers(ctrlc, 0, FALSE);
		} else if (hup_on && (SIGHUP_MSK & mask))
			(void)xfer_set_handlers(sighup, ob_char, FALSE);
		else
			assertpro((mask & tt_ptr->enbld_outofbands.mask) || (OUTOFBAND_MSK & mask));
	}
}
