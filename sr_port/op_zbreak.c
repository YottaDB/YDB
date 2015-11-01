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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "cache.h"
#include "zbreak.h"
#include "io.h"
#include "compiler.h"

#ifdef __MVS__
#define SIZEOF_LA	4
#else
#define SIZEOF_LA	0
#endif

GBLREF z_records zbrk_recs;
GBLREF stack_frame *frame_pointer;

int op_zbreak(stack_frame *fp)
{
	unsigned char	*line_addr;
	zbrk_struct	*z;

	line_addr = find_line_start(fp->mpc, fp->rvector);
	assert(line_addr);
	line_addr = (unsigned char *)find_line_call(line_addr) - SIZEOF_LA;
	z = (zbrk_struct *)zr_find(&zbrk_recs, (char *)line_addr);
	assert(z);

	if ((z->count > 0 && --z->count > 0))
		return(TRUE);
	else
	{
		flush_pio();
		comp_indr(&z->action->obj);
		frame_pointer->type = SFT_ZBRK_ACT;
		return(FALSE);
	}
}
