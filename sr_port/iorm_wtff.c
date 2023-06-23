/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"
#include "iormdef.h"
#include "error.h"

#define	FORM_FEED	"\014"

GBLREF	io_pair		io_curr_device;

void iorm_wtff(void)
{
	mstr		temp;
	io_desc		*iod;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	iod = io_curr_device.out;
	ESTABLISH_GTMIO_CH(&io_curr_device, ch_set);
	iorm_cond_wteol(iod);
	temp.len = SIZEOF(FORM_FEED) - 1;
	temp.addr = FORM_FEED;
	iorm_write(&temp);
	if (iod->fflf)			/* GTM-9136: If fflf is FALSE, we don't follow the FF with an NL */
		iorm_wteol(1, iod);
	iod->dollar.x = 0;
	iod->dollar.y = 0;
	REVERT_GTMIO_CH(&io_curr_device, ch_set);
}
