/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "cache.h"
#include "zbreak.h"
#include "io.h"
#include "compiler.h"
#include "restrict.h"

GBLREF int4		gtm_trigger_depth;
GBLREF stack_frame	*frame_pointer;
GBLREF z_records	zbrk_recs;

int op_zbreak(stack_frame *fp)
{
	unsigned char	*line_addr;
	zbrk_struct	*z_ptr;

	if ((0 < gtm_trigger_depth) && (RESTRICTED(trigger_mod)))
		return (TRUE);

	line_addr = find_line_start(fp->mpc, fp->rvector);
	assert(NULL != line_addr);
	line_addr = (unsigned char *)find_line_call(line_addr) - SIZEOF_LA;
	z_ptr = zr_find(&zbrk_recs, (zb_code *)line_addr, RETURN_CLOSEST_MATCH_FALSE);
	assert(NULL != z_ptr);

	if (0 < z_ptr->count && 0 < --z_ptr->count)
		return (TRUE);
	else
	{
		flush_pio();
		comp_indr(&z_ptr->action->obj);
		frame_pointer->type = SFT_ZBRK_ACT;
		return (FALSE);
	}
}
