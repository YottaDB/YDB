/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "dm_setup.h"
#include "io.h"
GBLREF	io_desc		*gtm_err_dev;

GBLREF stack_frame	*frame_pointer;

void dm_setup(void)
{
#ifdef UNIX
	/* zero the error device just to be safe */
	assert(NULL == gtm_err_dev);
	gtm_err_dev = NULL;
#endif
	new_stack_frame(frame_pointer->rvector, GTM_CONTEXT(call_dm), CODE_ADDRESS(call_dm));
	frame_pointer->type = SFT_DM;
}
