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
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "mvalconv.h"
#include "underr.h"

GBLREF	boolean_t	in_op_fnnext;

void op_fnnext(lv_val *src, mval *key, mval *dst)
{

	assert(!in_op_fnnext);
	in_op_fnnext = TRUE;
	op_fnorder(src, key, dst);
	assert(!in_op_fnnext); /* should have been reset by op_fnorder */
	in_op_fnnext = FALSE;
}
