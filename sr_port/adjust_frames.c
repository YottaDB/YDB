/****************************************************************
 *								*
 * Copyright (c) 2002-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "arlinkdbg.h"

GBLREF stack_frame	*frame_pointer;

/* Routine used when creating a private copy of shared code so we can insert breakpoints in it or when releasing such
 * a private code copy when all breakpoints in a shared routine are removed. This routine modifies the mpc value
 * in instance of the M stack for this routine to point to the process private code on allocation or back to the
 * shared copy when the storage is removed.
 *
 * Arguments are the start/end addresses of the previous section and the address of the code section to use
 * instead.
 */
void adjust_frames(unsigned char *old_ptext_beg, unsigned char *old_ptext_end, unsigned char *new_ptext_beg)
{
	stack_frame	*fp;

	for (fp = frame_pointer; NULL != fp; fp = fp->old_frame_pointer)
	{
#		ifdef GTM_TRIGGER
		if (fp->type & SFT_TRIGR)
			/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
			fp = *(stack_frame **)(fp + 1);
		assert(fp);
#		endif
		assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
		if ((old_ptext_beg <= fp->mpc) && (fp->mpc <= old_ptext_end))
		{
			DBGARLNK((stderr, "adjust_frames: M stackframe 0x"lvaddr" resetting mpc from 0x"lvaddr" to 0x"lvaddr
				  " for private code alloc/release (new seg 0x"lvaddr", old seg 0x"lvaddr")\n", fp, fp->mpc,
				  fp->mpc + (new_ptext_beg - old_ptext_beg), old_ptext_beg, new_ptext_beg));
			fp->mpc += (new_ptext_beg - old_ptext_beg);
		}
	}
	return;
}
