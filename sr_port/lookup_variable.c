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
#include "hashdef.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "lookup_variable.h"

GBLREF symval *curr_symval;
GBLREF stack_frame *frame_pointer;

mval *lookup_variable(unsigned int x)
{
	char new;
	ht_entry *q;

	assert(x < frame_pointer->vartab_len);
	q = ht_put(&curr_symval->h_symtab , (mname *)&(((vent *) frame_pointer->vartab_ptr)[x]) , &new);
	if (new)
	{	lv_newname(q, curr_symval);
	}
	return (mval *) q->ptr;
}
