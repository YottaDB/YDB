/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmd_qlf.h"
#include "op.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "stringpool.h"

GBLREF	stack_frame	*frame_pointer;

void op_stolitc(mval *val)
{
#	ifdef USHBIN_SUPPORTED
	assert(DYNAMIC_LITERALS_ENABLED(frame_pointer->rvector));
	assert((char *)frame_pointer->literal_ptr == (char *)frame_pointer->rvector->literal_adr);
	assert(!MVAL_IN_RANGE(val, frame_pointer->literal_ptr, frame_pointer->rvector->literal_len));	/* val is NOT a literal */
	val->str.addr += (INTPTR_T)(frame_pointer->rvector->literal_text_adr);
#	else
	assert(FALSE);
#	endif
	return;
}
