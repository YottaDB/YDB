/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
short ious_open(io_log_name *dev, mval *pp, int file_des, mval *mspace, uint8 timeout)
{
	io_desc *iod;

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
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVMNEMCSPC, 2, mspace->str.len, mspace->str.addr);
		assert(NULL == (((d_us_struct*)(iod->dev_sp))->disp));
		/* Ideally we should have something like the following.
		 *
		 *	((void(*)())(((d_us_struct*)(iod->dev_sp))->disp->open))();
		 *	iod->state = dev_open;
		 *
		 * But we don't know which user device driver open function to dispatch to. So issue an error for now.
		 */
		rts_error(VARLSTCNT(1) ERR_USRIOINIT);
	}
	return TRUE;
}
