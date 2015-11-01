/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "dm_setup.h"

GBLREF stack_frame	*frame_pointer;

void dm_setup(void)
{
	new_stack_frame(frame_pointer->rvector, GTM_CONTEXT(call_dm), CODE_ADDRESS(call_dm));
	frame_pointer->type = SFT_DM;
}
