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
#include "op.h"
#include "unwind_nocounts.h"
#include "dm_setup.h"

GBLREF stack_frame	*frame_pointer;

void	op_hardret(void)
{
	bool		dmode;

	dmode = unwind_nocounts();
	assert(frame_pointer->old_frame_pointer);
	assert(SFT_COUNT & frame_pointer->type);

	/* QUIT in dmode should
	   + re-install SFT_DM frame that would have been unwound by unwind_nocounts
	   + not unwind the associated SFT_COUNT frame that lies below SFT_DM frame
	   to re-enter dmode and to preserve all mvals associated with level-1. */
	if (!dmode)
		op_unwind();
	else
		dm_setup();
	return;
}/* eor */
