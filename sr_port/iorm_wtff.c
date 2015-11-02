/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"

#define FORM_FEED "\014"
GBLREF io_pair		io_curr_device;

void iorm_wtff(void)
{
	mstr		temp;
	io_desc		*iod;

	iod = io_curr_device.out;
	iorm_flush(iod);
	temp.len = SIZEOF(FORM_FEED) - 1;
	temp.addr = FORM_FEED;
	iorm_write(&temp);
	iorm_wteol(1,iod);
	iod->dollar.x = 0;
	iod->dollar.y = 0;
}
