/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> eb3ea98c (GT.M V7.0-002)
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

<<<<<<< HEAD
short ious_open(io_log_name *dev, mval *pp, int file_des, mval *mspace, uint8 timeout)
{
	io_desc *iod;
=======
/* Open a device belonging to the user defined nmemonicspace */
short ious_open(io_log_name *dev, mval *pp, int file_des, mval *mspace, int4 timeout)
{
	io_desc *iod;
	dev_dispatch_struct *fgn_driver;
>>>>>>> eb3ea98c (GT.M V7.0-002)

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
<<<<<<< HEAD
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVMNEMCSPC, 2, mspace->str.len, mspace->str.addr);
		}
		assert(NULL == (((d_us_struct*)(iod->dev_sp))->disp));
		/* Ideally we should have something like the following.
		 *
		 *	((void(*)())(((d_us_struct*)(iod->dev_sp))->disp->open))();
		 *	iod->state = dev_open;
		 *
		 * But we don't know which user device driver open function to dispatch to. So issue an error for now.
		 */
		rts_error(VARLSTCNT(1) ERR_USRIOINIT);
=======
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
>>>>>>> eb3ea98c (GT.M V7.0-002)
	}
	return TRUE;
}
