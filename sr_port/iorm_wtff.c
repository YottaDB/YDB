/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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
#include "iormdef.h"
#include "error.h"

#define	FORM_FEED	"\014"

GBLREF	io_pair		io_curr_device;

void iorm_wtff(void)
{
	mstr		temp;
	io_desc		*iod;
	boolean_t	ch_set;

	iod = io_curr_device.out;
	ESTABLISH_GTMIO_CH(&io_curr_device, ch_set);
	iorm_cond_wteol(iod);
	temp.len = SIZEOF(FORM_FEED) - 1;
	temp.addr = FORM_FEED;
	iorm_write(&temp);
	iorm_wteol(1, iod);
	iod->dollar.x = 0;
	iod->dollar.y = 0;
	REVERT_GTMIO_CH(&io_curr_device, ch_set);
}
