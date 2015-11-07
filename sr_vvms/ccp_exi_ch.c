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
#include "error.h"


#define UNWIND_LEVELS 3
/* 0 unwinds CCP_EXI_CH, 1 unwinds CCP_RUNDOWN,  2,3 and 4 unwind the three VMS handlers,
   thus returning to the pc when the exit condition was received.		*/

static uint4	depth = UNWIND_LEVELS;

CONDITION_HANDLER(ccp_exi_ch)
{
	START_CH;
	UNWIND(&depth, 0);
}
