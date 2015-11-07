/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>	/* for VSIG_ATOMIC_T type */

#include "error.h"
#include "have_crit.h"


#define UNWIND_LEVELS 4
/* 0 unwinds EXI_CH, 1 unwinds EXI_RUNDOWN,  2,3 and 4 unwind the three VMS handlers,
   thus returning to pc was at when the exit condition was received.            */

static int      		depth = UNWIND_LEVELS;

CONDITION_HANDLER(exi_ch)
{
        START_CH;
	SET_FORCED_EXIT_STATE;
        UNWIND(&depth, NULL);
}
