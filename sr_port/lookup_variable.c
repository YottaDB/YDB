/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashtab_mname.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "lookup_variable.h"

GBLREF symval *curr_symval;
GBLREF stack_frame *frame_pointer;

mval *lookup_variable(unsigned int x)
{
	ht_ent_mname	*tabent;

	assert(x < frame_pointer->vartab_len);
	if (add_hashtab_mname(&curr_symval->h_symtab, ((var_tabent *)frame_pointer->vartab_ptr + x), NULL, &tabent))
		lv_newname(tabent, curr_symval);
	return (mval *) tabent->value;
}
