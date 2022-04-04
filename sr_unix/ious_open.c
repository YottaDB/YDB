/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "io.h"
#include "iousdef.h"

error_def(ERR_USRIOINIT);

/* Open a device belonging to the user defined nmemonicspace */
short ious_open(io_log_name *dev, mval *pp, int file_des, mval *mspace, int4 timeout)
{
	io_desc *iod;
	dev_dispatch_struct *fgn_driver;

	iod = dev->iod;
	if (iod->state == dev_never_opened)
	{
		iod->dev_sp = (void *)malloc(SIZEOF(d_us_struct));
		memset(iod->dev_sp, 0, SIZEOF(d_us_struct));
		iod->state = dev_closed;
	}

	if (iod->state != dev_open)
	{
		if (mspace && mspace->str.len)
		{	/* Currently there is no mechanism implementing a user nmemonicspace device. As a
			 * result, io_get_fgn_driver() issues an rts_error() which leaves
			 * (NULL == (d_us_struct*)(iod->dev_sp))->disp) true. If a user nmemonicspace is
			 * implemented, it must open the device and complete enough setup/configuration to
			 * avoid a memory access violation.
			 */
			fgn_driver = io_get_fgn_driver(&mspace->str);
			((d_us_struct*)(iod->dev_sp))->disp = fgn_driver;
		} else if ((NULL == ((d_us_struct*)(iod->dev_sp))->disp)
				|| (NULL == (((d_us_struct*)(iod->dev_sp))->disp->open)))
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_USRIOINIT);
			return FALSE;
		}
		((void(*)())(((d_us_struct*)(iod->dev_sp))->disp->open))();
		iod->state = dev_open;
	}
	return TRUE;
}
