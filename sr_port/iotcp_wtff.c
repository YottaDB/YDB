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
#include "gtm_stdio.h"
#include "io.h"

#define FORM_FEED "\014"
GBLREF io_pair		io_curr_device;

void iotcp_wtff(void)
{
	mstr		temp;
	io_desc		*iod;

#ifdef DEBUG_TCP
    PRINTF("%s >>>\n",__FILE__);
#endif
	iod = io_curr_device.out;
#ifdef C9A06001531
	iotcp_wteol(1,iod);
#endif
	temp.len = SIZEOF(FORM_FEED) - 1;
	temp.addr = FORM_FEED;
	iotcp_write(&temp);
#ifdef C9A06001531
	iotcp_wteol(1,iod);
#endif
	iod->dollar.x = 0;
	iod->dollar.y = 0;
#ifdef DEBUG_TCP
    PRINTF("%s <<<\n",__FILE__);
#endif
}
